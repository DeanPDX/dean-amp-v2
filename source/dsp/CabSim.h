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
            for (size_t v = 0; v < 2; ++v) // 0 = on-axis, 1 = off-axis
                irs[c][v] = makeIR ((int) c, v == 1);

        convolution.reset();
        convolution.prepare (spec);
        loadActiveIR();
    }

    void setCabIndex (int idx)
    {
        cabIndex = juce::jlimit (0, kNumCabs - 1, idx);
        loadActiveIR();
    }

    void setMicBlend (float blend01)
    {
        const float clamped = juce::jlimit (0.0f, 1.0f, blend01);
        if (std::abs (clamped - micBlend) < 0.01f) return; // dedupe
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
        // Crossfade the on-axis and off-axis IRs into the convolver
        juce::AudioBuffer<float> blend (1, kIrLengthSamples);
        auto* dst = blend.getWritePointer (0);
        const auto& a = irs[(size_t) cabIndex][0];
        const auto& b = irs[(size_t) cabIndex][1];
        for (size_t i = 0; i < (size_t) kIrLengthSamples; ++i)
            dst[i] = a[i] * (1.0f - micBlend) + b[i] * micBlend;

        // Frequency-domain peak normalization. A peak-normalized time-domain
        // IR can still have a +15 dB peak in its frequency response if it has
        // strong resonant modes — exactly what happened with our parametric IRs.
        // Compute |H(f)| via FFT and scale so the peak magnitude is 0 dB.
        const int fftOrder = 11;            // 2048 = 1 << 11
        const int fftSize  = 1 << fftOrder; // matches kIrLengthSamples
        juce::dsp::FFT fft (fftOrder);

        std::vector<float> fftData ((size_t) fftSize * 2, 0.0f);
        for (int i = 0; i < kIrLengthSamples; ++i)
            fftData[(size_t) i] = dst[i];

        fft.performFrequencyOnlyForwardTransform (fftData.data());

        float maxMag = 1e-9f;
        for (int i = 0; i < fftSize / 2; ++i)
            maxMag = std::max (maxMag, fftData[(size_t) i]);

        // Target: peak magnitude == -3 dB (leave a little headroom)
        const float target = 0.707f;
        const float scale  = target / maxMag;
        for (int i = 0; i < kIrLengthSamples; ++i) dst[i] *= scale;

        convolution.loadImpulseResponse (
            std::move (blend),
            sr,
            juce::dsp::Convolution::Stereo::no,
            juce::dsp::Convolution::Trim::no,
            juce::dsp::Convolution::Normalise::no);
    }

    // Generate a parametric IR for the given cab
    std::array<float, kIrLengthSamples> makeIR (int cabId, bool offAxis) const
    {
        std::array<float, kIrLengthSamples> ir {};

        // Per-cab parameters: [low resonance Hz, low Q, presence Hz, presence Q, presence gain,
        //                     high cut Hz, body bump Hz, body gain, decay seconds]
        struct CabSpec { float resHz, resQ, presHz, presGain, hiCut, bodyHz, bodyGain, decay; };
        static const CabSpec specs[kNumCabs] = {
            //  resHz  resQ  presHz presG hiCut  bodyHz bodyG  decay
            {   95.0f, 5.0f, 1800.f,  4.f, 4000.f, 450.f,  3.f, 0.020f }, // 4x12 Greenback
            {  110.0f, 6.0f, 2400.f,  6.f, 5200.f, 800.f,  4.f, 0.022f }, // 4x12 V30
            {   80.0f, 4.0f, 3200.f,  3.f, 5500.f, 350.f,  2.f, 0.018f }, // 2x12 American
            {  120.0f, 5.5f, 1500.f,  5.f, 3500.f, 600.f,  4.f, 0.025f }, // 1x12 Tweed
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
        struct Mode { float f, q, g, decayMul; };
        std::array<Mode, 4> modes = {{
            { s.resHz,   s.resQ * 0.5f,  1.0f,           1.4f },
            { s.bodyHz,  3.5f,            s.bodyGain * 0.4f, 1.0f },
            { s.presHz,  4.0f,            s.presGain * 0.35f, 0.7f },
            { offAxis ? s.hiCut * 0.7f : s.hiCut, 1.2f, offAxis ? 0.5f : 0.8f, 0.4f }
        }};

        // Sum decaying sinusoids
        for (auto& m : modes)
        {
            const float omega = juce::MathConstants<float>::twoPi * m.f / fs;
            const float decay = std::exp (-1.0f / (decaySamps * m.decayMul));
            float amp = m.g;
            for (int n = 0; n < kIrLengthSamples; ++n)
            {
                ir[(size_t) n] += amp * std::cos (omega * (float) n);
                amp *= decay;
            }
        }

        // Add an initial transient (the direct sound)
        ir[0] += offAxis ? 0.8f : 1.2f;

        // Quick smoothing 1-pole low-pass to round off zipper highs
        const float lpCoef = offAxis ? 0.35f : 0.20f;
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

    std::array<std::array<std::array<float, kIrLengthSamples>, 2>, kNumCabs> irs {};
    int cabIndex { 1 };       // default V30
    float micBlend { 0.35f }; // a touch off-axis sounds better as default
    bool bypass { false };
};

} // namespace deanamp
