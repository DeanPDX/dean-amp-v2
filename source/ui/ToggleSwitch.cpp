#include "ToggleSwitch.h"

namespace deanamp
{

ToggleSwitch::ToggleSwitch (juce::AudioProcessorValueTreeState& s,
                            const juce::String& pid,
                            const juce::String& t,
                            bool showTitle_)
    : title (t.toUpperCase()),
      showTitle (showTitle_),
      attachment (*s.getParameter (pid),
                  [this] (float v) { valueChanged (v); })
{
    attachment.sendInitialUpdate();
}

void ToggleSwitch::resized() {}

void ToggleSwitch::mouseDown (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
}

void ToggleSwitch::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // When the title/labels are baked into the artwork we paint an opaque
    // faceplate-coloured backing so the static switch + "OFF" underneath are
    // fully hidden before we draw the live control on top.
    if (! showTitle)
        g.fillAll (juce::Colour (0xff0b0b0d));

    if (showTitle)
    {
        auto title_ = b.removeFromTop (12.0f);
        DeanLookAndFeel::drawEngravedText (g, title, title_,
            DeanLookAndFeel::getDisplayFont (9.5f, true, 0.20f),
            juce::Justification::centred, DeanLookAndFeel::kLabelDim);
    }

    // State label occupies the bottom band; switch fills the rest.
    const float stateH = juce::jmax (12.0f, b.getHeight() * 0.26f);
    auto state = b.removeFromBottom (stateH);

    // Switch plate, scaled to the available area.
    const float plateW = juce::jmin (b.getWidth() * 0.72f, b.getHeight() * 0.55f);
    const float plateH = juce::jmin (b.getHeight() * 0.92f, plateW * 1.7f);
    auto plate = juce::Rectangle<float> (plateW, plateH).withCentre (b.getCentre());

    g.setColour (juce::Colour (0xff050608));
    g.fillRoundedRectangle (plate, 3.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (plate, 3.0f, 1.0f);

    // Bat handle pointing up (on) or down (off)
    const float handleW = plateW * 0.62f;
    const float handleH = plateH * 0.52f;
    auto handle = juce::Rectangle<float> (handleW, handleH).withCentre (plate.getCentre());
    if (on) handle.setY (plate.getY() + plateH * 0.06f);
    else    handle.setY (plate.getBottom() - handleH - plateH * 0.06f);

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (handle.translated (0.0f, 2.0f), handleW * 0.4f);

    juce::ColourGradient hg (juce::Colour (0xffe2e4e6), handle.getX(), handle.getY(),
                             juce::Colour (0xff6a6d72), handle.getRight(), handle.getY(), false);
    g.setGradientFill (hg);
    g.fillRoundedRectangle (handle, handleW * 0.4f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (handle, handleW * 0.4f, 0.8f);
    // Bright tip cap at the "live" end
    auto cap = on ? handle.removeFromTop (handleH * 0.32f)
                  : handle.removeFromBottom (handleH * 0.32f);
    g.setColour (juce::Colour (0xfff2f3f5));
    g.fillRoundedRectangle (cap, handleW * 0.35f);

    // ON/OFF label
    g.setColour (on ? DeanLookAndFeel::kGold : DeanLookAndFeel::kLabelDim);
    g.setFont (DeanLookAndFeel::getDisplayFont (juce::jmin (11.0f, stateH * 0.78f), true, 0.22f));
    g.drawText (on ? "ON" : "OFF", state, juce::Justification::centred);
}

} // namespace deanamp
