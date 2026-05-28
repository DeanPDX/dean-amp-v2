#include "LevelMeter.h"
#include "DeanLookAndFeel.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace deanamp
{

LevelMeter::LevelMeter (std::atomic<float>& l, std::atomic<float>& r)
    : srcL (l), srcR (r)
{
    startTimerHz (30);
}

LevelMeter::~LevelMeter() { stopTimer(); }

void LevelMeter::timerCallback()
{
    auto smooth = [] (float disp, float src)
    {
        const float coef = (src > disp) ? 0.6f : 0.10f;
        return disp + coef * (src - disp);
    };

    dispL = smooth (dispL, srcL.load());
    dispR = smooth (dispR, srcR.load());

    if (dispL >= peakL) { peakL = dispL; peakHoldL = 30; }
    else if (--peakHoldL <= 0) peakL = std::max (0.0f, peakL * 0.94f);

    if (dispR >= peakR) { peakR = dispR; peakHoldR = 30; }
    else if (--peakHoldR <= 0) peakR = std::max (0.0f, peakR * 0.94f);

    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);

    // Two thin bars side by side
    const float gap = 2.0f;
    const float colW = (b.getWidth() - gap) * 0.5f;
    auto barL = juce::Rectangle<float> (b.getX(),          b.getY(), colW, b.getHeight());
    auto barR = juce::Rectangle<float> (b.getX() + colW + gap, b.getY(), colW, b.getHeight());

    auto drawBar = [&] (juce::Rectangle<float> bar, float val, float pk)
    {
        // dB-mapped position (val is linear; convert to a normalized 0..1 over -60..+6 dB)
        auto toY = [&] (float v) -> float
        {
            const float dB = juce::Decibels::gainToDecibels (v, -60.0f);
            const float t  = juce::jlimit (0.0f, 1.0f, (dB + 60.0f) / 66.0f);
            return bar.getBottom() - t * bar.getHeight();
        };

        // Background well
        g.setColour (juce::Colour (0xff0a0b0d));
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (bar, 2.0f, 0.8f);

        // Active fill: gradient green→amber→red
        const float fillY = toY (val);
        if (fillY < bar.getBottom() - 1.0f)
        {
            juce::Rectangle<float> fill (bar.getX() + 1.0f, fillY,
                                         bar.getWidth() - 2.0f, bar.getBottom() - fillY - 1.0f);
            juce::ColourGradient grad (juce::Colour (0xff44d774), bar.getCentreX(), bar.getBottom(),
                                       juce::Colour (0xffff3550), bar.getCentreX(), bar.getY(),
                                       false);
            grad.addColour (0.55, juce::Colour (0xffd6c43a));
            g.setGradientFill (grad);
            g.fillRect (fill);
        }

        // Peak indicator (thin horizontal line)
        if (pk > 0.001f)
        {
            const float pkY = toY (pk);
            g.setColour (juce::Colour (0xfffff0c0));
            g.fillRect (bar.getX() + 1.0f, pkY - 0.5f, bar.getWidth() - 2.0f, 1.5f);
        }
    };

    drawBar (barL, dispL, peakL);
    drawBar (barR, dispR, peakR);
}

} // namespace deanamp
