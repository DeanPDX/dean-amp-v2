#pragma once
#include <juce_dsp/juce_dsp.h>

namespace deanamp
{

/**
 * Simple envelope-follower noise gate. Hysteresis between open/close thresholds
 * to avoid chatter on borderline signals.
 */
class NoiseGate
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        envState = 0.0f;
        gainState = 0.0f;
        attackCoef  = std::exp (-1.0f / (0.003f  * (float) sr));   // 3ms
        releaseCoef = std::exp (-1.0f / (0.120f  * (float) sr));   // 120ms
        gateAttack  = std::exp (-1.0f / (0.001f  * (float) sr));
        gateRelease = std::exp (-1.0f / (0.080f  * (float) sr));
    }

    void setThresholdDb (float dB) { thresholdDb = dB; }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        const float openLin  = juce::Decibels::decibelsToGain (thresholdDb);
        const float closeLin = juce::Decibels::decibelsToGain (thresholdDb - 6.0f);
        const size_t numCh = block.getNumChannels();
        const size_t numS  = block.getNumSamples();

        for (size_t i = 0; i < numS; ++i)
        {
            float peak = 0.0f;
            for (size_t ch = 0; ch < numCh; ++ch)
                peak = juce::jmax (peak, std::abs (block.getChannelPointer (ch)[i]));

            const float coef = peak > envState ? attackCoef : releaseCoef;
            envState = peak + coef * (envState - peak);

            float target;
            if (envState > openLin)       target = 1.0f;
            else if (envState < closeLin) target = 0.0f;
            else                          target = gainState; // hold

            const float gCoef = target > gainState ? gateAttack : gateRelease;
            gainState = target + gCoef * (gainState - target);

            for (size_t ch = 0; ch < numCh; ++ch)
                block.getChannelPointer (ch)[i] *= gainState;
        }
    }

private:
    double sr { 0.0 };
    float thresholdDb { -60.0f };
    float envState { 0.0f }, gainState { 0.0f };
    float attackCoef { 0.0f }, releaseCoef { 0.0f };
    float gateAttack { 0.0f }, gateRelease { 0.0f };
};

} // namespace deanamp
