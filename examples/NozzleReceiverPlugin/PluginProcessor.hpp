#pragma once

#include <JuceHeader.h>

class NozzleReceiverAudioProcessor : public juce::AudioProcessor {
public:
    NozzleReceiverAudioProcessor();
    ~NozzleReceiverAudioProcessor() override = default;

    void prepareToPlay(double sample_rate, int samples_per_block) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
    void processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi_messages) override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &new_name) override;

    void getStateInformation(juce::MemoryBlock &dest_data) override;
    void setStateInformation(const void *data, int size_in_bytes) override;

    juce::String source_name() const;
    void set_source_name(const juce::String &name);

private:
    juce::String source_name_{"nozzle_tester"};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NozzleReceiverAudioProcessor)
};
