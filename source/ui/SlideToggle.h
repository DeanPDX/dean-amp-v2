#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DeanLookAndFeel.h"

namespace deanamp
{

/**
 * Horizontal "pill" slide switch (the STEREO/MONO selectors in the top bar).
 * The STEREO / MONO text labels are part of the photoreal background; this
 * component only paints the interactive sliding pill on top of it.
 *
 * Backed by a bool parameter: false = left position, true = right position.
 */
class SlideToggle : public juce::Component
{
public:
    SlideToggle (juce::AudioProcessorValueTreeState& s, const juce::String& paramID)
        : attachment (*s.getParameter (paramID),
                      [this] (float v) { on = v > 0.5f; repaint(); })
    {
        attachment.sendInitialUpdate();
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
    }

    void paint (juce::Graphics& g) override
    {
        auto track = getLocalBounds().toFloat().reduced (1.0f);
        const float r = track.getHeight() * 0.5f;

        // Track well
        juce::ColourGradient tg (juce::Colour (0xff0c0d10), track.getCentreX(), track.getY(),
                                 juce::Colour (0xff202329), track.getCentreX(), track.getBottom(), false);
        g.setGradientFill (tg);
        g.fillRoundedRectangle (track, r);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawRoundedRectangle (track, r, 1.0f);

        // Knob
        const float d = track.getHeight() - 4.0f;
        const float cx = on ? track.getRight() - d * 0.5f - 2.0f
                            : track.getX()    + d * 0.5f + 2.0f;
        auto knob = juce::Rectangle<float> (d, d).withCentre ({ cx, track.getCentreY() });

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (knob.translated (0.0f, 1.0f));
        juce::ColourGradient kg (juce::Colour (0xfff0f1f3), knob.getCentreX(), knob.getY(),
                                 juce::Colour (0xff9a9da3), knob.getCentreX(), knob.getBottom(), false);
        g.setGradientFill (kg);
        g.fillEllipse (knob);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawEllipse (knob, 0.8f);
    }

private:
    juce::ParameterAttachment attachment;
    bool on { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlideToggle)
};

} // namespace deanamp
