#include <JuceHeader.h>
#include <juce_nozzle/juce_nozzle_receiver.hpp>

namespace {

juce::Image rgba8_to_image(const juce_nozzle::receiver_frame &frame) {
    juce::Image image(juce::Image::ARGB, (int)frame.width, (int)frame.height, true);
    for(uint32_t y = 0; y < frame.height; y++) {
        for(uint32_t x = 0; x < frame.width; x++) {
            const size_t offset = ((size_t)y * frame.width + x) * 4u;
            image.setPixelAt(
                (int)x,
                (int)y,
                juce::Colour(frame.rgba8[offset + 0u], frame.rgba8[offset + 1u], frame.rgba8[offset + 2u], frame.rgba8[offset + 3u])
            );
        }
    }
    return image;
}

} // namespace

class receiver_component final : public juce::Component, private juce::Timer {
public:
    receiver_component() {
        title_label_.setText("Nozzle Receiver Standalone", juce::dontSendNotification);
        title_label_.setFont(juce::FontOptions(18.0f, juce::Font::bold));
        addAndMakeVisible(title_label_);

        source_name_editor_.setText("juce_nozzle_sender", false);
        addAndMakeVisible(source_name_editor_);

        connect_button_.onClick = [this]() { connect_receiver(); };
        disconnect_button_.onClick = [this]() { disconnect_receiver(); };
        addAndMakeVisible(connect_button_);
        addAndMakeVisible(disconnect_button_);

        status_label_.setJustificationType(juce::Justification::topLeft);
        status_label_.setText("Disconnected. Receives rgba8_unorm frames from the message-thread timer.", juce::dontSendNotification);
        addAndMakeVisible(status_label_);

        setSize(560, 420);
        startTimerHz(30);
    }

    ~receiver_component() override {
        stopTimer();
        disconnect_receiver();
    }

    void paint(juce::Graphics &graphics) override {
        graphics.fillAll(juce::Colours::black);
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

    void resized() override {
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

private:
    void connect_receiver() {
        const juce::String source_name = source_name_editor_.getText().trim();
        if(source_name.isEmpty()) {
            status_label_.setText("Source name is empty.", juce::dontSendNotification);
            return;
        }
        receiver_ = std::make_unique<juce_nozzle::receiver_client>();
        observed_frames_ = 0;
        preview_image_ = {};
        const bool connected = receiver_->connect(source_name.toStdString(), "juce-nozzle receiver standalone");
        status_label_.setText(connected ? "Receiver created; waiting for frames." : receiver_->last_error(), juce::dontSendNotification);
        repaint();
    }

    void disconnect_receiver() {
        if(receiver_ != nullptr) {
            receiver_->disconnect();
            receiver_.reset();
        }
        status_label_.setText("Disconnected. No nozzle work runs on an audio callback thread.", juce::dontSendNotification);
    }

    void timerCallback() override {
        if(receiver_ == nullptr) return;
        const juce_nozzle::receiver_poll_result result = receiver_->poll(0);
        if(result.has_frame) {
            observed_frames_ += 1u;
            preview_image_ = rgba8_to_image(result.frame);
            juce::String text;
            text << "Receiving " << (int)result.frame.width << "x" << (int)result.frame.height;
            text << " frame=" << (juce::int64)result.frame.frame_index;
            text << " observed=" << (juce::int64)observed_frames_;
            text << " fps=" << juce::String(result.frame.estimated_fps, 1);
            text << " dropped=" << (juce::int64)result.frame.dropped_frame_count;
            text << " | expected sender corners: TL red, TR green, BL blue, BR white.";
            status_label_.setText(text, juce::dontSendNotification);
            repaint();
        } else if(!result.status.empty()) {
            status_label_.setText(result.status, juce::dontSendNotification);
        }
    }

    juce::Label title_label_;
    juce::TextEditor source_name_editor_;
    juce::TextButton connect_button_{"Connect"};
    juce::TextButton disconnect_button_{"Disconnect"};
    juce::Label status_label_;
    juce::Image preview_image_;
    std::unique_ptr<juce_nozzle::receiver_client> receiver_;
    uint64_t observed_frames_{0};
};

class receiver_application final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "Nozzle Receiver Standalone"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String &command_line) override {
        juce::ignoreUnused(command_line);
        main_window_ = std::make_unique<main_window>(getApplicationName());
    }

    void shutdown() override {
        main_window_.reset();
    }

    void systemRequestedQuit() override {
        quit();
    }

private:
    class main_window final : public juce::DocumentWindow {
    public:
        explicit main_window(juce::String name)
        : juce::DocumentWindow(name, juce::Colours::black, juce::DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new receiver_component(), true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<main_window> main_window_;
};

START_JUCE_APPLICATION(receiver_application)
