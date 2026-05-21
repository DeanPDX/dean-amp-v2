#include "LedButton.h"

namespace deanamp
{

LedButton::LedButton (juce::AudioProcessorValueTreeState& s,
                      const juce::String& paramID,
                      const juce::String& labelText)
    : attachment (s, paramID, button)
{
    button.setButtonText (labelText);
    button.setClickingTogglesState (true);
    addAndMakeVisible (button);
}

void LedButton::resized()
{
    button.setBounds (getLocalBounds());
}

} // namespace deanamp
