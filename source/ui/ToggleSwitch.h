#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DeanLookAndFeel.h"

namespace deanamp
{

/**
 * Physical toggle switch (vintage amp style) — a chrome bat handle that
 * flips up/down, with an OFF/ON label underneath.
 */
class ToggleSwitch : public juce::Component
{
public:
    ToggleSwitch (juce::AudioProcessorValueTreeState& s,
                  const juce::String& paramID,
                  const juce::String& title,
                  bool showTitle = true);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::String title;
    bool showTitle { true };
    juce::ParameterAttachment attachment;
    bool on { false };

    void valueChanged (float v) { on = v > 0.5f; repaint(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToggleSwitch)
};

} // namespace deanamp
