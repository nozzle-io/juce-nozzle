#pragma once

#include "PluginProcessor.hpp"
#include "juce_nozzle/juce_nozzle_receiver.hpp"

#include <memory>

class NozzleReceiverAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit NozzleReceiverAudioProcessorEditor(NozzleReceiverAudioProcessor &processor);
    ~NozzleReceiverAudioProcessorEditor() override;

    void paint(juce::Graphics &graphics) override;
    void resized() override;

private:
    void timerCallback() override;
    void connect_receiver();
    void disconnect_receiver();
    void update_preview(const juce_nozzle::receiver_frame &frame);
    void update_status(const juce::String &message);

    NozzleReceiverAudioProcessor &processor_;
    juce::Label title_label_;
    juce::TextEditor source_name_editor_;
    juce::TextButton connect_button_{"Connect"};
    juce::TextButton disconnect_button_{"Disconnect"};
    juce::Label status_label_;
    juce::Image preview_image_;
    juce::String status_text_{"Idle. No nozzle work runs on the audio callback thread."};
    std::unique_ptr<juce_nozzle::receiver_client> receiver_;
    uint64_t observed_frames_{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NozzleReceiverAudioProcessorEditor)
};
