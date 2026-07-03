#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

namespace deanamp
{

/**
 * Spring reverb tank (three-spring, Accutronics flavour).
 *
 * Sits post-cab: the wet return can be stereo, the tail rides on the finished
 * amp tone instead of being re-saturated by the power amp, and the noise gate
 * (which lives at the front of the chain) can never chop it off.
 *
 * Signal path — mono send, stereo return:
 *
 *   send   = band-limit (a tank's transducers only carry ~180 Hz..4 kHz)
 *            → soft tanh (input transducer overload)
 *            → allpass cascade (dispersion — see below)
 *   tank   = three feedback delay loops with unequal lengths so their echo
 *            patterns interleave instead of comb-stacking. Each loop carries
 *            a damping lowpass (every bounce gets darker) and its own short
 *            allpass cascade (every bounce gets *more* dispersed).
 *   return = the three springs mixed with mirrored L/R weights — decorrelated
 *            width without losing mono compatibility.
 *
 * The dispersion is what makes a spring a spring: waves of different
 * frequencies travel a coil at different speeds, so a transient smears into
 * the characteristic "boing" chirp. A cascade of first-order allpasses has
 * exactly that property (group delay rising with frequency), and because each
 * loop recirculates through its own cascade, later echoes chirp progressively
 * more — the classic drip.
 *
 * The DEPTH knob is a pure wet level, like the reverb pot on an amp; the
 * decay (~2 s) is a property of the "tank" and stays fixed.
 */
class SpringReverb
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sr = spec.sampleRate;

        inputHP.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 180.0f);
        inputLP.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sr, 4200.0f);

        for (auto& ap : inputChirp) ap.a = 0.62f;

        for (size_t s = 0; s < kSprings; ++s)
        {
            auto& sp = springs[s];
            sp.delaySamples = (float) (kDelayMs[s] * 0.001 * sr);

            // Loop gain for a fixed T60: g = 10^(-3·D/T60) per trip round the loop.
            sp.feedback = std::pow (10.0f, -3.0f * (float) (kDelayMs[s] * 0.001) / kDecaySeconds);

            // One-pole damping inside the loop — every bounce loses highs, as
            // it does in steel.
            sp.dampCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 2600.0f / (float) sr);

            for (auto& ap : sp.loopChirp) ap.a = kLoopChirpA[s];

            sp.lfoInc = juce::MathConstants<float>::twoPi * kLfoHz[s] / (float) sr;

            // Power-of-two buffer big enough for the delay + modulation swing.
            const int need = (int) sp.delaySamples + 8;
            int size = 1;
            while (size < need) size <<= 1;
            sp.buf.assign ((size_t) size, 0.0f);
            sp.mask = size - 1;
        }

        modDepth = (float) (sr / 48000.0); // ±1 sample @48k, scaled to the rate

        wetGain.reset (sr, 0.05);
        reset();
    }

    /** DEPTH knob 0..1 → wet level (audio-taper so the lower half is usable). */
    void setAmount (float v01)
    {
        wetGain.setTargetValue (std::pow (juce::jlimit (0.0f, 1.0f, v01), 1.5f) * kWetMax);
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        auto* L = block.getChannelPointer (0);
        auto* R = block.getNumChannels() > 1 ? block.getChannelPointer (1) : nullptr;
        const size_t n = block.getNumSamples();

        for (size_t i = 0; i < n; ++i)
        {
            const float wet = wetGain.getNextValue();

            float send = R != nullptr ? 0.5f * (L[i] + R[i]) : L[i];
            send = inputLP.processSample (inputHP.processSample (send));
            send = std::tanh (1.5f * send) * (1.0f / 1.5f);
            for (auto& ap : inputChirp) send = ap.process (send);

            float wetL = 0.0f, wetR = 0.0f;
            for (size_t s = 0; s < kSprings; ++s)
            {
                auto& sp = springs[s];

                // A whisker of delay modulation (±1 sample, sub-Hz) keeps the
                // fixed loops from ringing metallically on sustained notes.
                sp.lfoPhase += sp.lfoInc;
                if (sp.lfoPhase > juce::MathConstants<float>::twoPi)
                    sp.lfoPhase -= juce::MathConstants<float>::twoPi;

                const float d = sp.delaySamples + modDepth * std::sin (sp.lfoPhase);
                float rp = (float) sp.writePos - d;
                if (rp < 0.0f) rp += (float) (sp.mask + 1);
                const int   i0   = (int) rp;
                const float frac = rp - (float) i0;
                const float y0   = sp.buf[(size_t) (i0 & sp.mask)];
                const float y1   = sp.buf[(size_t) ((i0 + 1) & sp.mask)];
                float c = y0 + frac * (y1 - y0);

                sp.dampState += sp.dampCoef * (c - sp.dampState);
                c = sp.dampState;
                for (auto& ap : sp.loopChirp) c = ap.process (c);

                sp.buf[(size_t) sp.writePos] = send + sp.feedback * c;
                sp.writePos = (sp.writePos + 1) & sp.mask;

                wetL += kMixL[s] * c;
                wetR += kMixR[s] * c;
            }

            if (R != nullptr) { L[i] += wet * wetL;  R[i] += wet * wetR; }
            else              { L[i] += wet * 0.5f * (wetL + wetR); }
        }
    }

    void reset()
    {
        inputHP.reset();
        inputLP.reset();
        for (auto& ap : inputChirp) ap.reset();
        for (size_t s = 0; s < kSprings; ++s)
        {
            auto& sp = springs[s];
            std::fill (sp.buf.begin(), sp.buf.end(), 0.0f);
            sp.writePos  = 0;
            sp.dampState = 0.0f;
            sp.lfoPhase  = kLfoPhase0[s];
            for (auto& ap : sp.loopChirp) ap.reset();
        }
        wetGain.setCurrentAndTargetValue (wetGain.getTargetValue());
    }

private:
    // First-order allpass: unity gain everywhere, frequency-dependent phase —
    // the building block of the chirp cascades.
    struct Allpass1
    {
        float a { 0.0f }, x1 { 0.0f }, y1 { 0.0f };
        float process (float x) { const float y = a * (x - y1) + x1; x1 = x; y1 = y; return y; }
        void  reset()           { x1 = y1 = 0.0f; }
    };

    static constexpr size_t kSprings      = 3;
    static constexpr float  kDecaySeconds = 2.0f;
    static constexpr float  kWetMax       = 1.0f;

    struct Spring
    {
        std::vector<float> buf;
        int   writePos { 0 }, mask { 0 };
        float delaySamples { 0.0f }, feedback { 0.0f };
        float dampCoef { 0.0f }, dampState { 0.0f };
        float lfoPhase { 0.0f }, lfoInc { 0.0f };
        std::array<Allpass1, 8> loopChirp;
    };

    // Unequal lengths (non-integer ratios) so the echo patterns interleave;
    // slightly different dispersion per spring decorrelates the returns.
    static constexpr double kDelayMs[kSprings]    = { 33.7, 41.3, 48.9 };
    static constexpr float  kLoopChirpA[kSprings] = { 0.55f, 0.60f, 0.65f };
    static constexpr float  kLfoHz[kSprings]      = { 0.41f, 0.67f, 0.93f };
    static constexpr float  kLfoPhase0[kSprings]  = { 0.0f, 2.1f, 4.2f };
    // Mirrored pan weights: each side sums to 1, so the image is wide but the
    // mono fold-down keeps all three springs at full level.
    static constexpr float  kMixL[kSprings]       = { 0.50f, 0.33f, 0.17f };
    static constexpr float  kMixR[kSprings]       = { 0.17f, 0.33f, 0.50f };

    double sr { 0.0 };
    float  modDepth { 1.0f };

    juce::dsp::IIR::Filter<float> inputHP, inputLP;
    std::array<Allpass1, 32>      inputChirp;
    std::array<Spring, kSprings>  springs;
    juce::SmoothedValue<float>    wetGain;
};

} // namespace deanamp
