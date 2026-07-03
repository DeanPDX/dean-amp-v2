#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/NoiseGate.h"
#include "dsp/TubeStage.h"
#include "dsp/ToneStack.h"
#include "dsp/PowerAmp.h"
#include "dsp/CabSim.h"
#include "dsp/SpringReverb.h"

namespace deanamp
{

class DeanAmpProcessor : public juce::AudioProcessor
{
public:
    DeanAmpProcessor();
    ~DeanAmpProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Dean Amp"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; } // spring reverb tail

    int getNumPrograms() override            { return 1; }
    int getCurrentProgram() override         { return 0; }
    void setCurrentProgram (int) override    {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // Level metering (read by the editor on a timer). Linear 0..1+ peak per block.
    std::atomic<float> inputMeterL  { 0.0f }, inputMeterR  { 0.0f };
    std::atomic<float> outputMeterL { 0.0f }, outputMeterR { 0.0f };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void updateFromParams();

    NoiseGate    gate;
    TubePreamp   preamp;
    ToneStack    toneStack;
    PowerAmp     powerAmp;
    CabSim       cab;
    SpringReverb reverb;

    float inputTrim { 1.0f }, outputTrim { 1.0f };
    float cabLevelGain { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeanAmpProcessor)
};

} // namespace deanamp
