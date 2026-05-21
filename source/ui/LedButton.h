#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DeanLookAndFeel.h"

namespace deanamp
{

/** A labeled illuminated toggle button. */
class LedButton : public juce::Component
{
public:
    LedButton (juce::AudioProcessorValueTreeState& s,
               const juce::String& paramID,
               const juce::String& labelText);

    void resized() override;

private:
    juce::ToggleButton button;
    juce::AudioProcessorValueTreeState::ButtonAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LedButton)
};

} // namespace deanamp
