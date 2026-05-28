#include "RotaryKnob.h"

namespace deanamp
{

RotaryKnob::RotaryKnob (juce::AudioProcessorValueTreeState& s,
                        const juce::String& paramID,
                        const juce::String& labelText,
                        bool compact)
    : attachment (s, paramID, slider),
      caption (labelText.toUpperCase()),
      compactMode (compact)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false,
                            compact ? 56 : 70, compact ? 12 : 14);
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
    const float labelH = compactMode ? 12.0f : 14.0f;
    auto labelBounds = bounds.removeFromTop (labelH);
    DeanLookAndFeel::drawEngravedText (g, caption, labelBounds,
        DeanLookAndFeel::getDisplayFont (compactMode ? 9.5f : 10.5f, true, 0.18f),
        juce::Justification::centred, labelColour);
}

void RotaryKnob::resized()
{
    auto bounds = getLocalBounds();
    const int labelH = compactMode ? 12 : 14;
    bounds.removeFromTop (labelH);
    slider.setBounds (bounds);
}

} // namespace deanamp
