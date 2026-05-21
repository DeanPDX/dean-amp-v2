#include "DeanLookAndFeel.h"

namespace deanamp
{

const juce::Colour DeanLookAndFeel::kPanelBlack     { 0xff0a0a0c };
const juce::Colour DeanLookAndFeel::kPanelTop       { 0xff1d1f23 };
const juce::Colour DeanLookAndFeel::kPanelBottom    { 0xff0d0e10 };
const juce::Colour DeanLookAndFeel::kMetalDark      { 0xff15171b };
const juce::Colour DeanLookAndFeel::kMetalLight     { 0xff2c2f35 };
const juce::Colour DeanLookAndFeel::kEdgeShadow     { 0xff000000 };
const juce::Colour DeanLookAndFeel::kAccentCrimson  { 0xffd9203b };
const juce::Colour DeanLookAndFeel::kAccentGlow     { 0xffff5a6e };
const juce::Colour DeanLookAndFeel::kLabelText      { 0xffe6e8ec };
const juce::Colour DeanLookAndFeel::kLabelDim       { 0xff7a7e86 };

DeanLookAndFeel::DeanLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, kPanelBlack);
    setColour (juce::Slider::textBoxTextColourId,         kLabelText);
    setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,                 kLabelText);
    setColour (juce::ComboBox::backgroundColourId,        kMetalDark);
    setColour (juce::ComboBox::textColourId,              kLabelText);
    setColour (juce::ComboBox::outlineColourId,           kMetalLight);
    setColour (juce::ComboBox::arrowColourId,             kAccentCrimson);
    setColour (juce::PopupMenu::backgroundColourId,       kPanelBlack);
    setColour (juce::PopupMenu::textColourId,             kLabelText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccentCrimson.withAlpha (0.4f));
    setColour (juce::PopupMenu::highlightedTextColourId,  juce::Colours::white);
}

void DeanLookAndFeel::drawBrushedMetalPanel (juce::Graphics& g, juce::Rectangle<float> r,
                                             juce::Colour top, juce::Colour bottom,
                                             float cornerRadius)
{
    // Base vertical gradient
    juce::ColourGradient grad (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, cornerRadius);

    // Brushed lines — subtle horizontal noise
    juce::Random rng (1234);
    g.setColour (juce::Colour (0xff000000).withAlpha (0.06f));
    for (int y = (int) r.getY(); y < (int) r.getBottom(); y += 2)
    {
        const float jitter = rng.nextFloat() * 0.6f;
        g.drawHorizontalLine (y, r.getX() + jitter, r.getRight() - jitter);
    }

    // Inner highlight rim
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (r.reduced (0.75f), cornerRadius - 0.5f, 0.75f);

    // Outer shadow edge
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawRoundedRectangle (r.reduced (0.0f), cornerRadius, 1.0f);
}

void DeanLookAndFeel::drawEngravedText (juce::Graphics& g, const juce::String& text,
                                        juce::Rectangle<float> bounds, juce::Font font,
                                        juce::Justification just)
{
    g.setFont (font);
    // Engraved effect: dark shadow above, light highlight below
    g.setColour (juce::Colours::black.withAlpha (0.75f));
    g.drawText (text, bounds.translated (0.0f, -1.0f), just);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawText (text, bounds.translated (0.0f, 1.0f), just);
    g.setColour (kLabelText);
    g.drawText (text, bounds, just);
}

void DeanLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float rotaryStart, float rotaryEnd,
                                        juce::Slider& s)
{
    auto area = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
    const float diameter = juce::jmin (area.getWidth(), area.getHeight());
    auto knobArea = juce::Rectangle<float> (diameter, diameter).withCentre (area.getCentre());

    const float angle = rotaryStart + sliderPos * (rotaryEnd - rotaryStart);
    auto centre = knobArea.getCentre();
    const float radius = diameter * 0.5f;

    // --- 1) Outer recessed well (drop shadow into the panel) ---
    {
        auto well = knobArea.expanded (3.0f);
        juce::DropShadow ds (juce::Colours::black.withAlpha (0.65f), 8, { 0, 2 });
        juce::Path p; p.addEllipse (well);
        ds.drawForPath (g, p);
        g.setColour (juce::Colour (0xff050608));
        g.fillEllipse (well);
    }

    // --- 2) Glow arc behind the knob (showing value) ---
    {
        auto arcBounds = knobArea.expanded (-1.0f);
        const float arcR = arcBounds.getWidth() * 0.5f;
        juce::Path bgArc, valArc;
        bgArc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f,
                             rotaryStart, rotaryEnd, true);
        valArc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f,
                              rotaryStart, angle, true);

        g.setColour (juce::Colour (0xff202428));
        g.strokePath (bgArc, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Glow underlay
        g.setColour (kAccentGlow.withAlpha (0.30f));
        g.strokePath (valArc, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // Bright core
        g.setColour (kAccentCrimson);
        g.strokePath (valArc, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // --- 3) Knob body (machined aluminum) ---
    auto knobBody = knobArea.reduced (diameter * 0.18f);
    {
        // Base gradient (top highlight to bottom shadow)
        juce::ColourGradient grad (juce::Colour (0xff3b3f47), knobBody.getCentreX(), knobBody.getY(),
                                   juce::Colour (0xff14161a), knobBody.getCentreX(), knobBody.getBottom(), false);
        g.setGradientFill (grad);
        g.fillEllipse (knobBody);

        // Concentric machined-circle texture
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        for (int i = 1; i <= 6; ++i)
            g.drawEllipse (knobBody.reduced ((float) i * (knobBody.getWidth() * 0.02f)), 0.6f);

        // Radial inner highlight
        juce::ColourGradient rg (juce::Colours::white.withAlpha (0.20f),
                                 knobBody.getCentreX(), knobBody.getY() + knobBody.getHeight() * 0.25f,
                                 juce::Colours::transparentBlack,
                                 knobBody.getCentreX(), knobBody.getCentreY(),
                                 true);
        rg.isRadial = true;
        g.setGradientFill (rg);
        g.fillEllipse (knobBody);

        // Outer rim
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawEllipse (knobBody, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawEllipse (knobBody.reduced (1.0f), 0.6f);
    }

    // --- 4) Indicator line ---
    {
        const float indLen = radius * 0.55f;
        const float indStart = radius * 0.25f;
        juce::Path indicator;
        indicator.startNewSubPath (0.0f, -indStart);
        indicator.lineTo (0.0f, -indLen);
        const float thickness = juce::jmax (2.0f, radius * 0.06f);

        juce::AffineTransform tr = juce::AffineTransform::rotation (angle)
                                       .translated (centre.x, centre.y);

        // Glow under indicator
        g.setColour (kAccentGlow.withAlpha (0.55f));
        g.strokePath (indicator, juce::PathStrokeType (thickness + 2.0f,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded),
                      tr);
        // Core indicator
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.strokePath (indicator, juce::PathStrokeType (thickness,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded),
                      tr);
    }

    // --- 5) Centre dot ---
    {
        const float r2 = diameter * 0.04f;
        g.setColour (juce::Colours::black);
        g.fillEllipse (juce::Rectangle<float> (r2 * 2.0f, r2 * 2.0f).withCentre (centre));
    }

    juce::ignoreUnused (s);
}

void DeanLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                        bool /*hi*/, bool /*down*/)
{
    auto bounds = b.getLocalBounds().toFloat().reduced (2.0f);

    // LED area on left, label on right
    auto ledArea = bounds.removeFromLeft (bounds.getHeight()).reduced (3.0f);
    const bool on = b.getToggleState();

    // LED bezel
    g.setColour (juce::Colour (0xff05060a));
    g.fillEllipse (ledArea);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawEllipse (ledArea, 1.0f);

    // LED itself
    auto inner = ledArea.reduced (3.0f);
    if (on)
    {
        // Glow halo
        g.setColour (DeanLookAndFeel::kAccentGlow.withAlpha (0.55f));
        g.fillEllipse (ledArea.expanded (4.0f));
        juce::ColourGradient rg (juce::Colour (0xffffd5dc), inner.getCentreX(), inner.getCentreY(),
                                 DeanLookAndFeel::kAccentCrimson, inner.getRight(), inner.getBottom(), true);
        rg.isRadial = true;
        g.setGradientFill (rg);
        g.fillEllipse (inner);
    }
    else
    {
        juce::ColourGradient rg (juce::Colour (0xff2b1418), inner.getCentreX(), inner.getCentreY(),
                                 juce::Colour (0xff0c0608), inner.getRight(), inner.getBottom(), true);
        rg.isRadial = true;
        g.setGradientFill (rg);
        g.fillEllipse (inner);
    }

    // Label
    bounds.removeFromLeft (6.0f);
    g.setColour (on ? DeanLookAndFeel::kLabelText : DeanLookAndFeel::kLabelDim);
    g.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
    g.drawText (b.getButtonText().toUpperCase(), bounds, juce::Justification::centredLeft);
}

void DeanLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height,
                                    bool /*isButtonDown*/, int /*bx*/, int /*by*/,
                                    int /*bw*/, int /*bh*/, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);

    drawBrushedMetalPanel (g, bounds.reduced (1.0f), kMetalLight, kMetalDark, 4.0f);

    // Arrow
    auto arrowArea = bounds.removeFromRight ((float) height).reduced (10.0f);
    juce::Path tri;
    tri.addTriangle (arrowArea.getX(), arrowArea.getY(),
                     arrowArea.getRight(), arrowArea.getY(),
                     arrowArea.getCentreX(), arrowArea.getBottom());
    g.setColour (kAccentCrimson);
    g.fillPath (tri);

    juce::ignoreUnused (box);
}

juce::Font DeanLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::Font (juce::FontOptions (12.0f).withStyle ("Bold"));
}

juce::Font DeanLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (13.0f).withStyle ("Bold"));
}

juce::Font DeanLookAndFeel::getPopupMenuFont()
{
    return juce::Font (juce::FontOptions (13.0f));
}

} // namespace deanamp
