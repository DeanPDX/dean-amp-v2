#include "PluginEditor.h"

namespace deanamp
{

DeanAmpEditor::DeanAmpEditor (DeanAmpProcessor& p)
    : juce::AudioProcessorEditor (&p),
      proc (p),
      kGate     (p.apvts, "gate",      "Gate"),
      kInput    (p.apvts, "input",     "Input"),
      kGain     (p.apvts, "gain",      "Gain"),
      kBass     (p.apvts, "bass",      "Bass"),
      kMid      (p.apvts, "mid",       "Mid"),
      kTreble   (p.apvts, "treble",    "Treble"),
      kPresence (p.apvts, "presence",  "Presence"),
      kResonance(p.apvts, "resonance", "Resonance"),
      kMaster   (p.apvts, "master",    "Master"),
      kOutput   (p.apvts, "output",    "Output"),
      kMic      (p.apvts, "mic",       "Mic"),
      brightBtn (p.apvts, "bright",    "Bright"),
      cabBypassBtn (p.apvts, "cabBypass", "Cab Off")
{
    setLookAndFeel (&lf);

    for (auto* k : { &kGate, &kInput, &kGain, &kBass, &kMid, &kTreble,
                     &kPresence, &kResonance, &kMaster, &kOutput, &kMic })
        addAndMakeVisible (k);

    auto setupBox = [this] (juce::ComboBox& box, const juce::StringArray& items)
    {
        box.addItemList (items, 1);
        box.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (box);
    };

    setupBox (channelBox, { "CLEAN", "CRUNCH", "LEAD" });
    setupBox (voicingBox, { "BRITISH", "AMERICAN" });
    setupBox (cabBox,     { "4x12 GREENBACK", "4x12 V30", "2x12 AMERICAN", "1x12 TWEED" });

    channelAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "channel", channelBox);
    voicingAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "voicing", voicingBox);
    cabAtt     = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "cab",     cabBox);

    addAndMakeVisible (brightBtn);
    addAndMakeVisible (cabBypassBtn);

    setResizable (true, true);
    setResizeLimits (760, 340, 1520, 600);
    setSize (940, 380);
}

DeanAmpEditor::~DeanAmpEditor()
{
    setLookAndFeel (nullptr);
}

void DeanAmpEditor::drawFaceplate (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    // Background — deep gradient
    juce::ColourGradient bgGrad (DeanLookAndFeel::kPanelTop,    r.getCentreX(), r.getY(),
                                 DeanLookAndFeel::kPanelBottom, r.getCentreX(), r.getBottom(), false);
    g.setGradientFill (bgGrad);
    g.fillRect (r);

    // Subtle brushed-metal texture
    juce::Random rng (4242);
    g.setColour (juce::Colours::black.withAlpha (0.06f));
    for (int y = 0; y < (int) r.getHeight(); y += 2)
    {
        float jx = rng.nextFloat() * 1.5f;
        g.drawHorizontalLine (y, r.getX() + jx, r.getRight() - jx);
    }

    // Faint vignette
    juce::ColourGradient vig (juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                              juce::Colour (0xff000000).withAlpha (0.40f),
                              r.getX(), r.getY(), true);
    vig.isRadial = true;
    g.setGradientFill (vig);
    g.fillRect (r);

    // Top accent stripe (the "chassis" rivet bar)
    auto top = r.removeFromTop (44.0f).reduced (12.0f, 6.0f);
    DeanLookAndFeel::drawBrushedMetalPanel (g, top, DeanLookAndFeel::kMetalLight,
                                            DeanLookAndFeel::kMetalDark, 8.0f);

    // Bottom accent stripe
    auto fullR = getLocalBounds().toFloat();
    auto bot = fullR.removeFromBottom (28.0f).reduced (12.0f, 4.0f);
    DeanLookAndFeel::drawBrushedMetalPanel (g, bot, DeanLookAndFeel::kMetalDark,
                                            juce::Colour (0xff050608), 6.0f);
}

void DeanAmpEditor::drawBranding (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto top = r.removeFromTop (44.0f);

    // Power LED at the right of the brand bar
    {
        auto led = juce::Rectangle<float> (12.0f, 12.0f)
                       .withCentre ({ top.getRight() - 26.0f, top.getCentreY() });
        g.setColour (DeanLookAndFeel::kAccentGlow.withAlpha (0.6f));
        g.fillEllipse (led.expanded (4.0f));
        juce::ColourGradient rg (juce::Colour (0xffffd5dc), led.getCentreX(), led.getCentreY(),
                                 DeanLookAndFeel::kAccentCrimson, led.getRight(), led.getBottom(), true);
        rg.isRadial = true;
        g.setGradientFill (rg);
        g.fillEllipse (led);

        g.setColour (DeanLookAndFeel::kLabelDim);
        g.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
        g.drawText ("PWR", led.translated (-32.0f, 0.0f).withWidth (28.0f),
                    juce::Justification::centredRight);
    }

    // Logo: "DEAN AMP" engraved
    auto brandArea = top.reduced (24.0f, 6.0f);
    juce::Font brand = juce::Font (juce::FontOptions (22.0f).withStyle ("Bold"))
                          .withExtraKerningFactor (0.18f);
    DeanLookAndFeel::drawEngravedText (g, "DEAN  AMP", brandArea, brand,
                                       juce::Justification::centredLeft);

    // Tagline
    juce::Font tag = juce::Font (juce::FontOptions (9.5f).withStyle ("Bold"))
                         .withExtraKerningFactor (0.30f);
    g.setColour (DeanLookAndFeel::kAccentCrimson);
    g.drawText ("TUBE GUITAR AMPLIFIER", brandArea.translated (175.0f, 6.0f),
                juce::Justification::centredLeft);
}

void DeanAmpEditor::drawSectionBackings (juce::Graphics& g)
{
    auto roundedSection = [&] (juce::Rectangle<int> bounds, const juce::String& title)
    {
        auto r = bounds.toFloat();
        DeanLookAndFeel::drawBrushedMetalPanel (g, r, DeanLookAndFeel::kMetalDark,
                                                juce::Colour (0xff090a0d), 10.0f);
        // Top section label
        auto labelArea = r.removeFromTop (22.0f).reduced (8.0f, 2.0f);
        juce::Font f = juce::Font (juce::FontOptions (11.0f).withStyle ("Bold"))
                           .withExtraKerningFactor (0.28f);
        g.setColour (DeanLookAndFeel::kAccentCrimson);
        g.setFont (f);
        g.drawText (title.toUpperCase(), labelArea, juce::Justification::centredLeft);

        // Underline
        g.setColour (DeanLookAndFeel::kAccentCrimson.withAlpha (0.5f));
        const float ly = labelArea.getBottom();
        g.drawLine (labelArea.getX(), ly, labelArea.getX() + 28.0f, ly, 1.5f);
    };

    roundedSection (preampSection, "Preamp");
    roundedSection (eqSection,     "EQ");
    roundedSection (powerSection,  "Power");
    roundedSection (cabSection,    "Cabinet");
}

void DeanAmpEditor::paint (juce::Graphics& g)
{
    drawFaceplate (g);
    drawSectionBackings (g);
    drawBranding (g);
}

void DeanAmpEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (44);   // brand bar
    area.removeFromBottom (28); // bottom bar
    area.reduce (12, 8);

    // Top row: channel selector + voicing + bright switch (left of preamp)
    auto headerRow = area.removeFromTop (32);
    auto leftHeader  = headerRow.removeFromLeft (area.getWidth() / 2);
    channelBox.setBounds (leftHeader.removeFromLeft (160).reduced (2));
    leftHeader.removeFromLeft (8);
    brightBtn.setBounds  (leftHeader.removeFromLeft (110).reduced (2));

    auto rightHeader = headerRow;
    cabBox.setBounds      (rightHeader.removeFromLeft (200).reduced (2));
    rightHeader.removeFromLeft (8);
    voicingBox.setBounds  (rightHeader.removeFromLeft (140).reduced (2));
    rightHeader.removeFromLeft (8);
    cabBypassBtn.setBounds (rightHeader.removeFromLeft (110).reduced (2));

    area.removeFromTop (8);

    // The four sections sit in a 4-column grid below the header row.
    // Widths roughly: preamp 25%, eq 30%, power 25%, cab 20%.
    const int total = area.getWidth();
    const int preampW = juce::roundToInt (total * 0.25f);
    const int eqW     = juce::roundToInt (total * 0.30f);
    const int powerW  = juce::roundToInt (total * 0.25f);
    const int cabW    = total - preampW - eqW - powerW;
    const int gap = 8;

    preampSection = area.removeFromLeft (preampW).reduced (0, 0);
    area.removeFromLeft (gap);
    eqSection     = area.removeFromLeft (eqW).reduced (0, 0);
    area.removeFromLeft (gap);
    powerSection  = area.removeFromLeft (powerW).reduced (0, 0);
    area.removeFromLeft (gap);
    cabSection    = area.removeFromLeft (cabW).reduced (0, 0);

    auto layoutKnobs = [] (juce::Rectangle<int> section, std::vector<juce::Component*> knobs)
    {
        section.removeFromTop (26); // section title
        section.reduce (8, 6);
        const int n = (int) knobs.size();
        if (n == 0) return;
        // Center the knob row vertically; cap height so knobs don't get goofy-tall
        const int knobH = juce::jmin (section.getHeight(), 200);
        section = section.withSizeKeepingCentre (section.getWidth(), knobH);
        const int kw = section.getWidth() / n;
        for (int i = 0; i < n; ++i)
        {
            auto col = section.removeFromLeft (kw);
            knobs[(size_t) i]->setBounds (col.reduced (4));
        }
    };

    layoutKnobs (preampSection, { &kGate, &kInput, &kGain });
    layoutKnobs (eqSection,     { &kBass, &kMid, &kTreble });
    layoutKnobs (powerSection,  { &kPresence, &kResonance, &kMaster });
    layoutKnobs (cabSection,    { &kMic, &kOutput });
}

} // namespace deanamp
