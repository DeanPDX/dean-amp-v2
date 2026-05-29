#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace deanamp
{

namespace pid
{
    static constexpr const char* gate     = "gate";
    static constexpr const char* input    = "input";
    static constexpr const char* gain     = "gain";
    static constexpr const char* bass     = "bass";
    static constexpr const char* mid      = "mid";
    static constexpr const char* treble   = "treble";
    static constexpr const char* presence = "presence";
    static constexpr const char* resonance= "resonance";
    static constexpr const char* master   = "master";
    static constexpr const char* output   = "output";
    static constexpr const char* channel  = "channel";
    static constexpr const char* bright   = "bright";
    static constexpr const char* voicing  = "voicing";
    static constexpr const char* cab      = "cab";
    static constexpr const char* mic      = "mic";
    static constexpr const char* cabBypass= "cabBypass";
    static constexpr const char* cabLevel = "cabLevel";
}

juce::AudioProcessorValueTreeState::ParameterLayout DeanAmpProcessor::createLayout()
{
    using namespace juce;
    using FParam = AudioParameterFloat;
    using CParam = AudioParameterChoice;
    using BParam = AudioParameterBool;

    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pct = [] (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; };
    auto db  = [] (float v) { return juce::String (v, 1) + " dB"; };
    // Amp-knob convention: show 0..10 like a real amp panel, one decimal.
    auto ten = [] (float v) { return juce::String (v * 10.0f, 1); };

    p.push_back (std::make_unique<FParam> (
        ParameterID { pid::gate, 1 }, "Gate",
        NormalisableRange<float> (-80.0f, -20.0f, 0.1f), -60.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([](float v, int){ return juce::String (v, 1) + " dB"; })));
    p.push_back (std::make_unique<FParam> (
        ParameterID { pid::input, 1 }, "Input",
        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([](float v, int){ return juce::String (v, 1) + " dB"; })));
    p.push_back (std::make_unique<FParam> (
        ParameterID { pid::gain, 1 }, "Gain",
        NormalisableRange<float> (0.0f, 1.0f), 0.55f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([ten](float v, int){ return ten(v); })));
    p.push_back (std::make_unique<FParam> (
        ParameterID { pid::bass, 1 }, "Bass",   NormalisableRange<float> (0.0f, 1.0f), 0.55f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([ten](float v, int){ return ten(v); })));
    p.push_back (std::make_unique<FParam> (
        ParameterID { pid::mid, 1 },  "Mid",    NormalisableRange<float> (0.0f, 1.0f), 0.55f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([ten](float v, int){ return ten(v); })));
    p.push_back (std::make_unique<FParam> (
        ParameterID { pid::treble, 1 }, "Treble", NormalisableRange<float> (0.0f, 1.0f), 0.60f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([ten](float v, int){ return ten(v); })));
    p.push_back (std::make_unique<FParam> (
        ParameterID { pid::presence, 1 }, "Presence", NormalisableRange<float> (0.0f, 1.0f), 0.50f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([ten](float v, int){ return ten(v); })));
    p.push_back (std::make_unique<FParam> (
        ParameterID { pid::resonance, 1 }, "Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.45f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([ten](float v, int){ return ten(v); })));
    p.push_back (std::make_unique<FParam> (
        ParameterID { pid::master, 1 }, "Master", NormalisableRange<float> (0.0f, 1.0f), 0.55f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([ten](float v, int){ return ten(v); })));
    p.push_back (std::make_unique<FParam> (
        ParameterID { pid::output, 1 }, "Output",
        NormalisableRange<float> (-36.0f, 12.0f, 0.1f), -3.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([](float v, int){ return juce::String (v, 1) + " dB"; })));
    p.push_back (std::make_unique<CParam> (
        ParameterID { pid::mic, 1 }, "Mic Position",
        juce::StringArray { "Center", "Edge", "Off-Axis" }, 0));

    p.push_back (std::make_unique<CParam> (
        ParameterID { pid::channel, 1 }, "Channel",
        juce::StringArray { "Clean", "Crunch", "Lead" }, 1));
    p.push_back (std::make_unique<CParam> (
        ParameterID { pid::voicing, 1 }, "Voicing",
        juce::StringArray { "British", "American" }, 0));
    p.push_back (std::make_unique<CParam> (
        ParameterID { pid::cab, 1 }, "Cabinet",
        juce::StringArray { "4x12 Greenback", "4x12 V30", "2x12 American", "1x12 Tweed" }, 1));

    p.push_back (std::make_unique<BParam> (ParameterID { pid::bright, 1 },    "Bright",     false));
    p.push_back (std::make_unique<BParam> (ParameterID { pid::cabBypass, 1 }, "Cab Bypass", false));

    p.push_back (std::make_unique<FParam> (
        ParameterID { pid::cabLevel, 1 }, "Level",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([](float v, int){ return juce::String (v, 1) + " dB"; })));

    juce::ignoreUnused (db, pct);
    return { p.begin(), p.end() };
}

DeanAmpProcessor::DeanAmpProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createLayout())
{}

bool DeanAmpProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in.isDisabled() || out.isDisabled()) return false;
    return (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo())
        && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void DeanAmpProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // The amp itself is mono (input is summed to mono and the preamp/power-amp
    // run on one channel), so those stages are prepared for a single channel.
    // The cab is the stereo stage: the mono amp signal is fed to both channels
    // and convolved with slightly different L/R IRs for width — so it is
    // prepared for two channels.
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = 1;

    gate.prepare (sampleRate);
    preamp.prepare (spec);
    toneStack.prepare (sampleRate);
    powerAmp.prepare (spec);

    auto cabSpec = spec;
    cabSpec.numChannels = 2;
    cab.prepare (cabSpec);

    setLatencySamples (preamp.getOversamplingLatencySamples() + powerAmp.getLatencySamples());
    updateFromParams();
}

void DeanAmpProcessor::updateFromParams()
{
    const float gateDb   = apvts.getRawParameterValue (pid::gate)->load();
    const float inputDb  = apvts.getRawParameterValue (pid::input)->load();
    const float gainV    = apvts.getRawParameterValue (pid::gain)->load();
    const float bassV    = apvts.getRawParameterValue (pid::bass)->load();
    const float midV     = apvts.getRawParameterValue (pid::mid)->load();
    const float trebleV  = apvts.getRawParameterValue (pid::treble)->load();
    const float presV    = apvts.getRawParameterValue (pid::presence)->load();
    const float resoV    = apvts.getRawParameterValue (pid::resonance)->load();
    const float masterV  = apvts.getRawParameterValue (pid::master)->load();
    const float outDb    = apvts.getRawParameterValue (pid::output)->load();
    const int   micIdx   = (int) apvts.getRawParameterValue (pid::mic)->load();
    const int   chanIdx  = (int) apvts.getRawParameterValue (pid::channel)->load();
    const int   voiceIdx = (int) apvts.getRawParameterValue (pid::voicing)->load();
    const int   cabIdx   = (int) apvts.getRawParameterValue (pid::cab)->load();
    const bool  brightOn = apvts.getRawParameterValue (pid::bright)->load() > 0.5f;
    const bool  cabBy    = apvts.getRawParameterValue (pid::cabBypass)->load() > 0.5f;

    gate.setThresholdDb (gateDb);
    inputTrim  = juce::Decibels::decibelsToGain (inputDb);
    outputTrim = juce::Decibels::decibelsToGain (outDb);

    preamp.setChannel (static_cast<TubePreamp::Channel> (chanIdx));
    preamp.setBright  (brightOn);
    preamp.setGain    (gainV);

    toneStack.setVoicing (voiceIdx == 0 ? ToneStack::Voicing::Marshall : ToneStack::Voicing::Fender);
    toneStack.setBass    (bassV);
    toneStack.setMid     (midV);
    toneStack.setTreble  (trebleV);

    powerAmp.setPresence    (presV);
    powerAmp.setResonance   (resoV);
    powerAmp.setMasterDrive (masterV);

    // Mic position → on-axis/off-axis IR blend (Center brightest, Off-Axis darkest).
    const float micBlend = micIdx == 0 ? 0.0f : (micIdx == 1 ? 0.6f : 1.0f);
    cab.setCabIndex (cabIdx);
    cab.setMicBlend (micBlend);
    cab.setBypass   (cabBy);

    const float cabLevelDb = apvts.getRawParameterValue (pid::cabLevel)->load();
    cabLevelGain = juce::Decibels::decibelsToGain (cabLevelDb);
}

void DeanAmpProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numIn  = getTotalNumInputChannels();
    const auto numOut = getTotalNumOutputChannels();
    for (int i = numIn; i < numOut; ++i) buffer.clear (i, 0, buffer.getNumSamples());

    updateFromParams();

    const int n = buffer.getNumSamples();

    // Input trim.
    buffer.applyGain (inputTrim);

    // The amp is a mono device fed by a guitar: sum the input down to channel 0.
    {
        auto* mono = buffer.getWritePointer (0);
        for (int i = 0; i < n; ++i)
        {
            float s = mono[i];
            for (int c = 1; c < numIn; ++c) s += buffer.getReadPointer (c)[i];
            mono[i] = s / (float) juce::jmax (1, numIn);
        }
    }

    // Input metering (mono).
    {
        const float peak = buffer.getMagnitude (0, 0, n);
        inputMeterL.store (peak);
        inputMeterR.store (peak);
    }

    // Run the mono amp (gate → preamp → tone → power) on a single channel — both
    // correct (the per-stage IIR filters keep one state) and cheaper.
    juce::dsp::AudioBlock<float> block (buffer);
    auto mono = block.getSingleChannelBlock (0);

    gate.process     (mono);
    preamp.process   (mono);
    toneStack.process(mono);
    powerAmp.process (mono);

    // Feed the finished mono signal to every output channel, then run the STEREO
    // cab — its slightly different L/R impulse responses pull the channels apart
    // for a wide image (while the low end stays mono-compatible).
    for (int c = 1; c < numOut; ++c)
        buffer.copyFrom (c, 0, buffer, 0, 0, n);

    cab.process (block);

    // Post-cab level + output trim (all output channels).
    buffer.applyGain (cabLevelGain * outputTrim);

    // Output metering (per channel — the cab makes L/R genuinely different now).
    {
        outputMeterL.store (buffer.getMagnitude (0, 0, n));
        outputMeterR.store (buffer.getMagnitude (numOut > 1 ? 1 : 0, 0, n));
    }
}

juce::AudioProcessorEditor* DeanAmpProcessor::createEditor()
{
    return new DeanAmpEditor (*this);
}

void DeanAmpProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void DeanAmpProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

} // namespace deanamp

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new deanamp::DeanAmpProcessor();
}
