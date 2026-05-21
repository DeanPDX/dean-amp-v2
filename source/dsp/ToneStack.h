#pragma once
#include <juce_dsp/juce_dsp.h>

namespace deanamp
{

/**
 * Classic passive Bass/Mid/Treble tone stack, modeled as 4 biquad sections that
 * approximate the FMV (Fender/Marshall/Vox) network response.
 *
 * Rather than literally solving the analog RC mesh per sample (expensive), this
 * uses a perceptually-tuned cascade:
 *   - LF shelf (Bass)
 *   - Bandpass peak (Mid)
 *   - HF shelf (Treble)
 *   - Fixed "stack" voicing notch around 500 Hz (the famous mid-scoop of these
 *     networks even with all knobs at noon)
 *
 * This gets you 95% of the feel for 5% of the CPU, and it's stable.
 */
class ToneStack
{
public:
    enum class Voicing { Marshall, Fender };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto* f : { &bass, &mid, &treble, &scoop })
            f->reset();
        updateCoefficients();
    }

    void setVoicing (Voicing v) { voicing = v; updateCoefficients(); }
    void setBass    (float v01) { bassKnob = v01;   updateCoefficients(); }
    void setMid     (float v01) { midKnob  = v01;   updateCoefficients(); }
    void setTreble  (float v01) { trebKnob = v01;   updateCoefficients(); }

    inline float processSample (float x) noexcept
    {
        x = bass.processSample (x);
        x = mid.processSample (x);
        x = treble.processSample (x);
        x = scoop.processSample (x);
        return x;
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto* d = block.getChannelPointer (ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
                d[i] = processSample (d[i]);
        }
    }

private:
    void updateCoefficients()
    {
        if (sr <= 0.0) return;

        const bool marshall = (voicing == Voicing::Marshall);

        // BASS: low shelf — Marshall has a tighter knee, Fender bigger and lower
        const float bassFreq = marshall ? 120.0f : 90.0f;
        const float bassGain = juce::jmap (bassKnob, 0.0f, 1.0f, -12.0f, 12.0f);
        bass.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
            sr, bassFreq, 0.7f, juce::Decibels::decibelsToGain (bassGain));

        // MID: peak filter — Fender mid is broader, Marshall is sharper around 700Hz
        const float midFreq = marshall ? 700.0f : 500.0f;
        const float midQ    = marshall ? 0.9f  : 0.6f;
        const float midGain = juce::jmap (midKnob, 0.0f, 1.0f, -10.0f, 10.0f);
        mid.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sr, midFreq, midQ, juce::Decibels::decibelsToGain (midGain));

        // TREBLE: high shelf — Marshall is brighter, Fender airier
        const float trebFreq = marshall ? 3200.0f : 4500.0f;
        const float trebGain = juce::jmap (trebKnob, 0.0f, 1.0f, -12.0f, 14.0f);
        treble.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sr, trebFreq, 0.7f, juce::Decibels::decibelsToGain (trebGain));

        // Static mid-scoop intrinsic to the FMV network (~3dB cut at 450Hz)
        scoop.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sr, 450.0f, 1.2f, juce::Decibels::decibelsToGain (-2.5f));
    }

    double sr { 0.0 };
    Voicing voicing { Voicing::Marshall };
    float bassKnob { 0.5f }, midKnob { 0.5f }, trebKnob { 0.5f };

    juce::dsp::IIR::Filter<float> bass, mid, treble, scoop;
};

} // namespace deanamp
