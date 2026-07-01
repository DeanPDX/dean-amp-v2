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

        // Frequency-domain loudness normalization. Normalizing by the single
        // PEAK |H(f)| bin makes the level depend on how resonant the response
        // is: the close-mic'd (on-axis) IRs have a sharp presence peak, so
        // peak-normalizing crushed their whole passband ~20 dB below the
        // flatter off-axis IRs. Instead normalize the MEAN magnitude across
        // the guitar band (80 Hz – 6 kHz) — perceived loudness — and only use
        // the peak as a safety clamp. Both channels get the same factor so
        // the L/R balance is preserved.
        const int fftOrder = 11;            // 2048 = 1 << 11
        const int fftSize  = 1 << fftOrder; // matches kIrLengthSamples
        juce::dsp::FFT fft (fftOrder);

        auto bandStats = [&] (const float* ir)
        {
            std::vector<float> fftData ((size_t) fftSize * 2, 0.0f);
            for (int i = 0; i < kIrLengthSamples; ++i) fftData[(size_t) i] = ir[i];
            fft.performFrequencyOnlyForwardTransform (fftData.data());
            const int lo = juce::jmax (1, (int) (80.0 * fftSize / sr));
            const int hi = juce::jmin (fftSize / 2 - 1, (int) (6000.0 * fftSize / sr));
            double sum = 0.0;
            float pk = 1e-9f;
            for (int i = lo; i <= hi; ++i)
            {
                sum += fftData[(size_t) i];
                pk = std::max (pk, fftData[(size_t) i]);
            }
            return std::pair<float, float> ((float) (sum / (double) (hi - lo + 1)), pk);
        };
        const auto [meanL, pkL] = bandStats (dstL);
        const auto [meanR, pkR] = bandStats (dstR);
        const float meanMag = std::max (meanL, meanR);
        const float peakMag = std::max (pkL, pkR);

        // Target: mean magnitude == -9 dB; resonant peaks may ride above, but
        // never past unity (keeps the convolver from adding gain anywhere).
        float scale = 0.35f / meanMag;
        if (peakMag * scale > 1.0f)
            scale = 1.0f / peakMag;
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

        // Build as a direct-sound transient plus damped resonant modes. The
        // transient sets the broadband "body" of the response; each mode's
        // FREQUENCY-RESPONSE peak is pinned at an explicit ratio above it.
        // (A decaying cosine's |H| peak is ~gain × decay-samples, so tuning
        // raw mode gains by ear put the resonances 40+ dB above the mid-band
        // and the "cab" was four ringing bells with valleys between them.
        // Pinning the ratio keeps the response speaker-shaped.)
        const float fs = (float) sr;
        const float direct = offAxis ? 0.5f : 1.3f;

        // Mic position (offAxis) dramatically darkens the top: moving off the
        // speaker cap rolls off the presence peak and the cone-breakup highs.
        const float presMicMul = offAxis ? 0.45f : 1.0f;

        // Right-channel (variant 1) decorrelation — applied to the upper modes only.
        const bool  right     = (variant == 1);
        const float presFMul  = right ? 1.035f : 1.0f;
        const float hiFMul    = right ? 1.08f  : 1.0f;
        const float upperPhase= right ? juce::MathConstants<float>::halfPi : 0.0f;

        // Modes: peakRatio = |H| at the mode peak relative to the direct sound
        // (2.5 ≈ +8 dB). Long decay only on the low resonance (the "thump");
        // the upper modes use millisecond decays so they read as broad EQ
        // character, not metallic ringing.
        struct Mode { float f, peakRatio, decaySecs, phase; };
        std::array<Mode, 4> modes = {{
            { s.resHz,               2.5f,                                s.decay * 1.3f, 0.0f },
            { s.bodyHz,              1.0f + s.bodyGain * 0.20f,           0.003f,         0.0f },
            { s.presHz * presFMul,   (1.0f + s.presGain * 0.18f) * presMicMul, 0.0007f,   upperPhase },
            { (offAxis ? s.hiCut * 0.5f : s.hiCut) * hiFMul,
                                     offAxis ? 0.6f : 1.5f,               0.00035f,       upperPhase }
        }};

        // Sum decaying sinusoids, gain derived from the pinned peak ratio.
        for (auto& m : modes)
        {
            const float omega = juce::MathConstants<float>::twoPi * m.f / fs;
            const float decaySamps = juce::jmax (1.0f, m.decaySecs * fs);
            const float decay = std::exp (-1.0f / decaySamps);
            float amp = m.peakRatio * direct / decaySamps;
            for (int n = 0; n < kIrLengthSamples; ++n)
            {
                ir[(size_t) n] += amp * std::cos (omega * (float) n + m.phase);
                amp *= decay;
            }
        }

        // The direct sound (punchier on-axis).
        ir[0] += direct;

        // 1-pole low-pass that sets the overall top-end. A HIGHER coefficient is
        // brighter, so on-axis (close to the cap) is bright and off-axis is much
        // darker.
        const float lpCoef = offAxis ? 0.12f : 0.42f;
        float prev = 0.0f;
        for (int n = 0; n < kIrLengthSamples; ++n)
        {
            prev = prev + lpCoef * (ir[(size_t) n] - prev);
            ir[(size_t) n] = prev;
        }

        // Closed-back cab low cut: 2nd-order HP at 70 Hz keeps the bottom
        // tight (the resonance mode supplies the thump, not sub energy).
        {
            juce::dsp::IIR::Filter<float> hp;
            hp.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 70.0f, 0.9f);
            for (int n = 0; n < kIrLengthSamples; ++n)
                ir[(size_t) n] = hp.processSample (ir[(size_t) n]);
        }

        // Right-channel decorrelation, part 2: rotate the phase of the mids
        // and highs with a pair of first-order allpasses (a dual-mic'd cab is
        // phase-aligned in the lows but drifts apart up top). |H| is unity,
        // so the right channel's loudness and tone still match the left.
        if (right)
        {
            // Single pass, corner up at 2.2 kHz: ±90° around the corner and
            // more above, but the low/mid power band stays phase-aligned
            // (mono-safe; L/R correlation lands near a real dual-mic pair).
            const float t = std::tan (juce::MathConstants<float>::pi * 2200.0f / fs);
            const float a = (1.0f - t) / (1.0f + t);
            float x1 = 0.0f, y1 = 0.0f;
            for (int n = 0; n < kIrLengthSamples; ++n)
            {
                const float x0 = ir[(size_t) n];
                const float y0 = -a * x0 + x1 + a * y1;
                ir[(size_t) n] = y0;
                x1 = x0; y1 = y0;
            }
        }

        // Fade the tail so the low resonance doesn't truncate audibly.
        constexpr int fadeLen = 512;
        for (int n = 0; n < fadeLen; ++n)
        {
            const float t = (float) n / (float) fadeLen;
            ir[(size_t) (kIrLengthSamples - fadeLen + n)] *= 0.5f * (1.0f + std::cos (juce::MathConstants<float>::pi * t));
        }

        return ir;
    }

    double sr { 0.0 };
    juce::dsp::ProcessSpec spec_ {};
    juce::dsp::Convolution convolution;

    // [cab][axis: 0=on,1=off][variant: 0=left,1=right]
    std::array<std::array<std::array<std::array<float, kIrLengthSamples>, 2>, 2>, kNumCabs> irs {};
    int cabIndex { 1 };      // default V30
    float micBlend { 0.0f }; // matches the Mic parameter default (Center) so a
                             // fresh instance doesn't reload/crossfade the IR
                             // on its first block
    bool bypass { false };
};

} // namespace deanamp
