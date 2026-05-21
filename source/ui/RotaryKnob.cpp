#include "RotaryKnob.h"

namespace deanamp
{

RotaryKnob::RotaryKnob (juce::AudioProcessorValueTreeState& s,
                        const juce::String& paramID,
                        const juce::String& labelText)
    : attachment (s, paramID, slider),
      caption (labelText.toUpperCase())
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 14);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);
    slider.setColour (juce::Slider::textBoxTextColourId, DeanLookAndFeel::kLabelDim);
    slider.setVelocityBasedMode (true);
    slider.setVelocityModeParameters (0.4, 1, 0.09, false);
    addAndMakeVisible (slider);
}

void RotaryKnob::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto labelBounds = bounds.removeFromBottom (16.0f);
    DeanLookAndFeel::drawEngravedText (g, caption, labelBounds,
        juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
}

void RotaryKnob::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromBottom (16); // engraved label area
    slider.setBounds (bounds);
}

} // namespace deanamp
