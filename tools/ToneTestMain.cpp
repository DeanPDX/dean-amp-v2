// Tone-test harness: render a test signal through DeanAmpProcessor and write WAV files
// for each channel (Clean/Crunch/Lead). Also prints peak/RMS so we can sanity-check levels.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../source/PluginProcessor.h"

namespace
{
    void fillTestSignal (juce::AudioBuffer<float>& buf, double sr)
    {
        const int n = buf.getNumSamples();
        const int numCh = buf.getNumChannels();

        // 3 seconds: 1s sine sweep, 1s "chord" (3 sines), 1s decaying note
        const int s1 = (int) sr;
        const int s2 = s1 * 2;

        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) sr;
            float s = 0.0f;

            if (i < s1)
            {
                // Log sweep 80 Hz → 5 kHz
                const float f = 80.0f * std::pow (5000.0f / 80.0f, t);
                s = 0.4f * std::sin (juce::MathConstants<float>::twoPi * f * t);
            }
            else if (i < s2)
            {
                // E-minor chord (E2, B2, E3, G3)
                const float u = t - 1.0f;
                s = 0.0f;
                for (float f : { 82.4f, 123.5f, 164.8f, 196.0f })
                    s += 0.10f * std::sin (juce::MathConstants<float>::twoPi * f * u);
            }
            else
            {
                // Decaying high-E (E4)
                const float u = t - 2.0f;
                const float env = std::exp (-u * 1.4f);
                s = 0.6f * env * std::sin (juce::MathConstants<float>::twoPi * 329.63f * u);
                // Add a touch of 2nd + 3rd harmonic to imitate guitar pickup
                s += 0.2f * env * std::sin (juce::MathConstants<float>::twoPi * 659.26f * u);
                s += 0.08f * env * std::sin (juce::MathConstants<float>::twoPi * 988.89f * u);
            }

            for (int ch = 0; ch < numCh; ++ch)
                buf.setSample (ch, i, s);
        }
    }

    void runChannel (deanamp::DeanAmpProcessor& p, int channelIdx,
                     const juce::String& label,
                     const juce::File& outDir, double sr, int blockSize)
    {
        // Set channel parameter
        if (auto* par = p.getParameters()[0]) {} // ensure params loaded
        if (auto* paramChan = p.apvts.getParameter ("channel"))
            paramChan->setValueNotifyingHost (paramChan->convertTo0to1 ((float) channelIdx));

        const int totalSamples = (int) (sr * 3.0);
        juce::AudioBuffer<float> buf (2, totalSamples);
        fillTestSignal (buf, sr);

        // Process in blocks
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> work (2, blockSize);
        for (int start = 0; start < totalSamples; start += blockSize)
        {
            const int n = juce::jmin (blockSize, totalSamples - start);
            work.setSize (2, n, false, false, true);
            for (int ch = 0; ch < 2; ++ch)
                work.copyFrom (ch, 0, buf, ch, start, n);
            p.processBlock (work, midi);
            for (int ch = 0; ch < 2; ++ch)
                buf.copyFrom (ch, start, work, ch, 0, n);
        }

        // Stats
        float peak = 0.0f, sumSq = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < totalSamples; ++i)
            {
                const float v = buf.getSample (ch, i);
                peak = std::max (peak, std::abs (v));
                sumSq += v * v;
            }
        const float rms = std::sqrt (sumSq / (float) (totalSamples * 2));

        std::cout << label
                  << "  peak=" << juce::Decibels::gainToDecibels (peak, -120.0f) << " dB"
                  << "  rms="  << juce::Decibels::gainToDecibels (rms,  -120.0f) << " dB\n";

        // Write WAV
        auto outFile = outDir.getChildFile ("dean_" + label.toLowerCase() + ".wav");
        outFile.deleteFile(); // FileOutputStream appends to existing files
        juce::WavAudioFormat fmt;
        if (auto* stream = outFile.createOutputStream().release())
        {
            std::unique_ptr<juce::AudioFormatWriter> writer (
                fmt.createWriterFor (stream, sr, 2, 24, {}, 0));
            if (writer)
                writer->writeFromAudioSampleBuffer (buf, 0, totalSamples);
        }
    }

}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    juce::File outDir ("/tmp/deanamp");
    outDir.createDirectory();

    const double sr = 48000.0;
    const int blockSize = 512;

    deanamp::DeanAmpProcessor p;
    p.prepareToPlay (sr, blockSize);

    // Warm up the cab convolver: the IR loads on a background thread, so an
    // offline render would otherwise start with the cab dry for ~0.5 s.
    {
        juce::AudioBuffer<float> warm (2, blockSize);
        juce::MidiBuffer midi;
        for (int i = 0; i < 50; ++i)
        {
            warm.clear();
            p.processBlock (warm, midi);
            if (i % 10 == 0) juce::Thread::sleep (20);
        }
    }

    const char* channelLabels[3] = { "Clean", "Crunch", "Lead" };
    for (int ch = 0; ch < 3; ++ch)
        runChannel (p, ch, channelLabels[ch], outDir, sr, blockSize);

    std::cout << "Wrote test renders to " << outDir.getFullPathName() << "\n";
    juce::ignoreUnused (argc, argv);
    return 0;
}
