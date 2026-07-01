#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace deanamp
{

/**
 * Single 12AX7 triode gain stage.
 *
 * Models, in signal order:
 *  - Interstage coupling cap (HP) into the grid.
 *  - Grid conduction: past the conduction knee the grid draws current and the
 *    source impedance eats the swing — a sharp knee with a logarithmic tail
 *    (the "singing" compression of a pushed stage).
 *  - Blocking distortion: grid current charges the coupling cap and drags the
 *    operating point down, recovering over ~20 ms — the spongy feel on hard
 *    digs. Depth is clamped so hot signals shift the duty cycle instead of
 *    gating themselves silent.
 *  - Asymmetric plate curve: the cutoff side has ~1.45x the headroom and a much
 *    softer knee than the conduction side — that contrast (and the duty-cycle
 *    shift it causes) is where the even-harmonic warmth comes from.
 *  - Miller capacitance (LP) after the nonlinearity (de-fizzes).
 *  - Stage inversion: a triode inverts, so cascaded stages clip ALTERNATING
 *    sides of the waveform — essential to how a cascaded preamp sounds; without
 *    it every stage flattens the same side and the tone goes homogeneous.
 */
class TubeStage
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        driftCoef = (float) std::exp (-1.0 / (0.020 * sr)); // ~20 ms recovery
        updateFilters();
        reset();
    }

    void reset()
    {
        couplingHP.reset();
        millerLP.reset();
        biasState = 0.0f;
    }

    /** biasIn > 0 biases toward grid conduction (clips the top earlier);
        biasIn < 0 biases toward cutoff — a strongly negative value makes a
        "cold clipper" (the SLO/5150-style stage behind modern high gain).
        outLevelIn is the interstage divider that follows the stage. */
    void setVoicing (float couplingHzIn, float millerHzIn, float biasIn,
                     float gainIn, float outLevelIn)
    {
        couplingHz = couplingHzIn;
        millerHz   = millerHzIn;
        bias       = biasIn;
        gain       = gainIn;
        outLevel   = outLevelIn;
        restingY   = triodeShape (biasIn);
        if (sr > 0.0)
            updateFilters();
    }

    /** Static triode transfer. Positive input heads toward grid conduction
        (sharp knee, log tail); negative toward cutoff (soft knee, ~1.45x the
        headroom). Both halves have unity slope at 0 and join C1-continuously. */
    static inline float triodeShape (float v) noexcept
    {
        if (v > kGridKnee)
            v = kGridKnee + 0.5f * std::log1p ((v - kGridKnee) * 2.0f);
        if (v >= 0.0f)
            return std::tanh (v);
        return 1.45f * std::tanh (v * (1.0f / 1.45f));
    }

    inline float processSample (float x) noexcept
    {
        x = couplingHP.processSample (x);
        float v = x * gain + bias;

        // Blocking distortion (bias drift), clamped via tanh so it can shift
        // the operating point at most ~0.5 toward cutoff.
        biasState = driftCoef * biasState
                  + (1.0f - driftCoef) * juce::jmax (0.0f, v - kGridKnee);
        v -= 0.5f * std::tanh (biasState);

        float y = triodeShape (v) - restingY;
        y = millerLP.processSample (y);
        return -outLevel * y; // triode inverts
    }

private:
    static constexpr float kGridKnee = 0.75f;

    void updateFilters()
    {
        couplingHP.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, couplingHz);
        millerLP.coefficients   = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sr, millerHz);
    }

    double sr { 0.0 };
    float couplingHz { 30.0f };
    float millerHz   { 8000.0f };
    float bias       { 0.2f };
    float gain       { 1.0f };
    float outLevel   { 1.0f };
    float restingY   { 0.0f };
    float biasState  { 0.0f };
    float driftCoef  { 0.9995f };

    juce::dsp::IIR::Filter<float> couplingHP;
    juce::dsp::IIR::Filter<float> millerLP;
};

/**
 * Cascaded preamp, 8x oversampled.
 *
 * Topology follows a real amp rather than "N identical clippers in a row":
 *
 *   input tube (fixed gain) → GAIN POT → cascade of voiced stages
 *
 * The input tube runs at a fixed, moderate gain so pick transients hit the
 * cascade with their dynamics intact; the gain knob is the pot AFTER it, like
 * the real thing. Each later stage has its own drive and an interstage divider
 * (outLevel) so saturation builds progressively down the chain instead of
 * every stage slamming into a square wave. On Lead, stage 3 is a cold clipper
 * (bias far toward cutoff) — the aggressive, chunky asymmetry modern high-gain
 * amps are built around.
 */
class TubePreamp
{
public:
    enum class Channel { Clean = 0, Crunch = 1, Lead = 2 };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        baseSampleRate = spec.sampleRate;
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            spec.numChannels, 3 /* 2^3 = 8x */,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        oversampler->initProcessing (spec.maximumBlockSize);

        const double osRate = spec.sampleRate * 8.0;
        for (auto& s : stages) s.prepare (osRate);
        applyChannelVoicing();

        inputHP.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (spec.sampleRate, 40.0f);
        inputHP.reset();
        // Asymmetric clipping leaves a drive-dependent DC offset on the last
        // stage's output; block it before the tone stack.
        outputDC.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (spec.sampleRate, 20.0f);
        outputDC.reset();
    }

    void setChannel (Channel c)
    {
        channel = c;
        applyChannelVoicing();
    }

    void setBright (bool b) { bright = b; applyChannelVoicing(); }
    void setGain (float normalized01)
    {
        gainKnob = normalized01;
        applyChannelVoicing();
    }

    int getOversamplingLatencySamples() const
    {
        return oversampler ? (int) oversampler->getLatencyInSamples() : 0;
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        // Input HP at base rate (removes rumble before nonlinearity)
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto* d = block.getChannelPointer (ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
                d[i] = inputHP.processSample (d[i]);
        }

        // 8x oversample, then cascade stages, then downsample
        auto osBlock = oversampler->processSamplesUp (block);

        const int numStages = (channel == Channel::Clean) ? 2
                            : (channel == Channel::Crunch) ? 3 : 4;

        // Each stage inverts; flip odd-stage-count channels back so switching
        // channels doesn't flip the output polarity.
        const float polarity = (numStages % 2 == 1) ? -1.0f : 1.0f;

        for (size_t ch = 0; ch < osBlock.getNumChannels(); ++ch)
        {
            auto* d = osBlock.getChannelPointer (ch);
            const size_t n = osBlock.getNumSamples();
            for (size_t i = 0; i < n; ++i)
            {
                float x = stages[0].processSample (d[i]);
                x *= potGain; // the gain knob lives after the input tube
                for (int s = 1; s < numStages; ++s)
                    x = stages[(size_t) s].processSample (x);
                d[i] = x * makeup * polarity;
            }
        }

        oversampler->processSamplesDown (block);

        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto* d = block.getChannelPointer (ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
                d[i] = outputDC.processSample (d[i]);
        }
    }

    void reset()
    {
        if (oversampler) oversampler->reset();
        inputHP.reset();
        outputDC.reset();
        if (baseSampleRate > 0)
            for (auto& s : stages) s.prepare (baseSampleRate * 8.0);
        applyChannelVoicing();
    }

private:
    void applyChannelVoicing()
    {
        // Audio-taper gain pot (never fully closed — real amps still pass a
        // little at gain 0).
        potGain = 0.045f + 0.955f * gainKnob * gainKnob;

        // Gain-dependent tightening: more gain → higher HP into the second
        // stage, so cranked settings stay chunky instead of turning to mud.
        const float tight = 1.0f + 0.8f * gainKnob;

        // Makeup values calibrated with DeanAmpDiTest against the Strat DI
        // fixture (play-loud balance across channels; see tools/DiTestMain.cpp).
        switch (channel)
        {
            case Channel::Clean:
                // Nearly linear; the input tube just kisses the knee on hard
                // pick attacks, stage 2 adds gentle warmth.
                stages[0].setVoicing (bright ? 180.0f : 25.0f, 12000.0f, 0.10f, 3.5f, 1.0f);
                stages[1].setVoicing (45.0f, 11000.0f, 0.15f, 5.0f, 1.0f);
                makeup = 0.66f;
                break;

            case Channel::Crunch:
                stages[0].setVoicing (bright ? 220.0f : 55.0f, 12000.0f, 0.12f, 5.0f, 1.0f);
                stages[1].setVoicing (110.0f * tight, 9500.0f, 0.22f, 7.0f, 0.45f);
                stages[2].setVoicing (190.0f, 8200.0f, 0.30f, 4.5f, 1.0f);
                makeup = 0.345f;
                break;

            case Channel::Lead:
                stages[0].setVoicing (bright ? 220.0f : 80.0f, 12000.0f, 0.12f, 7.0f, 1.0f);
                stages[1].setVoicing (140.0f * tight, 9500.0f, 0.20f, 9.0f, 0.40f);
                // Cold clipper: biased hard toward cutoff, clips one side
                // almost immediately — the modern high-gain "chunk" stage.
                stages[2].setVoicing (220.0f, 8000.0f, -0.90f, 7.5f, 0.35f);
                stages[3].setVoicing (260.0f, 6800.0f, 0.25f, 5.0f, 1.0f);
                makeup = 0.20f;
                break;
        }
    }

    Channel channel { Channel::Crunch };
    bool    bright  { false };
    float   gainKnob { 0.5f };
    float   potGain  { 0.334f };
    float   makeup   { 0.5f };
    double  baseSampleRate { 0.0 };

    std::array<TubeStage, 4> stages;
    juce::dsp::IIR::Filter<float> inputHP, outputDC;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
};

} // namespace deanamp
