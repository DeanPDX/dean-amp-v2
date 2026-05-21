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
        AudioParameterFloatAttributes().withStringFromValueFunction ([pct](float v, int){ return pct(v); })));
    p.push_back (std::make_unique<FParam> (
        ParameterID { pid::bass, 1 }, "Bass",   NormalisableRange<float> (0.0f, 1.0f), 0.55f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([pct](float v, int){ return pct(v); })));
    p.push_back (std::make_unique<FParam> (
        ParameterID { pid::mid, 1 },  "Mid",    NormalisableRange<float> (0.0f, 1.0f), 0.55f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([pct](float v, int){ return pct(v); })));
    p.push_back (std::make_unique<FParam> (
        ParameterID { pid::treble, 1 }, "Treble", NormalisableRange<float> (0.0f, 1.0f), 0.60f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([pct](float v, int){ return pct(v); })));
    p.push_back (std::make_unique<FParam> (
        ParameterID { pid::presence, 1 }, "Presence", NormalisableRange<float> (0.0f, 1.0f), 0.50f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([pct](float v, int){ return pct(v); })));
    p.push_back (std::make_unique<FParam> (
        ParameterID { pid::resonance, 1 }, "Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.45f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([pct](float v, int){ return pct(v); })));
    p.push_back (std::make_unique<FParam> (
        ParameterID { pid::master, 1 }, "Master", NormalisableRange<float> (0.0f, 1.0f), 0.55f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([pct](float v, int){ return pct(v); })));
    p.push_back (std::make_unique<FParam> (
        ParameterID { pid::output, 1 }, "Output",
        NormalisableRange<float> (-36.0f, 12.0f, 0.1f), -3.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([](float v, int){ return juce::String (v, 1) + " dB"; })));
    p.push_back (std::make_unique<FParam> (
        ParameterID { pid::mic, 1 }, "Mic", NormalisableRange<float> (0.0f, 1.0f), 0.35f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([](float v, int){
            return v < 0.5f ? "On-Axis" : (v < 0.8f ? "Edge" : "Off-Axis"); })));

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

    juce::ignoreUnused (db);
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
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels());

    gate.prepare (sampleRate);
    preamp.prepare (spec);
    toneStack.prepare (sampleRate);
    powerAmp.prepare (spec);
    cab.prepare (spec);

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
    const float micV     = apvts.getRawParameterValue (pid::mic)->load();
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

    cab.setCabIndex (cabIdx);
    cab.setMicBlend (micV);
    cab.setBypass   (cabBy);
}

void DeanAmpProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numIn  = getTotalNumInputChannels();
    const auto numOut = getTotalNumOutputChannels();
    for (int i = numIn; i < numOut; ++i) buffer.clear (i, 0, buffer.getNumSamples());

    updateFromParams();

    // Input trim
    buffer.applyGain (inputTrim);

    juce::dsp::AudioBlock<float> block (buffer);

    gate.process     (block);
    preamp.process   (block);
    toneStack.process(block);
    powerAmp.process (block);
    cab.process      (block);

    // Output trim
    buffer.applyGain (outputTrim);
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
