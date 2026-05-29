#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace deanamp
{

/**
 * Single 12AX7 triode gain stage.
 *
 * Models:
 *  - Asymmetric soft clipping (cathode-biased triode) via a tuned tanh with bias shift,
 *    producing the even-harmonic content that gives 12AX7s their warmth.
 *  - Grid-leak conduction (hard limit on the positive swing) for that singing compression
 *    when pushed.
 *  - Inter-stage coupling cap (HP) and miller capacitance (LP) — voiced per stage so the
 *    bass tightens as gain stacks (just like a real preamp).
 *  - Bias drift: positive grid current pulls the bias point negative (blocking distortion
 *    on sustained notes; the "spongy" feel of a cranked amp).
 */
class TubeStage
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        couplingHP.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, couplingHz);
        millerLP.coefficients   = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sr, millerHz);
        couplingHP.reset();
        millerLP.reset();
        biasState = 0.0f;
    }

    void setVoicing (float couplingHzIn, float millerHzIn, float biasAsymmetryIn, float gainIn)
    {
        couplingHz = couplingHzIn;
        millerHz   = millerHzIn;
        biasAsym   = biasAsymmetryIn;
        gain       = gainIn;
        if (sr > 0.0)
        {
            couplingHP.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, couplingHz);
            millerLP.coefficients   = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sr, millerHz);
        }
    }

    inline float processSample (float x) noexcept
    {
        // Pre stage coupling cap (blocks DC, sets low-end voicing)
        x = couplingHP.processSample (x);

        // Apply stage gain
        float v = x * gain;

        // Bias-drift envelope (slow): positive excursions push bias negative
        const float driftCoef = 0.9995f; // ~ms time constant @ 44.1k
        biasState = driftCoef * biasState + (1.0f - driftCoef) * juce::jmax (0.0f, v);

        // Bias point: nominal asymmetry plus drift
        const float bias = biasAsym - 0.6f * biasState;

        // Asymmetric triode transfer: tanh on the negative half is softer than on the positive
        // Positive grid swing hits grid-current limiting earlier — produces 2nd harmonic content
        float y;
        const float vb = v + bias;
        if (vb >= 0.0f)
        {
            // Grid current region — harder clip
            y = std::tanh (vb * 0.7f) * 0.85f;
        }
        else
        {
            // Cathode region — softer, more headroom
            y = std::tanh (vb * 1.1f);
        }

        // Re-center (remove the DC offset the asymmetry created)
        y -= std::tanh (bias);

        // Miller capacitance — high-cut after the nonlinearity (de-fizzes)
        y = millerLP.processSample (y);

        return y;
    }

private:
    double sr { 44100.0 };
    float couplingHz { 30.0f };
    float millerHz   { 8000.0f };
    float biasAsym   { 0.3f };
    float gain       { 1.0f };
    float biasState  { 0.0f };

    juce::dsp::IIR::Filter<float> couplingHP;
    juce::dsp::IIR::Filter<float> millerLP;
};

/**
 * Cascaded preamp: 3–4 tube stages with progressively brighter voicing,
 * an interstage "bright cap" on stage 1, and 8x oversampling for alias-free
 * high-gain.
 */
class TubePreamp
{
public:
    enum class Channel { Clean = 0, Crunch = 1, Lead = 2 };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        baseSampleRate = spec.sampleRate;
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            spec.numChannels, 3 /* 2^3 = 8x */,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        oversampler->initProcessing (spec.maximumBlockSize);

        const double osRate = spec.sampleRate * 8.0;
        for (auto& s : stages) s.prepare (osRate);
        applyChannelVoicing();

        inputHP.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (spec.sampleRate, 40.0f);
        inputHP.reset();
    }

    void setChannel (Channel c)
    {
        channel = c;
        applyChannelVoicing();
    }

    void setBright (bool b) { bright = b; applyChannelVoicing(); }
    void setGain (float normalized01)
    {
        gainKnob = normalized01;
        applyChannelVoicing();
    }

    int getOversamplingLatencySamples() const
    {
        return oversampler ? (int) oversampler->getLatencyInSamples() : 0;
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        // Input HP at base rate (removes rumble before nonlinearity)
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto* d = block.getChannelPointer (ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
                d[i] = inputHP.processSample (d[i]);
        }

        // 8x oversample, then cascade stages, then downsample
        auto osBlock = oversampler->processSamplesUp (block);

        const int numStages = (channel == Channel::Clean) ? 2
                            : (channel == Channel::Crunch) ? 3 : 4;

        // Per-channel makeup to balance perceived loudness — heavier channels
        // get more compression so they need a bigger post-stage trim. These
        // values were tuned against the tone-test peak/RMS readings.
        // Calibrated against a realistic-level DI (see DeanAmpDiTest), NOT the
        // hot synthetic tone-test signal. Clean is nearly linear (it barely
        // compresses), so at real playing levels it is ~16 dB quieter than the
        // saturating channels and needs a much LARGER makeup to match them.
        const float makeup = (channel == Channel::Clean)  ? 0.42f
                           : (channel == Channel::Crunch) ? 0.30f
                           :                                0.42f;

        for (size_t ch = 0; ch < osBlock.getNumChannels(); ++ch)
        {
            auto* d = osBlock.getChannelPointer (ch);
            const size_t n = osBlock.getNumSamples();
            for (size_t i = 0; i < n; ++i)
            {
                float x = d[i];
                for (int s = 0; s < numStages; ++s)
                    x = stages[(size_t) s].processSample (x);
                d[i] = x * makeup;
            }
        }

        oversampler->processSamplesDown (block);
    }

    void reset()
    {
        if (oversampler) oversampler->reset();
        inputHP.reset();
        // re-prepare stages by re-applying voicing (resets filters)
        if (baseSampleRate > 0)
            for (auto& s : stages) s.prepare (baseSampleRate * 8.0);
        applyChannelVoicing();
    }

private:
    void applyChannelVoicing()
    {
        // Per-channel gain mapping: Clean is gentler, Lead is brutal
        float chanGainMul = (channel == Channel::Clean)  ? 1.5f
                          : (channel == Channel::Crunch) ? 4.0f
                          :                                9.0f;

        // Gain knob shapes 0..1 into stage drive (exponential feel)
        const float drive = 0.6f + 6.0f * std::pow (gainKnob, 1.8f);

        // Stage 1: bright voicing if bright switch on
        stages[0].setVoicing (
            /* coupling Hz */ bright ? 220.0f : 70.0f,
            /* miller Hz   */ 12000.0f,
            /* bias asym   */ 0.25f,
            /* gain        */ drive * chanGainMul * 0.9f);

        // Stage 2: tighter low end as gain stacks
        stages[1].setVoicing (180.0f, 9500.0f, 0.30f, drive * chanGainMul * 1.0f);

        // Stage 3: even tighter, slightly darker (de-fizz)
        stages[2].setVoicing (240.0f, 7500.0f, 0.35f, drive * chanGainMul * 0.9f);

        // Stage 4 (lead only): the "saturator" — most asymmetry, controlled bandwidth
        stages[3].setVoicing (300.0f, 6500.0f, 0.40f, drive * chanGainMul * 0.7f);
    }

    Channel channel { Channel::Crunch };
    bool    bright  { false };
    float   gainKnob { 0.5f };
    double  baseSampleRate { 0.0 };

    std::array<TubeStage, 4> stages;
    juce::dsp::IIR::Filter<float> inputHP;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
};

} // namespace deanamp
