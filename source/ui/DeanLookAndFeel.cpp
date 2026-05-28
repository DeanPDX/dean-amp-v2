#include "DeanLookAndFeel.h"

namespace deanamp
{

const juce::Colour DeanLookAndFeel::kPanelBlack     { 0xff0a0a0c };
const juce::Colour DeanLookAndFeel::kPanelTop       { 0xff15171b };
const juce::Colour DeanLookAndFeel::kPanelBottom    { 0xff060708 };
const juce::Colour DeanLookAndFeel::kMetalDark      { 0xff15171b };
const juce::Colour DeanLookAndFeel::kMetalLight     { 0xff2c2f35 };
const juce::Colour DeanLookAndFeel::kEdgeShadow     { 0xff000000 };
const juce::Colour DeanLookAndFeel::kAccentCrimson  { 0xffd9203b };
const juce::Colour DeanLookAndFeel::kAccentGlow     { 0xffff5a6e };
const juce::Colour DeanLookAndFeel::kAccentAmber    { 0xffe2a23a };
const juce::Colour DeanLookAndFeel::kAccentTeal     { 0xff3ad2c4 };
const juce::Colour DeanLookAndFeel::kGold           { 0xffc6a464 };
const juce::Colour DeanLookAndFeel::kGoldDark       { 0xff6e5836 };
const juce::Colour DeanLookAndFeel::kGoldHighlight  { 0xfff0d690 };
const juce::Colour DeanLookAndFeel::kTolex          { 0xff111114 };
const juce::Colour DeanLookAndFeel::kTolexHighlight { 0xff1a1a1e };
const juce::Colour DeanLookAndFeel::kFaceplate      { 0xff14151a };
const juce::Colour DeanLookAndFeel::kLabelText      { 0xffe6e8ec };
const juce::Colour DeanLookAndFeel::kLabelDim       { 0xff8e9099 };

DeanLookAndFeel::DeanLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, kPanelBlack);
    setColour (juce::Slider::textBoxTextColourId,         kLabelDim);
    setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,                 kLabelText);
    setColour (juce::ComboBox::backgroundColourId,        juce::Colour (0xff1a1c20));
    setColour (juce::ComboBox::textColourId,              kLabelText);
    setColour (juce::ComboBox::outlineColourId,           juce::Colour (0xff2a2c30));
    setColour (juce::ComboBox::arrowColourId,             kGold);
    setColour (juce::PopupMenu::backgroundColourId,       kPanelBlack);
    setColour (juce::PopupMenu::textColourId,             kLabelText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kGold.withAlpha (0.40f));
    setColour (juce::PopupMenu::highlightedTextColourId,  juce::Colours::white);
}

juce::Font DeanLookAndFeel::getDisplayFont (float size, bool bold, float kerning)
{
    auto opts = juce::FontOptions (size);
    if (bold) opts = opts.withStyle ("Bold");
    auto f = juce::Font (opts);
    if (kerning != 0.0f) f = f.withExtraKerningFactor (kerning);
    return f;
}

void DeanLookAndFeel::drawBrushedMetalPanel (juce::Graphics& g, juce::Rectangle<float> r,
                                             juce::Colour top, juce::Colour bottom,
                                             float cornerRadius)
{
    juce::ColourGradient grad (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, cornerRadius);

    juce::Random rng (1234);
    g.setColour (juce::Colour (0xff000000).withAlpha (0.05f));
    for (int y = (int) r.getY(); y < (int) r.getBottom(); y += 2)
    {
        const float jitter = rng.nextFloat() * 0.6f;
        g.drawHorizontalLine (y, r.getX() + jitter, r.getRight() - jitter);
    }

    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (r.reduced (0.75f), cornerRadius - 0.5f, 0.75f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawRoundedRectangle (r, cornerRadius, 1.0f);
}

void DeanLookAndFeel::drawEngravedText (juce::Graphics& g, const juce::String& text,
                                        juce::Rectangle<float> bounds, juce::Font font,
                                        juce::Justification just, juce::Colour baseColour)
{
    g.setFont (font);
    g.setColour (juce::Colours::black.withAlpha (0.75f));
    g.drawText (text, bounds.translated (0.0f, -1.0f), just);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawText (text, bounds.translated (0.0f, 1.0f), just);
    g.setColour (baseColour);
    g.drawText (text, bounds, just);
}

// "Tolex" panel — black leather-look amp covering rendered with a fine cross-hatch.
void DeanLookAndFeel::drawTolexPanel (juce::Graphics& g, juce::Rectangle<float> r,
                                      float cornerRadius)
{
    // Base
    juce::ColourGradient grad (kTolexHighlight, r.getCentreX(), r.getY(),
                               kTolex,          r.getCentreX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, cornerRadius);

    // Faux-leather noise: small dark + light specks
    juce::Random rng (8675309);
    juce::Path p;
    g.saveState();
    g.reduceClipRegion (r.toNearestInt());
    for (int i = 0; i < 4000; ++i)
    {
        const float x = r.getX() + rng.nextFloat() * r.getWidth();
        const float y = r.getY() + rng.nextFloat() * r.getHeight();
        const float a = 0.04f + rng.nextFloat() * 0.10f;
        g.setColour (juce::Colour (0xff000000).withAlpha (a));
        g.fillRect (x, y, 1.0f, 1.0f);
    }
    for (int i = 0; i < 800; ++i)
    {
        const float x = r.getX() + rng.nextFloat() * r.getWidth();
        const float y = r.getY() + rng.nextFloat() * r.getHeight();
        const float a = 0.02f + rng.nextFloat() * 0.06f;
        g.setColour (juce::Colours::white.withAlpha (a));
        g.fillRect (x, y, 1.0f, 1.0f);
    }
    g.restoreState();

    // Soft vertical vignette
    juce::ColourGradient vig (juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                              juce::Colours::black.withAlpha (0.35f), r.getX(), r.getBottom(), true);
    vig.isRadial = true;
    g.setGradientFill (vig);
    g.fillRoundedRectangle (r, cornerRadius);
}

// Gold piping: a thin gold rounded outline with light/dark sides for depth.
void DeanLookAndFeel::drawGoldPiping (juce::Graphics& g, juce::Rectangle<float> r,
                                      float cornerRadius, float thickness)
{
    // Outer dark edge
    g.setColour (kGoldDark);
    g.drawRoundedRectangle (r, cornerRadius, thickness + 1.0f);
    // Gold core
    g.setColour (kGold);
    g.drawRoundedRectangle (r, cornerRadius, thickness);
    // Bright highlight on top-left
    g.setColour (kGoldHighlight.withAlpha (0.6f));
    juce::Path hi;
    hi.startNewSubPath (r.getX() + cornerRadius, r.getY() + thickness * 0.5f);
    hi.lineTo (r.getRight() - cornerRadius, r.getY() + thickness * 0.5f);
    g.strokePath (hi, juce::PathStrokeType (0.8f));
}

// Gold "DEAN AMP" engraved plaque.
void DeanLookAndFeel::drawGoldPlaque (juce::Graphics& g, juce::Rectangle<float> r,
                                      const juce::String& text)
{
    // Subtle dark inset background
    g.setColour (juce::Colour (0xff080808));
    g.fillRoundedRectangle (r.expanded (4.0f, 2.0f), 4.0f);

    // Gold border first (outer dark, gold core, highlight)
    drawGoldPiping (g, r, 4.0f, 1.6f);

    // Inside fill — very subtle gradient suggesting brushed brass behind cutout text
    juce::ColourGradient plate (juce::Colour (0xff090909), r.getCentreX(), r.getY(),
                                juce::Colour (0xff050505), r.getCentreX(), r.getBottom(), false);
    g.setGradientFill (plate);
    g.fillRoundedRectangle (r.reduced (3.0f), 3.0f);

    // Engraved gold text with deep shadow + highlight
    auto f = getDisplayFont (r.getHeight() * 0.55f, true, 0.18f);
    g.setFont (f);
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.drawText (text, r.translated (0.0f, -1.5f), juce::Justification::centred);
    g.setColour (kGoldHighlight.withAlpha (0.20f));
    g.drawText (text, r.translated (0.0f, 1.5f), juce::Justification::centred);
    g.setColour (kGold);
    g.drawText (text, r, juce::Justification::centred);
}

// Modern amp-head knob: a dark machined cylinder with a thin pointer line and a
// faint ring of tick dots — matching the photoreal faceplate render. Painted
// fully opaque so it cleanly covers the knob baked into the background image.
void DeanLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float rotaryStart, float rotaryEnd,
                                        juce::Slider& s)
{
    auto area = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f);
    const float diameter = juce::jmin (area.getWidth(), area.getHeight());
    auto knobArea = juce::Rectangle<float> (diameter, diameter).withCentre (area.getCentre());

    const float angle  = rotaryStart + sliderPos * (rotaryEnd - rotaryStart);
    const auto  centre = knobArea.getCentre();
    const float radius = diameter * 0.5f;

    // --- 1) Faint tick dots around the perimeter (engraved into the faceplate) ---
    {
        const float tickR = radius + diameter * 0.10f;
        const int numTicks = 11;
        for (int i = 0; i < numTicks; ++i)
        {
            const float t  = (float) i / (float) (numTicks - 1);
            const float a  = rotaryStart + t * (rotaryEnd - rotaryStart);
            const float ca = std::cos (a - juce::MathConstants<float>::halfPi);
            const float sa = std::sin (a - juce::MathConstants<float>::halfPi);
            const float dotR = (i == 0 || i == numTicks - 1) ? 1.3f : 0.9f;
            auto dot = juce::Rectangle<float> (dotR * 2.0f, dotR * 2.0f)
                          .withCentre ({ centre.x + ca * tickR, centre.y + sa * tickR });
            g.setColour (juce::Colour (0xff4a4c52).withAlpha (0.65f));
            g.fillEllipse (dot);
        }
    }

    // --- 2) Recessed well shadow beneath the knob ---
    {
        juce::DropShadow ds (juce::Colours::black.withAlpha (0.75f), 9, { 0, 3 });
        juce::Path p; p.addEllipse (knobArea.expanded (1.5f));
        ds.drawForPath (g, p);
    }

    // --- 3) Outer rim ring (thin brushed bezel) ---
    auto body = knobArea.reduced (diameter * 0.04f);
    {
        juce::ColourGradient ring (juce::Colour (0xff3c3e44), body.getCentreX(), body.getY(),
                                   juce::Colour (0xff0a0b0d), body.getCentreX(), body.getBottom(), false);
        g.setGradientFill (ring);
        g.fillEllipse (knobArea);
    }

    // --- 4) Knob body: domed matte black with a top-down highlight ---
    body = knobArea.reduced (diameter * 0.10f);
    {
        // Base vertical gradient (top lighter, bottom near-black)
        juce::ColourGradient grad (juce::Colour (0xff242730), body.getCentreX(), body.getY(),
                                   juce::Colour (0xff080a0d), body.getCentreX(), body.getBottom(), false);
        g.setGradientFill (grad);
        g.fillEllipse (body);

        // Domed radial highlight near the top
        juce::ColourGradient dome (juce::Colours::white.withAlpha (0.18f),
                                   body.getCentreX(), body.getY() + body.getHeight() * 0.26f,
                                   juce::Colours::transparentBlack,
                                   body.getCentreX(), body.getCentreY() + body.getHeight() * 0.10f, true);
        dome.isRadial = true;
        g.setGradientFill (dome);
        g.fillEllipse (body);

        // Crisp dark edge + faint inner highlight ring
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.drawEllipse (body, 1.2f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawEllipse (body.reduced (1.6f), 0.8f);
    }

    // --- 5) Pointer line (thin bright indicator from mid-radius to the rim) ---
    {
        const float r0 = radius * 0.30f;
        const float r1 = radius * 0.80f;
        const float ca = std::cos (angle - juce::MathConstants<float>::halfPi);
        const float sa = std::sin (angle - juce::MathConstants<float>::halfPi);
        const juce::Point<float> p0 { centre.x + ca * r0, centre.y + sa * r0 };
        const juce::Point<float> p1 { centre.x + ca * r1, centre.y + sa * r1 };

        // Shadow underline
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawLine (p0.x, p0.y + 1.0f, p1.x, p1.y + 1.0f, 2.6f);
        // Bright indicator
        g.setColour (juce::Colour (0xffe9e3d4));
        g.drawLine (p0.x, p0.y, p1.x, p1.y, 2.2f);
    }

    juce::ignoreUnused (s);
}

// Toggle = small LED + label (used for in-line toggles).
void DeanLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                        bool /*hi*/, bool /*down*/)
{
    auto bounds = b.getLocalBounds().toFloat().reduced (2.0f);
    auto ledArea = bounds.removeFromLeft (bounds.getHeight()).reduced (3.0f);
    const bool on = b.getToggleState();

    g.setColour (juce::Colour (0xff05060a));
    g.fillEllipse (ledArea);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawEllipse (ledArea, 1.0f);

    auto inner = ledArea.reduced (3.0f);
    if (on)
    {
        g.setColour (kAccentGlow.withAlpha (0.55f));
        g.fillEllipse (ledArea.expanded (4.0f));
        juce::ColourGradient rg (juce::Colour (0xffffd5dc), inner.getCentreX(), inner.getCentreY(),
                                 kAccentCrimson, inner.getRight(), inner.getBottom(), true);
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

    bounds.removeFromLeft (6.0f);
    g.setColour (on ? kLabelText : kLabelDim);
    g.setFont (getDisplayFont (11.0f));
    g.drawText (b.getButtonText().toUpperCase(), bounds, juce::Justification::centredLeft);
}

void DeanLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height,
                                    bool /*isButtonDown*/, int /*bx*/, int /*by*/,
                                    int /*bw*/, int /*bh*/, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    const float radius = juce::jmin (8.0f, height * 0.22f);

    // Dark inset field (drawn opaque to cover the baked frame in the artwork)
    juce::ColourGradient fill (juce::Colour (0xff1a1c20), bounds.getCentreX(), bounds.getY(),
                               juce::Colour (0xff0e0f12), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (bounds, radius);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
    g.setColour (juce::Colour (0xff34373e));
    g.drawRoundedRectangle (bounds.reduced (1.0f), radius - 0.5f, 1.0f);

    // Chevron (v) on the right
    auto a = bounds.removeFromRight ((float) height).reduced (height * 0.32f);
    const float midY = a.getCentreY() - a.getHeight() * 0.1f;
    juce::Path chev;
    chev.startNewSubPath (a.getX(), midY);
    chev.lineTo (a.getCentreX(), midY + a.getHeight() * 0.45f);
    chev.lineTo (a.getRight(), midY);
    g.setColour (juce::Colour (0xffb8bcc4));
    g.strokePath (chev, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
    juce::ignoreUnused (box);
}

// Horizontal slider: thin groove, gold fill up to the thumb, bright vertical thumb.
// Matches the "CAB LEVEL" control in the bottom bar.
void DeanLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float minSliderPos, float maxSliderPos,
                                        juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearBar)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                          minSliderPos, maxSliderPos, style, s);
        return;
    }

    const float cy = (float) y + (float) height * 0.5f;
    const float left = (float) x;
    const float right = (float) x + (float) width;

    // Unfilled groove
    g.setColour (juce::Colour (0xff3a3c42));
    g.drawLine (left, cy, right, cy, 2.0f);
    // Filled portion (gold)
    g.setColour (kGold);
    g.drawLine (left, cy, sliderPos, cy, 2.6f);

    // Thumb: a short bright vertical bar
    const float th = (float) height * 0.85f;
    auto thumb = juce::Rectangle<float> (4.0f, th).withCentre ({ sliderPos, cy });
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (thumb.translated (0.0f, 1.0f), 1.5f);
    juce::ColourGradient tg (juce::Colour (0xfff3eedd), thumb.getCentreX(), thumb.getY(),
                             juce::Colour (0xffc9c0a6), thumb.getCentreX(), thumb.getBottom(), false);
    g.setGradientFill (tg);
    g.fillRoundedRectangle (thumb, 1.5f);
}

juce::Font DeanLookAndFeel::getLabelFont (juce::Label&)
{
    return getDisplayFont (12.0f, true);
}

juce::Font DeanLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return getDisplayFont (12.0f, true);
}

juce::Font DeanLookAndFeel::getPopupMenuFont()
{
    return getDisplayFont (12.5f, false);
}

} // namespace deanamp
