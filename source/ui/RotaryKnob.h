#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DeanLookAndFeel.h"

namespace deanamp
{

/**
 * Rotary knob with an engraved label below.
 * `compact = true` reduces label height for tight strips.
 */
class RotaryKnob : public juce::Component
{
public:
    RotaryKnob (juce::AudioProcessorValueTreeState& s,
                const juce::String& paramID,
                const juce::String& labelText,
                bool compact = false);

    void paint (juce::Graphics&) override;
    void resized() override;

    void setLabelColour (juce::Colour c) { labelColour = c; repaint(); }

private:
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    juce::String caption;
    juce::Colour labelColour { DeanLookAndFeel::kLabelText };
    bool compactMode;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RotaryKnob)
};

} // namespace deanamp
