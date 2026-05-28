#include "PluginEditor.h"
#include "BinaryData.h"

namespace deanamp
{

// ----------------------------------------------------------------------------
// Layout table. Coordinates are fractions of the 1536x1024 reference render.
// Tweak these against snapshots to align overlays with the baked-in artwork.
// ----------------------------------------------------------------------------
namespace layout
{
    // Faceplate knob row
    static constexpr float kFaceY  = 0.695f;   // knob centre-y
    static constexpr float kFaceVy = 0.741f;   // value readout centre-y

    // Top control row (value-readout y; knob centres set per-control below)
    static constexpr float kTopVy  = 0.247f;
}

DeanAmpEditor::DeanAmpEditor (DeanAmpProcessor& p)
    : juce::AudioProcessorEditor (&p),
      proc (p),
      brightSwitch (p.apvts, "bright",     "Bright", false),
      bypassSwitch (p.apvts, "cabBypass",  "Bypass", false),
      inModeSwitch (p.apvts, "inputMono"),
      outModeSwitch(p.apvts, "outputMono"),
      inMeter  (p.inputMeterL,  p.inputMeterR),
      outMeter (p.outputMeterL, p.outputMeterR)
{
    setLookAndFeel (&lf);

    background = juce::ImageCache::getFromMemory (BinaryData::amp_face_png,
                                                  BinaryData::amp_face_pngSize);

    // --- Configure the 10 rotary controls ----------------------------------
    struct Def { const char* pid; float nx, ny, nvy, nd; ValueStyle style; };
    const Def defs[] = {
        // Faceplate row (gain..depth)  — 0..1 params shown 0..10
        { "gain",      0.3236f, layout::kFaceY, layout::kFaceVy, 0.052f, Ten },
        { "bass",      0.3887f, layout::kFaceY, layout::kFaceVy, 0.052f, Ten },
        { "mid",       0.4538f, layout::kFaceY, layout::kFaceVy, 0.052f, Ten },
        { "treble",    0.5195f, layout::kFaceY, layout::kFaceVy, 0.052f, Ten },
        { "presence",  0.5846f, layout::kFaceY, layout::kFaceVy, 0.052f, Ten },
        { "master",    0.6497f, layout::kFaceY, layout::kFaceVy, 0.052f, Ten },
        { "resonance", 0.7148f, layout::kFaceY, layout::kFaceVy, 0.052f, Ten },  // "DEPTH"
        // Top row (input / gate / output) — dB params (baked knobs are a touch larger)
        { "input",     0.0990f, 0.196f, layout::kTopVy, 0.060f, Decibels },
        { "gate",      0.1999f, 0.196f, layout::kTopVy, 0.060f, Decibels },
        { "output",    0.7786f, 0.196f, layout::kTopVy, 0.060f, Decibels },
    };

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.pid = defs[i].pid;
        k.nx = defs[i].nx; k.ny = defs[i].ny; k.nvy = defs[i].nvy; k.nd = defs[i].nd;
        k.style = defs[i].style;

        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                      juce::MathConstants<float>::pi * 2.75f, true);
        k.slider.setVelocityBasedMode (true);
        k.slider.setVelocityModeParameters (0.4, 1, 0.09, false);
        k.slider.setDoubleClickReturnValue (true, k.slider.getValue());
        k.slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (k.slider);
        k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    p.apvts, k.pid, k.slider);
    }

    addAndMakeVisible (brightSwitch);
    addAndMakeVisible (bypassSwitch);
    addAndMakeVisible (inModeSwitch);
    addAndMakeVisible (outModeSwitch);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    // Preset stepper click zones (the < and v chevrons flanking the name pill)
    presetPrevZone = { 0.330f, 0.020f, 0.030f, 0.050f };
    presetNextZone = { 0.560f, 0.020f, 0.035f, 0.050f };

    // Lock the window to the artwork's 3:2 aspect ratio.
    setResizable (true, true);
    setResizeLimits (960, 640, 1920, 1280);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (kRefW / kRefH);
    setSize (1200, 800);

    startTimerHz (24);
}

DeanAmpEditor::~DeanAmpEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

juce::Rectangle<int> DeanAmpEditor::scaledRect (float nx, float ny, float nw, float nh) const
{
    return juce::Rectangle<int> (juce::roundToInt (nx * getWidth()),
                                 juce::roundToInt (ny * getHeight()),
                                 juce::roundToInt (nw * getWidth()),
                                 juce::roundToInt (nh * getHeight()));
}

juce::String DeanAmpEditor::presetName() const
{
    const int ch = (int) proc.apvts.getRawParameterValue ("channel")->load();
    switch (ch)
    {
        case 0:  return "SPECTRE CLEAN";
        case 2:  return "SPECTRE LEAD";
        default: return "SPECTRE CRUNCH";
    }
}

void DeanAmpEditor::resized()
{
    const float W = (float) getWidth(), H = (float) getHeight();

    // Cache the background scaled to the current size (fast blits in paint()).
    if (background.isValid() && getWidth() > 0 && getHeight() > 0)
        scaledBg = background.rescaled (getWidth(), getHeight(),
                                        juce::Graphics::highResamplingQuality);

    for (auto& k : knobs)
    {
        const float box = k.nd * W;
        auto r = juce::Rectangle<float> (box, box)
                     .withCentre ({ k.nx * W, k.ny * H });
        k.slider.setBounds (r.toNearestInt());
    }

    // Bright / Bypass bat switches (cover baked switch + OFF state; title baked).
    brightSwitch.setBounds (scaledRect (0.760f - 0.020f, 0.679f, 0.040f, 0.066f));
    bypassSwitch.setBounds (scaledRect (0.799f - 0.020f, 0.679f, 0.040f, 0.066f));

    // STEREO/MONO slide pills.
    inModeSwitch .setBounds (scaledRect (0.270f - 0.018f, 0.188f, 0.036f, 0.022f));
    outModeSwitch.setBounds (scaledRect (0.883f - 0.018f, 0.188f, 0.036f, 0.022f));

    // Edge level meters.
    inMeter .setBounds (scaledRect (0.0395f, 0.146f, 0.011f, 0.105f));
    outMeter.setBounds (scaledRect (0.9495f, 0.146f, 0.011f, 0.105f));
}

void DeanAmpEditor::paint (juce::Graphics& g)
{
    // 1) Photoreal background
    if (scaledBg.isValid())
        g.drawImageAt (scaledBg, 0, 0);
    else
        g.fillAll (juce::Colour (0xff0a0a0c));

    const float W = (float) getWidth(), H = (float) getHeight();
    const float faceBlackAlpha = 1.0f;
    const juce::Colour faceBlack (0xff0a0a0c);

    auto drawValue = [&] (const Knob& k)
    {
        const float v = proc.apvts.getRawParameterValue (k.pid)->load();
        juce::String text = (k.style == Ten) ? juce::String (v * 10.0f, 1)
                                              : juce::String (v, 1);

        const float fontPx = 0.0135f * H;
        auto cx = k.nx * W;
        auto cy = k.nvy * H;
        auto chip = juce::Rectangle<float> (0.055f * W, fontPx * 1.5f).withCentre ({ cx, cy });

        // Tiny faceplate-matched chip to hide the baked value underneath.
        g.setColour (faceBlack.withAlpha (faceBlackAlpha));
        g.fillRoundedRectangle (chip, 2.0f);

        g.setColour (juce::Colour (0xffcfd2d6));
        g.setFont (DeanLookAndFeel::getDisplayFont (fontPx, false, 0.04f));
        g.drawText (text, chip, juce::Justification::centred);
    };

    for (auto& k : knobs)
        drawValue (k);

    // 2) Preset name — drawn live on the cleaned artwork (no backing chips).
    //    Top pill (light, centred over the baked pill) + faceplate plate (gold,
    //    two lines next to the red status LED).
    {
        const auto name = presetName();

        // Top pill text — centred on the baked pill (≈ x 0.457 of the art).
        auto pill = scaledRect (0.380f, 0.018f, 0.155f, 0.052f).toFloat();
        g.setColour (juce::Colour (0xffe8eaee));
        g.setFont (DeanLookAndFeel::getDisplayFont (0.018f * H, true, 0.10f));
        g.drawText (name, pill, juce::Justification::centred);

        // Faceplate plate text (next to the red status LED), two lines, gold.
        auto plate = scaledRect (0.245f, 0.662f, 0.082f, 0.066f).toFloat();
        g.setColour (DeanLookAndFeel::kGold);
        g.setFont (DeanLookAndFeel::getDisplayFont (0.0145f * H, true, 0.05f));
        auto firstSpace = name.indexOfChar (' ');
        if (firstSpace > 0)
        {
            auto top = plate.removeFromTop (plate.getHeight() * 0.5f);
            g.drawText (name.substring (0, firstSpace), top, juce::Justification::centredLeft);
            g.drawText (name.substring (firstSpace + 1), plate, juce::Justification::centredLeft);
        }
        else
        {
            g.drawText (name, plate, juce::Justification::centredLeft);
        }
    }
}

void DeanAmpEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position;
    const float nx = p.x / (float) getWidth();
    const float ny = p.y / (float) getHeight();

    auto in = [&] (juce::Rectangle<float> z) { return z.contains (nx, ny); };

    if (auto* param = proc.apvts.getParameter ("channel"))
    {
        const int num = 3;
        int cur = (int) proc.apvts.getRawParameterValue ("channel")->load();
        int next = cur;
        if (in (presetPrevZone)) next = (cur + num - 1) % num;
        else if (in (presetNextZone)) next = (cur + 1) % num;

        if (next != cur)
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 ((float) next));
            param->endChangeGesture();
            repaint();
        }
    }
}

void DeanAmpEditor::timerCallback()
{
    // Repaint readouts only when a displayed value actually changes.
    juce::String hash = presetName();
    for (auto& k : knobs)
        hash << ':' << juce::String (proc.apvts.getRawParameterValue (k.pid)->load(), 3);

    if (hash != lastValueHash)
    {
        lastValueHash = hash;
        repaint();
    }
}

} // namespace deanamp
