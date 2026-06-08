#include <JuceHeader.h>
#include <juce_nozzle/juce_nozzle_sender.hpp>

class sender_component final : public juce::Component, private juce::Timer {
public:
    sender_component() {
        title_label_.setText("Nozzle Sender Standalone", juce::dontSendNotification);
        title_label_.setFont(juce::FontOptions(18.0f, juce::Font::bold));
        addAndMakeVisible(title_label_);

        source_name_editor_.setText("juce_nozzle_sender", false);
        width_editor_.setText("320", false);
        height_editor_.setText("240", false);
        addAndMakeVisible(source_name_editor_);
        addAndMakeVisible(width_editor_);
        addAndMakeVisible(height_editor_);

        start_button_.onClick = [this]() { start_sender(); };
        stop_button_.onClick = [this]() { stop_sender(); };
        addAndMakeVisible(start_button_);
        addAndMakeVisible(stop_button_);

        status_label_.setJustificationType(juce::Justification::topLeft);
        status_label_.setText("Stopped. Publishes rgba8_unorm test frames from the message-thread timer.", juce::dontSendNotification);
        addAndMakeVisible(status_label_);

        setSize(520, 220);
    }

    ~sender_component() override {
        stopTimer();
        stop_sender();
    }

    void paint(juce::Graphics &graphics) override {
        graphics.fillAll(juce::Colours::black);
        graphics.setColour(juce::Colours::white);
        graphics.drawRect(getLocalBounds().reduced(8), 1);
    }

    void resized() override {
        auto bounds = getLocalBounds().reduced(16);
        title_label_.setBounds(bounds.removeFromTop(28));
        bounds.removeFromTop(8);

        auto row = bounds.removeFromTop(30);
        source_name_editor_.setBounds(row.removeFromLeft(220));
        row.removeFromLeft(8);
        width_editor_.setBounds(row.removeFromLeft(72));
        row.removeFromLeft(8);
        height_editor_.setBounds(row.removeFromLeft(72));

        bounds.removeFromTop(8);
        row = bounds.removeFromTop(32);
        start_button_.setBounds(row.removeFromLeft(110));
        row.removeFromLeft(8);
        stop_button_.setBounds(row.removeFromLeft(110));

        bounds.removeFromTop(8);
        status_label_.setBounds(bounds);
    }

private:
    uint32_t parsed_dimension(const juce::TextEditor &editor, uint32_t fallback) const {
        const int value = editor.getText().getIntValue();
        if(value <= 0) return fallback;
        return (uint32_t)value;
    }

    void start_sender() {
        const juce::String source_name = source_name_editor_.getText().trim();
        if(source_name.isEmpty()) {
            status_label_.setText("Source name is empty.", juce::dontSendNotification);
            return;
        }

        sender_ = std::make_unique<juce_nozzle::sender_client>();
        if(!sender_->connect(source_name.toStdString(), "juce-nozzle sender standalone")) {
            status_label_.setText(sender_->last_error(), juce::dontSendNotification);
            sender_.reset();
            return;
        }

        frame_count_ = 0;
        startTimerHz(30);
        status_label_.setText("Sender started. Use nozzle-viewer to inspect source: " + source_name, juce::dontSendNotification);
    }

    void stop_sender() {
        stopTimer();
        if(sender_ != nullptr) {
            sender_->disconnect();
            sender_.reset();
        }
        status_label_.setText("Stopped. No nozzle work runs on an audio callback thread.", juce::dontSendNotification);
    }

    void timerCallback() override {
        if(sender_ == nullptr) return;
        const uint32_t width = parsed_dimension(width_editor_, 320u);
        const uint32_t height = parsed_dimension(height_editor_, 240u);
        const juce_nozzle::sender_publish_result result = sender_->publish_test_pattern(width, height);
        if(result.published) {
            frame_count_ += 1u;
            juce::String text;
            text << "Published " << (int)width << "x" << (int)height;
            text << " frame=" << (juce::int64)result.frame_index;
            text << " observed=" << (juce::int64)frame_count_;
            text << " | corners: TL red, TR green, BL blue, BR white.";
            status_label_.setText(text, juce::dontSendNotification);
        } else {
            status_label_.setText(result.status, juce::dontSendNotification);
        }
    }

    juce::Label title_label_;
    juce::TextEditor source_name_editor_;
    juce::TextEditor width_editor_;
    juce::TextEditor height_editor_;
    juce::TextButton start_button_{"Start Runtime"};
    juce::TextButton stop_button_{"Stop Runtime"};
    juce::Label status_label_;
    std::unique_ptr<juce_nozzle::sender_client> sender_;
    uint64_t frame_count_{0};
};

class sender_application final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "Nozzle Sender Standalone"; }
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
            setContentOwned(new sender_component(), true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<main_window> main_window_;
};

START_JUCE_APPLICATION(sender_application)
