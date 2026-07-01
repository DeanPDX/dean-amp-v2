// Stage-by-stage probe: feeds a steady 196 Hz sine (realistic DI level) through
// the DSP chain and prints an envelope + DC table tapped after every stage.
// Pinpoints which stage misbehaves (ducking, DC offsets, instability) without
// the cab/plugin wrapper obscuring things.

#include <juce_dsp/juce_dsp.h>
#include "../source/dsp/NoiseGate.h"
#include "../source/dsp/TubeStage.h"
#include "../source/dsp/ToneStack.h"
#include "../source/dsp/PowerAmp.h"
#include "../source/dsp/CabSim.h"
#include <iostream>

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int    kBlock = 512;
    constexpr float  kFreq = 196.0f;
    constexpr float  kAmp  = 0.16f;
    constexpr double kSecs = 3.0;

    struct Tap { const char* name; juce::AudioBuffer<float> buf; };

    void printTable (std::vector<Tap>& taps)
    {
        std::cout << "time ";
        for (auto& t : taps) std::cout << juce::String (t.name).paddedLeft (' ', 16);
        std::cout << "\n";
        const int N = taps[0].buf.getNumSamples();
        const int win = (int) (kSr * 0.1);
        for (int start = 0; start + win <= N; start += win)
        {
            std::cout << juce::String ((float) start / kSr, 1).paddedLeft (' ', 4);
            for (auto& t : taps)
            {
                double sum = 0.0, mean = 0.0;
                const auto* d = t.buf.getReadPointer (0);
                for (int i = start; i < start + win; ++i) { sum += d[i] * (double) d[i]; mean += d[i]; }
                const float rms = (float) juce::Decibels::gainToDecibels (std::sqrt (sum / win), -99.0);
                const float dc  = (float) (mean / win);
                std::cout << juce::String (rms, 1).paddedLeft (' ', 9)
                          << "/" << juce::String (dc, 3).paddedLeft (' ', 6);
            }
            std::cout << "\n";
        }
    }
}

int main (int argc, char** argv)
{
    const int chanIdx = argc > 1 ? juce::String (argv[1]).getIntValue() : 2; // default Lead
    const float gainKnob = argc > 2 ? juce::String (argv[2]).getFloatValue() : 0.5f;

    const int N = (int) (kSr * kSecs);

    deanamp::NoiseGate  gate;
    deanamp::TubePreamp preamp;
    deanamp::ToneStack  tone;
    deanamp::PowerAmp   power;
    deanamp::CabSim     cab;

    juce::dsp::ProcessSpec spec { kSr, (juce::uint32) kBlock, 1 };
    gate.prepare (kSr);
    gate.setThresholdDb (-80.0f);
    preamp.prepare (spec);
    preamp.setChannel ((deanamp::TubePreamp::Channel) chanIdx);
    preamp.setGain (gainKnob);
    tone.prepare (kSr);
    power.prepare (spec);

    auto cabSpec = spec; cabSpec.numChannels = 2;
    cab.prepare (cabSpec);
    cab.setCabIndex (1);

    // Warm up the convolver: the IR loads on a background thread, so an
    // offline render would otherwise start with the cab bypassed (dry).
    {
        juce::AudioBuffer<float> warm (2, kBlock);
        for (int i = 0; i < 50; ++i)
        {
            warm.clear();
            juce::dsp::AudioBlock<float> wb (warm);
            cab.process (wb);
            if (i % 10 == 0) juce::Thread::sleep (20);
        }
    }

    std::vector<Tap> taps;
    for (auto* name : { "input", "gate", "preamp", "tone", "power", "cab" })
    {
        Tap t; t.name = name; t.buf.setSize (1, N); t.buf.clear();
        taps.push_back (std::move (t));
    }

    // Steady sine with 5 ms fade-in.
    for (int i = 0; i < N; ++i)
    {
        const float t = (float) i / (float) kSr;
        const float env = juce::jmin (1.0f, t / 0.005f);
        taps[0].buf.setSample (0, i, kAmp * env * std::sin (juce::MathConstants<float>::twoPi * kFreq * t));
    }

    juce::AudioBuffer<float> work (1, kBlock);
    juce::AudioBuffer<float> stereo (2, kBlock);
    bool first = true;
    for (int start = 0; start < N; start += kBlock)
    {
        const int n = juce::jmin (kBlock, N - start);
        work.setSize (1, n, false, false, true);
        work.copyFrom (0, 0, taps[0].buf, 0, start, n);
        juce::dsp::AudioBlock<float> b (work);

        gate.process   (b); taps[1].buf.copyFrom (0, start, work, 0, 0, n);
        preamp.process (b); taps[2].buf.copyFrom (0, start, work, 0, 0, n);
        tone.process   (b); taps[3].buf.copyFrom (0, start, work, 0, 0, n);
        power.process  (b); taps[4].buf.copyFrom (0, start, work, 0, 0, n);

        // Replicate the processor's parameter push on the first block: mic =
        // Center → blend 0.0 (member default is 0.35, so this forces a reload).
        if (first) { cab.setMicBlend (0.0f); first = false; }

        stereo.setSize (2, n, false, false, true);
        for (int ch = 0; ch < 2; ++ch) stereo.copyFrom (ch, 0, work, 0, 0, n);
        juce::dsp::AudioBlock<float> sb (stereo);
        cab.process (sb);
        taps[5].buf.copyFrom (0, start, stereo, 0, 0, n);
    }

    const char* names[3] = { "Clean", "Crunch", "Lead" };
    std::cout << "Channel " << names[juce::jlimit (0, 2, chanIdx)]
              << "  gain=" << gainKnob << "  (cells: rms dB / DC)\n";
    printTable (taps);
    return 0;
}
