#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace deanamp
{

/**
 * Custom LookAndFeel for Dean Amp. Aims for the NeuralDSP "premium plugin" feel:
 *
 *   - Dark, near-black base with a subtle vertical brushed-metal sheen
 *   - Anodized accent (deep crimson) for active LEDs and the indicator arc
 *   - Knobs have:
 *       * a recessed shadow well underneath
 *       * a machined-aluminum body with a soft radial highlight
 *       * a glowing crimson arc showing the current value
 *       * a thin engraved indicator line on top
 *   - Sharp, condensed typography for labels
 */
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

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

    // Helpers used by the editor
    static void drawBrushedMetalPanel (juce::Graphics& g, juce::Rectangle<float> r,
                                       juce::Colour top, juce::Colour bottom,
                                       float cornerRadius = 6.0f);
    static void drawEngravedText (juce::Graphics& g, const juce::String& text,
                                  juce::Rectangle<float> bounds, juce::Font font,
                                  juce::Justification just = juce::Justification::centred);
};

} // namespace deanamp
