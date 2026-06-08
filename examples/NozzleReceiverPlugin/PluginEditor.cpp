#include "PluginEditor.hpp"

namespace {

bool is_juce_message_thread(void *user_data) {
    juce::ignoreUnused(user_data);
    return juce::MessageManager::getInstance()->isThisTheMessageThread();
}

juce_nozzle::thread_policy juce_message_thread_policy() {
    juce_nozzle::thread_policy policy;
    policy.required_context = "JUCE message thread";
    policy.is_allowed = is_juce_message_thread;
    return policy;
}

juce::Image rgba8_to_image(const juce_nozzle::receiver_frame &frame) {
    juce::Image image(juce::Image::ARGB, (int)frame.width, (int)frame.height, true);
    for(uint32_t y = 0; y < frame.height; y++) {
        for(uint32_t x = 0; x < frame.width; x++) {
            const size_t offset = ((size_t)y * frame.width + x) * 4u;
            const uint8_t red = frame.rgba8[offset + 0u];
            const uint8_t green = frame.rgba8[offset + 1u];
            const uint8_t blue = frame.rgba8[offset + 2u];
            const uint8_t alpha = frame.rgba8[offset + 3u];
            image.setPixelAt((int)x, (int)y, juce::Colour(red, green, blue, alpha));
        }
    }
    return image;
}

} // namespace

NozzleReceiverAudioProcessorEditor::NozzleReceiverAudioProcessorEditor(NozzleReceiverAudioProcessor &processor)
: AudioProcessorEditor(&processor)
, processor_(processor)
{
    title_label_.setText("Nozzle Receiver", juce::dontSendNotification);
    title_label_.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    addAndMakeVisible(title_label_);

    source_name_editor_.setText(processor_.source_name(), false);
    source_name_editor_.setTooltip("nozzle sender/source name to receive");
    addAndMakeVisible(source_name_editor_);

    connect_button_.onClick = [this]() { connect_receiver(); };
    disconnect_button_.onClick = [this]() { disconnect_receiver(); };
    addAndMakeVisible(connect_button_);
    addAndMakeVisible(disconnect_button_);

    status_label_.setText(status_text_, juce::dontSendNotification);
    status_label_.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(status_label_);

    setSize(560, 420);
    startTimerHz(30);
}

NozzleReceiverAudioProcessorEditor::~NozzleReceiverAudioProcessorEditor() {
    stopTimer();
    disconnect_receiver();
}

void NozzleReceiverAudioProcessorEditor::paint(juce::Graphics &graphics) {
    graphics.fillAll(juce::Colours::black);
    graphics.setColour(juce::Colours::white);

    const juce::Rectangle<int> preview_bounds = getLocalBounds().reduced(16).removeFromBottom(260);
    graphics.setColour(juce::Colours::darkgrey);
    graphics.fillRect(preview_bounds);
    graphics.setColour(juce::Colours::grey);
    graphics.drawRect(preview_bounds, 1);

    if(preview_image_.isValid()) {
        graphics.drawImageWithin(preview_image_, preview_bounds.getX(), preview_bounds.getY(), preview_bounds.getWidth(), preview_bounds.getHeight(), juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    } else {
        graphics.setColour(juce::Colours::lightgrey);
        graphics.drawFittedText("Waiting for rgba8_unorm nozzle frame", preview_bounds, juce::Justification::centred, 2);
    }
}

void NozzleReceiverAudioProcessorEditor::resized() {
    auto bounds = getLocalBounds().reduced(16);
    title_label_.setBounds(bounds.removeFromTop(28));
    bounds.removeFromTop(8);

    auto row = bounds.removeFromTop(32);
    source_name_editor_.setBounds(row.removeFromLeft(250));
    row.removeFromLeft(8);
    connect_button_.setBounds(row.removeFromLeft(110));
    row.removeFromLeft(8);
    disconnect_button_.setBounds(row.removeFromLeft(120));

    bounds.removeFromTop(8);
    status_label_.setBounds(bounds.removeFromTop(68));
}

void NozzleReceiverAudioProcessorEditor::timerCallback() {
    if(receiver_ == nullptr) return;

    const juce_nozzle::receiver_poll_result result = receiver_->poll(0);
    if(result.has_frame) {
        observed_frames_ += 1u;
        update_preview(result.frame);
        juce::String text;
        text << "Receiving " << (int)result.frame.width << "x" << (int)result.frame.height;
        text << " frame=" << (juce::int64)result.frame.frame_index;
        text << " observed=" << (juce::int64)observed_frames_;
        text << " fps=" << juce::String(result.frame.estimated_fps, 1);
        text << " dropped=" << (juce::int64)result.frame.dropped_frame_count;
        text << " | nozzle work is on the editor timer, not processBlock().";
        update_status(text);
    } else if(!result.status.empty()) {
        update_status(juce::String(result.status));
    }
}

void NozzleReceiverAudioProcessorEditor::connect_receiver() {
    processor_.set_source_name(source_name_editor_.getText());
    receiver_ = std::make_unique<juce_nozzle::receiver_client>(juce_message_thread_policy());
    observed_frames_ = 0;
    preview_image_ = {};
    const bool connected = receiver_->connect(processor_.source_name().toStdString(), "juce-nozzle receiver plugin");
    update_status(connected ? "Receiver created; waiting for frames." : receiver_->last_error());
    repaint();
}

void NozzleReceiverAudioProcessorEditor::disconnect_receiver() {
    if(receiver_ != nullptr) {
        receiver_->disconnect();
        receiver_.reset();
    }
    update_status("Disconnected. No nozzle work runs on the audio callback thread.");
}

void NozzleReceiverAudioProcessorEditor::update_preview(const juce_nozzle::receiver_frame &frame) {
    preview_image_ = rgba8_to_image(frame);
    repaint();
}

void NozzleReceiverAudioProcessorEditor::update_status(const juce::String &message) {
    status_text_ = message;
    status_label_.setText(status_text_, juce::dontSendNotification);
}
