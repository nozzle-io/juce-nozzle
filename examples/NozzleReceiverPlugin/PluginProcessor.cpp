#include "PluginProcessor.hpp"
#include "PluginEditor.hpp"

NozzleReceiverAudioProcessor::NozzleReceiverAudioProcessor()
: AudioProcessor(BusesProperties()
      .withInput("Input", juce::AudioChannelSet::stereo(), true)
      .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{}

void NozzleReceiverAudioProcessor::prepareToPlay(double sample_rate, int samples_per_block) {
    juce::ignoreUnused(sample_rate, samples_per_block);
}

void NozzleReceiverAudioProcessor::releaseResources() {}

bool NozzleReceiverAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const {
    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

void NozzleReceiverAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi_messages) {
    juce::ignoreUnused(midi_messages);
    for(int channel = 0; channel < buffer.getNumChannels(); channel++) {
        buffer.clear(channel, 0, buffer.getNumSamples());
    }
}

juce::AudioProcessorEditor *NozzleReceiverAudioProcessor::createEditor() {
    return new NozzleReceiverAudioProcessorEditor(*this);
}

bool NozzleReceiverAudioProcessor::hasEditor() const {
    return true;
}

const juce::String NozzleReceiverAudioProcessor::getName() const {
    return JucePlugin_Name;
}

bool NozzleReceiverAudioProcessor::acceptsMidi() const {
    return false;
}

bool NozzleReceiverAudioProcessor::producesMidi() const {
    return false;
}

bool NozzleReceiverAudioProcessor::isMidiEffect() const {
    return false;
}

double NozzleReceiverAudioProcessor::getTailLengthSeconds() const {
    return 0.0;
}

int NozzleReceiverAudioProcessor::getNumPrograms() {
    return 1;
}

int NozzleReceiverAudioProcessor::getCurrentProgram() {
    return 0;
}

void NozzleReceiverAudioProcessor::setCurrentProgram(int index) {
    juce::ignoreUnused(index);
}

const juce::String NozzleReceiverAudioProcessor::getProgramName(int index) {
    juce::ignoreUnused(index);
    return {};
}

void NozzleReceiverAudioProcessor::changeProgramName(int index, const juce::String &new_name) {
    juce::ignoreUnused(index, new_name);
}

void NozzleReceiverAudioProcessor::getStateInformation(juce::MemoryBlock &dest_data) {
    juce::MemoryOutputStream stream(dest_data, true);
    stream.writeString(source_name_);
}

void NozzleReceiverAudioProcessor::setStateInformation(const void *data, int size_in_bytes) {
    if(data == nullptr || size_in_bytes <= 0) return;
    juce::MemoryInputStream stream(data, (size_t)size_in_bytes, false);
    const juce::String loaded_name = stream.readString();
    if(loaded_name.isNotEmpty()) {
        source_name_ = loaded_name;
    }
}

juce::String NozzleReceiverAudioProcessor::source_name() const {
    return source_name_;
}

void NozzleReceiverAudioProcessor::set_source_name(const juce::String &name) {
    if(name.isNotEmpty()) {
        source_name_ = name;
    }
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
    return new NozzleReceiverAudioProcessor();
}
