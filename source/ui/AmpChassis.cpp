#include "AmpChassis.h"
#include "DeanLookAndFeel.h"

namespace deanamp
{

AmpChassis::AmpChassis() { setInterceptsMouseClicks (false, true); }

void AmpChassis::resized() { layoutInternals(); }

void AmpChassis::layoutInternals()
{
    auto b = getLocalBounds();
    // Faceplate occupies the bottom ~38% of the chassis
    const int faceH = juce::roundToInt (b.getHeight() * 0.38f);
    faceplateBounds = b.removeFromBottom (faceH).reduced (24, 12);

    // Status LED + preset name on the left of the faceplate
    statusLedBounds = faceplateBounds.removeFromLeft (130);
    auto presetArea = statusLedBounds;
    // LED is small on the left, name to the right of it
    auto led = presetArea.removeFromLeft (20).withSizeKeepingCentre (10, 10);
    presetNameBounds = presetArea.reduced (4, 0);
    statusLedBounds  = led;
}

void AmpChassis::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // --- 1) Outer tolex body ---
    auto body = b.reduced (8.0f);
    {
        // Drop shadow under chassis
        juce::DropShadow ds (juce::Colours::black.withAlpha (0.75f), 18, { 0, 6 });
        juce::Path bp; bp.addRoundedRectangle (body, 16.0f);
        ds.drawForPath (g, bp);
    }
    DeanLookAndFeel::drawTolexPanel (g, body, 16.0f);

    // --- 2) Corner protectors (dark metal squares with rivets at four corners) ---
    auto drawCorner = [&] (juce::Point<float> p, float orientX, float orientY)
    {
        const float sz = 26.0f;
        juce::Rectangle<float> r (p.x, p.y, sz, sz);
        if (orientX < 0) r.setX (p.x - sz);
        if (orientY < 0) r.setY (p.y - sz);

        juce::ColourGradient cg (juce::Colour (0xff2c2e34), r.getCentreX(), r.getY(),
                                 juce::Colour (0xff0a0b0d), r.getCentreX(), r.getBottom(), false);
        g.setGradientFill (cg);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colour (0xff5a5d62).withAlpha (0.45f));
        g.drawRoundedRectangle (r.reduced (1.0f), 3.0f, 0.6f);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawRoundedRectangle (r, 4.0f, 0.8f);

        // Rivet dot
        auto rivet = juce::Rectangle<float> (4.0f, 4.0f).withCentre (r.getCentre());
        g.setColour (juce::Colour (0xff111214));
        g.fillEllipse (rivet);
        g.setColour (juce::Colour (0xff3a3d42));
        g.drawEllipse (rivet, 0.5f);
    };
    drawCorner (body.getTopLeft()    .translated (-2.0f, -2.0f),  +1.0f, +1.0f);
    drawCorner (body.getTopRight()   .translated ( 2.0f, -2.0f),  -1.0f, +1.0f);
    drawCorner (body.getBottomLeft() .translated (-2.0f,  2.0f),  +1.0f, -1.0f);
    drawCorner (body.getBottomRight().translated ( 2.0f,  2.0f),  -1.0f, -1.0f);

    // --- 3) Gold piping framing the upper panel ---
    auto upperPanel = body.reduced (20.0f);
    // Carve out the faceplate area from the gold-framed region
    const float faceY = body.getBottom() - body.getHeight() * 0.38f;
    upperPanel = upperPanel.withBottom (faceY - 6.0f);
    DeanLookAndFeel::drawGoldPiping (g, upperPanel, 6.0f, 1.4f);

    // --- 4) Handle suggestion (top centre) ---
    {
        const float w = 110.0f, h = 12.0f;
        auto handle = juce::Rectangle<float> (body.getCentreX() - w * 0.5f,
                                              body.getY() - h * 0.45f, w, h);
        // Posts
        for (float dx : { -w * 0.5f + 6.0f, w * 0.5f - 6.0f })
        {
            auto post = juce::Rectangle<float> (8.0f, 14.0f)
                            .withCentre ({ handle.getCentreX() + dx, body.getY() + 4.0f });
            juce::ColourGradient pg (juce::Colour (0xff8a8c8e), post.getCentreX(), post.getY(),
                                     juce::Colour (0xff2a2c30), post.getCentreX(), post.getBottom(), false);
            g.setGradientFill (pg);
            g.fillRoundedRectangle (post, 2.0f);
        }
        // Strap
        juce::Path strap;
        strap.startNewSubPath (handle.getX() + 8.0f, handle.getCentreY());
        strap.cubicTo (handle.getX() + 12.0f, handle.getY(),
                       handle.getRight() - 12.0f, handle.getY(),
                       handle.getRight() - 8.0f, handle.getCentreY());
        g.setColour (juce::Colour (0xff141517));
        g.strokePath (strap, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xff363840));
        g.strokePath (strap, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    // --- 5) Brand plaque centred in the upper panel ---
    {
        const float pw = juce::jmin (upperPanel.getWidth() * 0.52f, 360.0f);
        const float ph = juce::jmin (upperPanel.getHeight() * 0.42f, 78.0f);
        auto plaque = juce::Rectangle<float> (pw, ph).withCentre (upperPanel.getCentre());
        DeanLookAndFeel::drawGoldPlaque (g, plaque, "DEAN  AMP");
    }

    // --- 6) Faceplate ---
    auto face = b.removeFromBottom (b.getHeight() * 0.38f).reduced (16.0f, 6.0f);
    {
        juce::ColourGradient fg (juce::Colour (0xff111215), face.getCentreX(), face.getY(),
                                 juce::Colour (0xff070809), face.getCentreX(), face.getBottom(),
                                 false);
        g.setGradientFill (fg);
        g.fillRoundedRectangle (face, 4.0f);
        g.setColour (juce::Colour (0xff080809));
        g.drawRoundedRectangle (face, 4.0f, 1.0f);
        // Soft top highlight
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.drawLine (face.getX() + 6.0f, face.getY() + 1.0f,
                    face.getRight() - 6.0f, face.getY() + 1.0f, 0.8f);
    }

    // Status LED — glowing crimson dot
    {
        auto led = statusLedBounds.toFloat();
        g.setColour (DeanLookAndFeel::kAccentGlow.withAlpha (0.55f));
        g.fillEllipse (led.expanded (4.0f));
        juce::ColourGradient rg (juce::Colour (0xffffd5dc), led.getCentreX(), led.getCentreY(),
                                 DeanLookAndFeel::kAccentCrimson, led.getRight(), led.getBottom(), true);
        rg.isRadial = true;
        g.setGradientFill (rg);
        g.fillEllipse (led);
    }

    // Preset name to the right of the LED
    {
        auto name = presetNameBounds.toFloat();
        DeanLookAndFeel::drawEngravedText (g, presetName.toUpperCase(), name,
            DeanLookAndFeel::getDisplayFont (12.0f, true, 0.22f),
            juce::Justification::centredLeft, DeanLookAndFeel::kGold);
    }
}

} // namespace deanamp
