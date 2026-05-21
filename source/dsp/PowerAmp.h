#pragma once
#include <juce_dsp/juce_dsp.h>

namespace deanamp
{

/**
 * Class-AB push-pull power amp model.
 *
 *  - Symmetric soft clip (push-pull cancels even harmonics in the output transformer)
 *  - Sag: envelope-controlled gain droop simulating B+ rail dip under load
 *  - Presence: HF shelf in the negative feedback loop — modeled as a post-stage HF tilt
 *  - Resonance: LF resonance peak from the speaker/output-transformer interaction
 *  - Output transformer HF rolloff
 */
class PowerAmp
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sr = spec.sampleRate;

        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            spec.numChannels, 2 /* 4x */,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        oversampler->initProcessing (spec.maximumBlockSize);

        outputHP.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 60.0f);
        outputLP.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sr, 7500.0f);
        outputHP.reset();
        outputLP.reset();

        updateTone();
        envState = 0.0f;
    }

    int getLatencySamples() const { return oversampler ? (int) oversampler->getLatencyInSamples() : 0; }

    void setPresence  (float v01) { presence  = v01; updateTone(); }
    void setResonance (float v01) { resonance = v01; updateTone(); }
    void setMasterDrive (float v01) { masterDrive = v01; } // power-tube drive amount (sag + saturation)

    void process (juce::dsp::AudioBlock<float>& block)
    {
        // Envelope follower for sag (uses pre-saturation signal RMS)
        const float attack  = 0.005f;  // fast
        const float release = 0.0008f; // slow
        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            float s = 0.0f;
            for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
                s += std::abs (block.getChannelPointer (ch)[i]);
            s /= (float) std::max (size_t (1), block.getNumChannels());
            const float coef = s > envState ? attack : release;
            envState += coef * (s - envState);
        }
        // Sag: high envelope reduces gain a touch (rail droop)
        const float sag = 1.0f - juce::jlimit (0.0f, 0.25f, envState * 0.6f * masterDrive);

        auto osBlock = oversampler->processSamplesUp (block);

        const float drive = 1.0f + 2.5f * masterDrive;

        for (size_t ch = 0; ch < osBlock.getNumChannels(); ++ch)
        {
            auto* d = osBlock.getChannelPointer (ch);
            const size_t n = osBlock.getNumSamples();
            for (size_t i = 0; i < n; ++i)
            {
                float x = d[i] * drive * sag;
                // Push-pull symmetric soft clip (power tubes clip more abruptly than
                // preamp triodes — a sharper tanh approximates 6L6/EL34 character).
                // tanh keeps the output bounded to ±1.0 regardless of input — no need
                // for a follow-up hard limit, which would alias and sound ugly.
                x = std::tanh (x * 0.9f);
                d[i] = x * 0.55f;
            }
        }

        oversampler->processSamplesDown (block);

        // Output transformer / cab pre-shaping
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto* d = block.getChannelPointer (ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                float v = d[i];
                v = resoFilter.processSample (v);
                v = presenceFilter.processSample (v);
                v = outputHP.processSample (v);
                v = outputLP.processSample (v);
                d[i] = v;
            }
        }
    }

    void reset()
    {
        if (oversampler) oversampler->reset();
        outputHP.reset(); outputLP.reset();
        resoFilter.reset(); presenceFilter.reset();
        envState = 0.0f;
    }

private:
    void updateTone()
    {
        if (sr <= 0.0) return;
        const float resoDb = juce::jmap (resonance, 0.0f, 1.0f, -2.0f, 8.0f);
        const float presDb = juce::jmap (presence,  0.0f, 1.0f, -4.0f, 8.0f);

        resoFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sr, 110.0f, 1.4f, juce::Decibels::decibelsToGain (resoDb));
        presenceFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sr, 3500.0f, 0.7f, juce::Decibels::decibelsToGain (presDb));
    }

    double sr { 0.0 };
    float presence { 0.5f }, resonance { 0.5f }, masterDrive { 0.5f };
    float envState { 0.0f };

    juce::dsp::IIR::Filter<float> outputHP, outputLP, resoFilter, presenceFilter;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
};

} // namespace deanamp
