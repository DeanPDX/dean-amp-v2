#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace deanamp
{

/**
 * Procedural "amp head" chassis background. Draws tolex, gold piping, a brand
 * plaque, and the faceplate strip. Returns the faceplate rect via
 * `getFaceplateBounds()` so the editor can lay out knobs/toggles on top.
 *
 * This is a pure painter — it does not own any child controls.
 */
class AmpChassis : public juce::Component
{
public:
    AmpChassis();
    void paint (juce::Graphics&) override;
    void resized() override;

    // After resized(), call this to find where to place faceplate controls.
    juce::Rectangle<int> getFaceplateBounds() const   { return faceplateBounds; }
    juce::Rectangle<int> getStatusLedBounds() const   { return statusLedBounds; }
    juce::Rectangle<int> getPresetNameBounds() const  { return presetNameBounds; }

    void setPresetName (const juce::String& n) { presetName = n; repaint(); }

private:
    juce::Rectangle<int> faceplateBounds;
    juce::Rectangle<int> statusLedBounds;
    juce::Rectangle<int> presetNameBounds;
    juce::String presetName { "CRUNCH" };

    void layoutInternals();
};

} // namespace deanamp
