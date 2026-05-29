#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>

namespace deanamp
{

/**
 * Built-in cab sim. Rather than shipping WAV files, we synthesize 4 short
 * impulse responses (~2048 taps @ 48k) from a parametric speaker model and
 * convolve via JUCE's partitioned convolution. The IRs capture the broad
 * character of each cab type — main resonance, beam-blocker dip, edge bloom,
 * high-cut from the cone breakup.
 *
 * Cabs:
 *   0: 4x12 Greenback (G12M-25 vibe)   — warm, woody mids, soft top
 *   1: 4x12 V30       (G12 Vintage 30) — aggressive mids, present top
 *   2: 2x12 American  (Jensen-ish)     — open, bright, scooped
 *   3: 1x12 Tweed     (Alnico-ish)     — round, midrangey, vintage roll-off
 *
 * "Mic blend" crossfades the selected cab between a close-mic'd (bright, focused)
 * variant and a slightly-off-axis (darker, more body) variant.
 */
class CabSim
{
public:
    static constexpr int kIrLengthSamples = 2048;
    static constexpr int kNumCabs = 4;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sr = spec.sampleRate;
        spec_ = spec;

        for (size_t c = 0; c < (size_t) kNumCabs; ++c)
            for (size_t axis = 0; axis < 2; ++axis)       // 0 = on-axis, 1 = off-axis
                for (size_t var = 0; var < 2; ++var)      // 0 = left, 1 = right
                    irs[c][axis][var] = makeIR ((int) c, axis == 1, (int) var);

        convolution.reset();
        convolution.prepare (spec);
        loadActiveIR();
    }

    void setCabIndex (int idx)
    {
        const int clamped = juce::jlimit (0, kNumCabs - 1, idx);
        // Only rebuild the convolution when the selection actually changes.
        // Reloading every block hammers the convolver's async loader and makes
        // live cab switching unreliable (and wastes a lot of CPU).
        if (clamped == cabIndex) return;
        cabIndex = clamped;
        loadActiveIR();
    }

    void setMicBlend (float blend01)
    {
        const float clamped = juce::jlimit (0.0f, 1.0f, blend01);
        if (std::abs (clamped - micBlend) < 0.005f) return; // dedupe
        micBlend = clamped;
        loadActiveIR();
    }

    void setBypass (bool b) { bypass = b; }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        if (bypass) return;
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        convolution.process (ctx);
    }

    void reset() { convolution.reset(); }

private:
    void loadActiveIR()
    {
        // Build a 2-channel IR: L = left variant, R = right variant, each a
        // crossfade of the on-axis and off-axis IRs by the mic blend.
        juce::AudioBuffer<float> blend (2, kIrLengthSamples);
        auto* dstL = blend.getWritePointer (0);
        auto* dstR = blend.getWritePointer (1);
        const auto& onL  = irs[(size_t) cabIndex][0][0];
        const auto& offL = irs[(size_t) cabIndex][1][0];
        const auto& onR  = irs[(size_t) cabIndex][0][1];
        const auto& offR = irs[(size_t) cabIndex][1][1];
        for (size_t i = 0; i < (size_t) kIrLengthSamples; ++i)
        {
            dstL[i] = onL[i] * (1.0f - micBlend) + offL[i] * micBlend;
            dstR[i] = onR[i] * (1.0f - micBlend) + offR[i] * micBlend;
        }

        // Frequency-domain peak normalization. A peak-normalized time-domain
        // IR can still have a +15 dB peak in its frequency response if it has
        // strong resonant modes. Compute |H(f)| via FFT for both channels and
        // scale BOTH by the same factor so the L/R balance is preserved.
        const int fftOrder = 11;            // 2048 = 1 << 11
        const int fftSize  = 1 << fftOrder; // matches kIrLengthSamples
        juce::dsp::FFT fft (fftOrder);

        auto peakMag = [&] (const float* ir)
        {
            std::vector<float> fftData ((size_t) fftSize * 2, 0.0f);
            for (int i = 0; i < kIrLengthSamples; ++i) fftData[(size_t) i] = ir[i];
            fft.performFrequencyOnlyForwardTransform (fftData.data());
            float m = 1e-9f;
            for (int i = 0; i < fftSize / 2; ++i) m = std::max (m, fftData[(size_t) i]);
            return m;
        };
        const float maxMag = std::max (peakMag (dstL), peakMag (dstR));

        // Target: peak magnitude == -3 dB (leave a little headroom)
        const float scale = 0.707f / maxMag;
        for (int i = 0; i < kIrLengthSamples; ++i) { dstL[i] *= scale; dstR[i] *= scale; }

        convolution.loadImpulseResponse (
            std::move (blend),
            sr,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::no,
            juce::dsp::Convolution::Normalise::no);
    }

    // Generate a parametric IR for the given cab.
    //  variant 0 = left channel, variant 1 = right channel. The right variant
    //  shifts the frequency and phase of ONLY the upper modes (presence + cone
    //  breakup), leaving the low/low-mid modes and the direct transient identical.
    //  That decorrelates the top end for a wide stereo image while keeping the
    //  bass mono-compatible (no comb-filtering / phase cancellation when summed).
    std::array<float, kIrLengthSamples> makeIR (int cabId, bool offAxis, int variant) const
    {
        std::array<float, kIrLengthSamples> ir {};

        // Per-cab parameters: [low resonance Hz, low Q, presence Hz, presence Q, presence gain,
        //                     high cut Hz, body bump Hz, body gain, decay seconds]
        struct CabSpec { float resHz, resQ, presHz, presGain, hiCut, bodyHz, bodyGain, decay; };
        // Spread well apart so switching is obviously audible — hiCut (top-end
        // rolloff) is the most recognisable difference between real cabs.
        static const CabSpec specs[kNumCabs] = {
            //  resHz  resQ  presHz presG hiCut  bodyHz bodyG  decay
            {   95.0f, 5.0f, 1650.f,  3.f, 3600.f, 480.f,  5.f, 0.024f }, // 4x12 Greenback — dark, woody, soft top
            {  110.0f, 6.0f, 2700.f,  8.f, 5600.f, 820.f,  4.f, 0.020f }, // 4x12 V30       — aggressive upper-mid, present
            {   78.0f, 4.0f, 3700.f,  4.f, 7000.f, 300.f,  1.f, 0.015f }, // 2x12 American  — scooped, open, bright
            {  128.0f, 6.0f, 1250.f,  6.f, 2900.f, 600.f,  6.f, 0.030f }, // 1x12 Tweed     — boxy, midrangey, very dark top
        };
        const auto& s = specs[cabId];

        // Build via inverse-FFT-free approach: an initial impulse followed by a
        // damped sum of resonant modes. This sounds more "cab-like" than a pure
        // IIR steady-state response would.
        const float fs = (float) sr;
        const float decaySamps = s.decay * fs;

        // Mode 1: chassis resonance (low)
        // Mode 2: speaker body (low-mid)
        // Mode 3: presence peak (mid-high)
        // Mode 4: cone breakup (high) — broader, faster decay
        // Mic position (offAxis) dramatically darkens the top: moving off the
        // speaker cap rolls off the presence peak and the cone-breakup highs.
        const float presMicMul = offAxis ? 0.45f : 1.0f;

        // Right-channel (variant 1) decorrelation — applied to the upper modes only.
        const bool  right     = (variant == 1);
        const float presFMul  = right ? 1.035f : 1.0f;
        const float hiFMul    = right ? 1.08f  : 1.0f;
        const float upperPhase= right ? juce::MathConstants<float>::halfPi : 0.0f;

        struct Mode { float f, q, g, decayMul, phase; };
        std::array<Mode, 4> modes = {{
            { s.resHz,   s.resQ * 0.5f,  1.0f,                            1.4f, 0.0f },
            { s.bodyHz,  3.5f,            s.bodyGain * 0.4f,              1.0f, 0.0f },
            { s.presHz * presFMul, 4.0f,  s.presGain * 0.35f * presMicMul, 0.7f, upperPhase },
            { (offAxis ? s.hiCut * 0.5f : s.hiCut) * hiFMul, 1.2f, offAxis ? 0.22f : 0.9f, 0.4f, upperPhase }
        }};

        // Sum decaying sinusoids
        for (auto& m : modes)
        {
            const float omega = juce::MathConstants<float>::twoPi * m.f / fs;
            const float decay = std::exp (-1.0f / (decaySamps * m.decayMul));
            float amp = m.g;
            for (int n = 0; n < kIrLengthSamples; ++n)
            {
                ir[(size_t) n] += amp * std::cos (omega * (float) n + m.phase);
                amp *= decay;
            }
        }

        // Add an initial transient (the direct sound) — punchier on-axis.
        ir[0] += offAxis ? 0.5f : 1.3f;

        // 1-pole low-pass that sets the overall top-end. A HIGHER coefficient is
        // brighter, so on-axis (close to the cap) is bright and off-axis is much
        // darker. (The previous values had this backwards, making mic position
        // barely audible.)
        const float lpCoef = offAxis ? 0.12f : 0.42f;
        float prev = 0.0f;
        for (int n = 0; n < kIrLengthSamples; ++n)
        {
            prev = prev + lpCoef * (ir[(size_t) n] - prev);
            ir[(size_t) n] = prev;
        }

        return ir;
    }

    double sr { 0.0 };
    juce::dsp::ProcessSpec spec_ {};
    juce::dsp::Convolution convolution;

    // [cab][axis: 0=on,1=off][variant: 0=left,1=right]
    std::array<std::array<std::array<std::array<float, kIrLengthSamples>, 2>, 2>, kNumCabs> irs {};
    int cabIndex { 1 };       // default V30
    float micBlend { 0.35f }; // a touch off-axis sounds better as default
    bool bypass { false };
};

} // namespace deanamp
