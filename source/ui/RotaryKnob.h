#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DeanLookAndFeel.h"

namespace deanamp
{

/**
 * A rotary knob with engraved label and a value readout that fades in while
 * the user is dragging.
 */
class RotaryKnob : public juce::Component
{
public:
    RotaryKnob (juce::AudioProcessorValueTreeState& s,
                const juce::String& paramID,
                const juce::String& labelText);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    juce::Label  label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    juce::String caption;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RotaryKnob)
};

} // namespace deanamp
