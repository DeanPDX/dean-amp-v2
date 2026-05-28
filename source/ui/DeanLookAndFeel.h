#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace deanamp
{

class DeanLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DeanLookAndFeel();
    ~DeanLookAndFeel() override = default;

    // Brand palette
    static const juce::Colour kPanelBlack;
    static const juce::Colour kPanelTop;
    static const juce::Colour kPanelBottom;
    static const juce::Colour kMetalDark;
    static const juce::Colour kMetalLight;
    static const juce::Colour kEdgeShadow;
    static const juce::Colour kAccentCrimson;
    static const juce::Colour kAccentGlow;
    static const juce::Colour kAccentAmber;     // signal-chain "active" glow
    static const juce::Colour kAccentTeal;      // signal-chain "module" glow
    static const juce::Colour kGold;            // amp piping / plaque
    static const juce::Colour kGoldDark;
    static const juce::Colour kGoldHighlight;
    static const juce::Colour kTolex;           // amp covering
    static const juce::Colour kTolexHighlight;
    static const juce::Colour kFaceplate;       // amp faceplate background
    static const juce::Colour kLabelText;
    static const juce::Colour kLabelDim;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics&, int width, int height,
                       bool isButtonDown, int buttonX, int buttonY,
                       int buttonW, int buttonH, juce::ComboBox&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

    // Drawing helpers
    static void drawBrushedMetalPanel (juce::Graphics& g, juce::Rectangle<float> r,
                                       juce::Colour top, juce::Colour bottom,
                                       float cornerRadius = 6.0f);

    static void drawEngravedText (juce::Graphics& g, const juce::String& text,
                                  juce::Rectangle<float> bounds, juce::Font font,
                                  juce::Justification just = juce::Justification::centred,
                                  juce::Colour baseColour = juce::Colour (0xffe6e8ec));

    static void drawTolexPanel (juce::Graphics& g, juce::Rectangle<float> r,
                                float cornerRadius = 14.0f);

    static void drawGoldPiping (juce::Graphics& g, juce::Rectangle<float> r,
                                float cornerRadius, float thickness);

    static void drawGoldPlaque (juce::Graphics& g, juce::Rectangle<float> r,
                                const juce::String& text);

    static juce::Font getDisplayFont (float size, bool bold = true,
                                      float kerning = 0.0f);
};

} // namespace deanamp
