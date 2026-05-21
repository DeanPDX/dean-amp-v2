#pragma once
#include "PluginProcessor.h"
#include "ui/DeanLookAndFeel.h"
#include "ui/RotaryKnob.h"
#include "ui/LedButton.h"

namespace deanamp
{

class DeanAmpEditor : public juce::AudioProcessorEditor
{
public:
    explicit DeanAmpEditor (DeanAmpProcessor&);
    ~DeanAmpEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    [[maybe_unused]] DeanAmpProcessor& proc;
    DeanLookAndFeel lf;

    // Rotaries
    RotaryKnob kGate, kInput, kGain, kBass, kMid, kTreble,
               kPresence, kResonance, kMaster, kOutput, kMic;

    // Selectors
    juce::ComboBox channelBox, voicingBox, cabBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        channelAtt, voicingAtt, cabAtt;

    // Toggles
    LedButton brightBtn, cabBypassBtn;

    void drawFaceplate (juce::Graphics&);
    void drawBranding  (juce::Graphics&);
    void drawSectionBackings (juce::Graphics&);

    juce::Rectangle<int> preampSection, eqSection, powerSection, cabSection;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeanAmpEditor)
};

} // namespace deanamp
