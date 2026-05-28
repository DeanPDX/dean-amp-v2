#pragma once
#include "PluginProcessor.h"
#include "ui/DeanLookAndFeel.h"
#include "ui/LevelMeter.h"
#include "ui/ToggleSwitch.h"
#include "ui/SlideToggle.h"

namespace deanamp
{

/**
 * Photoreal editor. Paints the rendered amp-head image (embedded as BinaryData)
 * as the full background, then overlays live, interactive controls at the exact
 * positions of the elements baked into that render. All positions are stored as
 * fractions of the 1536x1024 reference art, so the whole face scales cleanly.
 */
class DeanAmpEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit DeanAmpEditor (DeanAmpProcessor&);
    ~DeanAmpEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::String presetName() const;
    juce::Rectangle<int> scaledRect (float nx, float ny, float nw, float nh) const;

    DeanAmpProcessor& proc;
    DeanLookAndFeel   lf;

    juce::Image background;
    juce::Image scaledBg;          // cached, regenerated on resize

    // --- Rotary controls ----------------------------------------------------
    enum ValueStyle { Ten, Decibels };
    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        juce::String pid;
        float nx = 0.0f, ny = 0.0f;   // normalized centre of the knob
        float nvy = 0.0f;             // normalized centre-y of the value readout
        float nd = 0.052f;            // normalized knob box size (fraction of width)
        ValueStyle style = Ten;
    };
    std::array<Knob, 10> knobs;       // gain..depth, input, gate, output

    // --- Switches -----------------------------------------------------------
    ToggleSwitch brightSwitch, bypassSwitch;
    SlideToggle  inModeSwitch, outModeSwitch;

    // --- Meters -------------------------------------------------------------
    LevelMeter inMeter, outMeter;

    // Click zones (normalized) for the preset stepper in the top bar.
    juce::Rectangle<float> presetPrevZone, presetNextZone;

    juce::String lastValueHash;       // repaint only when readouts change

    static constexpr float kRefW = 1536.0f;
    static constexpr float kRefH = 1024.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeanAmpEditor)
};

} // namespace deanamp
