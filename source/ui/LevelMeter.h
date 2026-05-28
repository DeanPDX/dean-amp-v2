#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

namespace deanamp
{

/**
 * Vertical level meter. Reads a peak value (linear 0..1+) from an external
 * atomic, paints a thin gradient bar with peak hold, ticks down on a timer.
 */
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    LevelMeter (std::atomic<float>& sourceLeft, std::atomic<float>& sourceRight);
    ~LevelMeter() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    std::atomic<float>& srcL;
    std::atomic<float>& srcR;

    float dispL { 0.0f }, dispR { 0.0f };
    float peakL { 0.0f }, peakR { 0.0f };
    int   peakHoldL { 0 }, peakHoldR { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
};

} // namespace deanamp
