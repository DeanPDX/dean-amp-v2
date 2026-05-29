// DI test/analysis harness.
//
// Loads a real guitar DI from a WAV (default /tmp/deanamp/di.wav, or argv[1]),
// or synthesizes a realistic-LEVEL DI if none is present, then renders it
// through DeanAmpProcessor for each channel and prints level / balance / width /
// sanity stats. This is the rig we use to calibrate channel loudness against a
// real signal (the synthetic tone-test signal is far too hot to balance against).

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../source/PluginProcessor.h"
#include <cmath>

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int    kBlock = 512;

    float rmsDb (const juce::AudioBuffer<float>& b)
    {
        double sum = 0.0; int n = 0;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i) { const float v = b.getSample (ch, i); sum += v * (double) v; ++n; }
        return (float) juce::Decibels::gainToDecibels (std::sqrt (sum / juce::jmax (1, n)), -120.0);
    }
    float peakDb (const juce::AudioBuffer<float>& b)
    {
        return juce::Decibels::gainToDecibels (b.getMagnitude (0, b.getNumSamples()), -120.0f);
    }

    // Realistic-level synthetic DI: peaks ~ -16 dBFS, with dynamics (plucks,
    // a chord, palm-mute chugs, a sustained note).
    void synthDI (juce::AudioBuffer<float>& buf)
    {
        const int N = buf.getNumSamples();
        auto* d = buf.getWritePointer (0);
        auto note = [] (float t, float f, float amp, float decay)
        {
            const float env = std::exp (-t * decay);
            return amp * env * (std::sin (juce::MathConstants<float>::twoPi * f * t)
                              + 0.4f * std::sin (juce::MathConstants<float>::twoPi * 2 * f * t)
                              + 0.2f * std::sin (juce::MathConstants<float>::twoPi * 3 * f * t));
        };
        for (int i = 0; i < N; ++i)
        {
            const float t = (float) i / (float) kSr;
            float s = 0.0f;
            if      (t < 1.0f) s = note (t,        82.41f, 0.16f, 3.0f);                 // low E pluck
            else if (t < 2.0f) { float u=t-1; for (float f:{82.41f,123.5f,164.8f,207.7f}) s += note(u,f,0.05f,2.0f); } // chord
            else if (t < 3.0f) { float u=std::fmod(t,0.25f); s = note(u,98.0f,0.18f,18.0f); }   // palm-mute chugs
            else               s = note (t-3.0f, 329.6f, 0.12f, 1.2f);                  // sustained high note
            d[i] = s;
        }
    }

    bool loadWav (const juce::File& f, juce::AudioBuffer<float>& out)
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
        if (r == nullptr) return false;
        const int n = (int) r->lengthInSamples;
        juce::AudioBuffer<float> tmp ((int) r->numChannels, n);
        r->read (&tmp, 0, n, 0, true, true);
        out.setSize (1, n);
        out.clear();
        for (int ch = 0; ch < tmp.getNumChannels(); ++ch)
            out.addFrom (0, 0, tmp, ch, 0, n, 1.0f / (float) tmp.getNumChannels());
        return true;
    }

    void setChoice (deanamp::DeanAmpProcessor& p, const juce::String& id, int idx)
    {
        if (auto* par = p.apvts.getParameter (id))
            par->setValueNotifyingHost (par->convertTo0to1 ((float) idx));
    }
    void setVal (deanamp::DeanAmpProcessor& p, const juce::String& id, float norm)
    {
        if (auto* par = p.apvts.getParameter (id))
            par->setValueNotifyingHost (norm);
    }

    struct ChannelStats { float peakDbV, activeRmsDb, corr; bool sane; };

    // RMS of the "played" portion only (samples above -45 dBFS), so silent gaps
    // between phrases don't skew the loudness measure — that's the metric that
    // actually reflects perceived level while playing.
    float activeRmsDb (const juce::AudioBuffer<float>& b)
    {
        const float thr = juce::Decibels::decibelsToGain (-45.0f);
        double sum = 0.0; long long cnt = 0;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const float v = b.getSample (ch, i);
                if (std::abs (v) > thr) { sum += v * (double) v; ++cnt; }
            }
        return cnt > 0 ? (float) juce::Decibels::gainToDecibels (std::sqrt (sum / (double) cnt), -120.0)
                       : -120.0f;
    }

    ChannelStats runChannel (deanamp::DeanAmpProcessor& p, int chan, const char* label,
                             const juce::AudioBuffer<float>& di, const juce::File& outDir)
    {
        // Neutral settings: knobs at noon, gate open, unity output.
        setChoice (p, "channel", chan);
        setChoice (p, "cab", 1); setChoice (p, "voicing", 0); setChoice (p, "mic", 0);
        for (auto id : { "gain","bass","mid","treble","presence","resonance","master" }) setVal (p, id, 0.5f);
        // input/output 0 dB on a -12..12 / -36..12 range; gate fully open at -80
        if (auto* par = p.apvts.getParameter ("input"))  par->setValueNotifyingHost (par->convertTo0to1 (0.0f));
        if (auto* par = p.apvts.getParameter ("output")) par->setValueNotifyingHost (par->convertTo0to1 (0.0f));
        if (auto* par = p.apvts.getParameter ("gate"))   par->setValueNotifyingHost (par->convertTo0to1 (-80.0f));

        const int N = di.getNumSamples();
        juce::AudioBuffer<float> buf (2, N);
        for (int ch = 0; ch < 2; ++ch) buf.copyFrom (ch, 0, di, 0, 0, N);

        juce::MidiBuffer midi;
        juce::AudioBuffer<float> work (2, kBlock);
        for (int start = 0; start < N; start += kBlock)
        {
            const int n = juce::jmin (kBlock, N - start);
            work.setSize (2, n, false, false, true);
            for (int ch = 0; ch < 2; ++ch) work.copyFrom (ch, 0, buf, ch, start, n);
            p.processBlock (work, midi);
            for (int ch = 0; ch < 2; ++ch) buf.copyFrom (ch, start, work, ch, 0, n);
        }

        // Sanity: NaN/Inf check.
        bool sane = true;
        for (int ch = 0; ch < 2 && sane; ++ch)
            for (int i = 0; i < N; ++i)
                if (! std::isfinite (buf.getSample (ch, i))) { sane = false; break; }

        // Width: L/R correlation.
        double sLL=0,sRR=0,sLR=0;
        for (int i = 0; i < N; ++i) { float L=buf.getSample(0,i),R=buf.getSample(1,i); sLL+=L*L; sRR+=R*R; sLR+=L*R; }
        const double corr = sLR / (std::sqrt(sLL*sRR)+1e-12);

        const float aRms = activeRmsDb (buf);
        std::cout << juce::String (label).paddedRight (' ', 8)
                  << " play-loud=" << juce::String (aRms, 1) << "dB"
                  << "  peak=" << juce::String (peakDb (buf), 1) << "dB"
                  << "  L/Rcorr=" << juce::String ((float) corr, 3)
                  << (sane ? "  [ok]" : "  [!! NaN/Inf]") << "\n";

        auto outFile = outDir.getChildFile (juce::String ("di_") + juce::String (label).toLowerCase() + ".wav");
        juce::WavAudioFormat fmt;
        if (auto* stream = outFile.createOutputStream().release())
        {
            std::unique_ptr<juce::AudioFormatWriter> writer (fmt.createWriterFor (stream, kSr, 2, 24, {}, 0));
            if (writer) writer->writeFromAudioSampleBuffer (buf, 0, N);
        }
        return { peakDb (buf), aRms, (float) corr, sane };
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::File outDir ("/tmp/deanamp"); outDir.createDirectory();

   #ifdef DEANAMP_DI_FIXTURE
    const juce::String defaultDi (DEANAMP_DI_FIXTURE);
   #else
    const juce::String defaultDi ("/tmp/deanamp/di.wav");
   #endif

    juce::File diFile (argc > 1 ? juce::String (argv[1]) : defaultDi);
    juce::AudioBuffer<float> di;
    if (loadWav (diFile, di))
        std::cout << "Loaded DI: " << diFile.getFullPathName() << "  (" << di.getNumSamples() << " samples)\n";
    else
    {
        di.setSize (1, (int) (kSr * 4.0));
        synthDI (di);
        std::cout << "No DI at " << diFile.getFullPathName() << " — using synthetic realistic-level DI.\n";
    }
    std::cout << "DI input level: rms=" << juce::String (rmsDb (di), 1)
              << "dB  peak=" << juce::String (peakDb (di), 1) << "dB\n\n";

    deanamp::DeanAmpProcessor p;
    p.prepareToPlay (kSr, kBlock);

    const ChannelStats s[3] = {
        runChannel (p, 0, "Clean",  di, outDir),
        runChannel (p, 1, "Crunch", di, outDir),
        runChannel (p, 2, "Lead",   di, outDir),
    };

    std::cout << "\nWrote di_*.wav to " << outDir.getFullPathName() << "\n";

    // ---- Assertions (regression guards) ------------------------------------
    int failures = 0;
    auto check = [&] (bool ok, const juce::String& msg)
    {
        std::cout << (ok ? "  PASS  " : "  FAIL  ") << msg << "\n";
        if (! ok) ++failures;
    };
    std::cout << "\nChecks:\n";

    for (int i = 0; i < 3; ++i)
    {
        const char* nm = (i == 0 ? "Clean" : i == 1 ? "Crunch" : "Lead");
        check (s[i].sane,               juce::String (nm) + ": finite output (no NaN/Inf)");
        check (s[i].peakDbV < -0.1f,    juce::String (nm) + ": no full-scale clipping (peak " + juce::String (s[i].peakDbV,1) + " dB)");
        check (s[i].activeRmsDb > -55.f,juce::String (nm) + ": not effectively silent (play-loud " + juce::String (s[i].activeRmsDb,1) + " dB)");
    }

    // Channel loudness balance: all three within 8 dB of each other.
    float lo = 1e9f, hi = -1e9f;
    for (auto& st : s) { lo = juce::jmin (lo, st.activeRmsDb); hi = juce::jmax (hi, st.activeRmsDb); }
    check (hi - lo <= 8.0f, "Channel balance within 8 dB (spread " + juce::String (hi - lo, 1) + " dB)");

    // Stereo cab produces width (L/R not perfectly correlated) on the driven channels.
    check (s[1].corr < 0.99f, "Crunch has stereo width (corr " + juce::String (s[1].corr,3) + ")");
    check (s[2].corr < 0.97f, "Lead has stereo width (corr "   + juce::String (s[2].corr,3) + ")");

    std::cout << (failures == 0 ? "\nALL CHECKS PASSED\n" : "\n" + juce::String (failures) + " CHECK(S) FAILED\n");
    juce::ignoreUnused (argc, argv);
    return failures == 0 ? 0 : 1;
}
