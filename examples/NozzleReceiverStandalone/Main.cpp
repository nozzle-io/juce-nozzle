#include <JuceHeader.h>
#include <juce_nozzle/juce_nozzle_receiver.hpp>
#include <juce_nozzle/juce_thread_policy.hpp>
#include <nozzle/nozzle_c.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {


struct smoke_receiver_options {
    bool enabled{false};
    std::string source_name{"juce_nozzle_app_smoke"};
    uint32_t width{320};
    uint32_t height{240};
    uint32_t timeout_ms{10000};
    std::string evidence_path{};
};

struct smoke_sample {
    std::string name{};
    uint32_t x{0};
    uint32_t y{0};
    uint8_t expected_red{0};
    uint8_t expected_green{0};
    uint8_t expected_blue{0};
    uint8_t actual_red{0};
    uint8_t actual_green{0};
    uint8_t actual_blue{0};
    bool passed{false};
};

struct smoke_evidence {
    bool passed{false};
    std::string failure_reason{};
    uint32_t observed_width{0};
    uint32_t observed_height{0};
    uint64_t observed_frame_index{0};
    uint64_t observed_frame_count{0};
    double estimated_fps{0.0};
    int observed_storage_format{0};
    int observed_semantic_format{0};
    int observed_copied_format{0};
    bool dimensions_ok{false};
    bool top_left_red{false};
    bool top_right_green{false};
    bool bottom_left_blue{false};
    bool bottom_right_white{false};
    bool orientation_ok{false};
    bool channel_order_ok{false};
    std::vector<smoke_sample> samples{};
};

uint32_t parse_uint_option(const juce::StringArray &tokens, const juce::String &name, uint32_t fallback) {
    const int index = tokens.indexOf(name);
    if(index < 0 || tokens.size() <= index + 1) return fallback;
    const int value = tokens[index + 1].getIntValue();
    if(value <= 0) return fallback;
    return (uint32_t)value;
}

juce::String parse_string_option(const juce::StringArray &tokens, const juce::String &name, const juce::String &fallback) {
    const int index = tokens.indexOf(name);
    if(index < 0 || tokens.size() <= index + 1) return fallback;
    const juce::String value = tokens[index + 1].trim();
    return value.isEmpty() ? fallback : value;
}

smoke_receiver_options parse_smoke_receiver_options(const juce::String &command_line) {
    smoke_receiver_options options;
    const juce::StringArray tokens = juce::StringArray::fromTokens(command_line, true);
    options.enabled = tokens.contains("--smoke-receiver");
    if(!options.enabled) return options;
    options.source_name = parse_string_option(tokens, "--source", "juce_nozzle_app_smoke").toStdString();
    options.width = parse_uint_option(tokens, "--width", 320u);
    options.height = parse_uint_option(tokens, "--height", 240u);
    options.timeout_ms = parse_uint_option(tokens, "--timeout-ms", 10000u);
    options.evidence_path = parse_string_option(tokens, "--evidence", {}).toStdString();
    return options;
}

std::string json_escape(const std::string &text) {
    std::string result;
    result.reserve(text.size() + 8);
    for(char c : text) {
        switch(c) {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if((unsigned char)c < 0x20u) {
                    result += "?";
                } else {
                    result += c;
                }
                break;
        }
    }
    return result;
}

const char *smoke_os_name() {
#if defined(_WIN32)
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#else
    return "unknown";
#endif
}

const char *check_text(bool value) {
    return value ? "PASS" : "FAIL";
}

const char *texture_format_name(int format) {
    switch(format) {
        case NOZZLE_FORMAT_RGBA8_UNORM: return "rgba8_unorm";
        case NOZZLE_FORMAT_BGRA8_UNORM: return "bgra8_unorm";
        case NOZZLE_FORMAT_RGBA8_SRGB: return "rgba8_srgb";
        case NOZZLE_FORMAT_BGRA8_SRGB: return "bgra8_srgb";
        case NOZZLE_FORMAT_UNKNOWN: return "unknown";
        default: return "other";
    }
}

smoke_sample sample_pixel(const juce_nozzle::receiver_frame &frame, const std::string &name, uint32_t x, uint32_t y, uint8_t red, uint8_t green, uint8_t blue) {
    smoke_sample sample{};
    sample.name = name;
    sample.x = x;
    sample.y = y;
    sample.expected_red = red;
    sample.expected_green = green;
    sample.expected_blue = blue;
    if(frame.width <= x || frame.height <= y) return sample;
    const size_t offset = ((size_t)y * frame.width + x) * 4u;
    sample.actual_red = frame.rgba8[offset + 0u];
    sample.actual_green = frame.rgba8[offset + 1u];
    sample.actual_blue = frame.rgba8[offset + 2u];
    sample.passed = sample.actual_red == red && sample.actual_green == green && sample.actual_blue == blue;
    return sample;
}

void verify_corners(const juce_nozzle::receiver_frame &frame, smoke_evidence &evidence) {
    const uint32_t left_x = frame.width / 8u;
    const uint32_t right_x = frame.width - 1u - frame.width / 8u;
    const uint32_t top_y = frame.height / 8u;
    const uint32_t bottom_y = frame.height - 1u - frame.height / 8u;
    evidence.samples.push_back(sample_pixel(frame, "top_left_red", left_x, top_y, 255u, 0u, 0u));
    evidence.samples.push_back(sample_pixel(frame, "top_right_green", right_x, top_y, 0u, 255u, 0u));
    evidence.samples.push_back(sample_pixel(frame, "bottom_left_blue", left_x, bottom_y, 0u, 0u, 255u));
    evidence.samples.push_back(sample_pixel(frame, "bottom_right_white", right_x, bottom_y, 255u, 255u, 255u));
    evidence.top_left_red = evidence.samples[0].passed;
    evidence.top_right_green = evidence.samples[1].passed;
    evidence.bottom_left_blue = evidence.samples[2].passed;
    evidence.bottom_right_white = evidence.samples[3].passed;
    evidence.orientation_ok = evidence.top_left_red && evidence.top_right_green && evidence.bottom_left_blue && evidence.bottom_right_white;
    evidence.channel_order_ok = evidence.top_left_red && evidence.bottom_left_blue;
    for(const smoke_sample &sample : evidence.samples) {
        if(!sample.passed) {
            std::fprintf(stderr, "standalone app receiver smoke pixel mismatch at %u,%u: got rgb(%u,%u,%u), expected rgb(%u,%u,%u)\n", sample.x, sample.y, sample.actual_red, sample.actual_green, sample.actual_blue, sample.expected_red, sample.expected_green, sample.expected_blue);
        }
    }
}

std::string make_smoke_evidence_json(const smoke_receiver_options &options, const smoke_evidence &evidence) {
    std::ostringstream stream;
    stream << "{\n";
    stream << "  \"schema_version\": \"0.1.0\",\n";
    stream << "  \"tool\": \"Nozzle Receiver Standalone\",\n";
    stream << "  \"os\": \"" << smoke_os_name() << "\",\n";
    stream << "  \"backend\": \"auto\",\n";
    stream << "  \"role\": \"juce_receiver_standalone\",\n";
    stream << "  \"sender_name\": \"" << json_escape(options.source_name) << "\",\n";
    stream << "  \"receiver_name\": \"Nozzle Receiver Standalone\",\n";
    stream << "  \"dimensions\": {\"expected_width\":" << options.width << ",\"expected_height\":" << options.height << ",\"observed_width\":" << evidence.observed_width << ",\"observed_height\":" << evidence.observed_height << "},\n";
    stream << "  \"frame\": {\"observed_index\":" << evidence.observed_frame_index << ",\"observed_count\":" << evidence.observed_frame_count << ",\"estimated_fps\":" << evidence.estimated_fps << ",\"timeout_ms\":" << options.timeout_ms << "},\n";
    stream << "  \"formats\": {\"storage\":\"" << texture_format_name(evidence.observed_storage_format) << "\",\"storage_value\":" << evidence.observed_storage_format << ",\"semantic\":\"" << texture_format_name(evidence.observed_semantic_format) << "\",\"semantic_value\":" << evidence.observed_semantic_format << ",\"copied\":\"" << texture_format_name(evidence.observed_copied_format) << "\",\"copied_value\":" << evidence.observed_copied_format << "},\n";
    stream << "  \"checks\": {\"dimensions\":\"" << check_text(evidence.dimensions_ok) << "\",\"top_left_red\":\"" << check_text(evidence.top_left_red) << "\",\"top_right_green\":\"" << check_text(evidence.top_right_green) << "\",\"bottom_left_blue\":\"" << check_text(evidence.bottom_left_blue) << "\",\"bottom_right_white\":\"" << check_text(evidence.bottom_right_white) << "\",\"orientation\":\"" << check_text(evidence.orientation_ok) << "\",\"channel_order\":\"" << check_text(evidence.channel_order_ok) << "\"},\n";
    stream << "  \"samples\": [\n";
    for(size_t index = 0; index < evidence.samples.size(); index++) {
        const smoke_sample &sample = evidence.samples[index];
        stream << "    {\"name\":\"" << json_escape(sample.name) << "\",\"x\":" << sample.x << ",\"y\":" << sample.y << ",\"expected_rgb\":[" << (int)sample.expected_red << "," << (int)sample.expected_green << "," << (int)sample.expected_blue << "],\"actual_rgb\":[" << (int)sample.actual_red << "," << (int)sample.actual_green << "," << (int)sample.actual_blue << "],\"passed\":" << (sample.passed ? "true" : "false") << "}";
        if(index + 1u < evidence.samples.size()) stream << ",";
        stream << "\n";
    }
    stream << "  ],\n";
    stream << "  \"verdict\": \"" << (evidence.passed ? "PASS" : "FAIL") << "\",\n";
    stream << "  \"failure_reason\": \"" << json_escape(evidence.failure_reason) << "\"\n";
    stream << "}\n";
    return stream.str();
}

bool write_smoke_evidence(const smoke_receiver_options &options, const smoke_evidence &evidence) {
    const std::string json = make_smoke_evidence_json(options, evidence);
    if(options.evidence_path.empty()) {
        std::printf("%s", json.c_str());
        return true;
    }
    std::ofstream stream(options.evidence_path, std::ios::binary);
    if(!stream) return false;
    stream << json;
    return (bool)stream;
}

int run_smoke_receiver(const smoke_receiver_options &options) {
    const uint32_t poll_ms = 100u;
    uint32_t waited_ms = 0u;
    smoke_evidence evidence{};
    juce_nozzle::receiver_client receiver(juce_nozzle::juce_message_thread_policy());

    while(!receiver.is_connected() && waited_ms < options.timeout_ms) {
        if(receiver.connect(options.source_name, "juce-nozzle receiver standalone smoke")) break;
        juce::Thread::sleep((int)poll_ms);
        waited_ms += poll_ms;
    }

    if(!receiver.is_connected()) {
        std::fprintf(stderr, "standalone app receiver smoke connect timed out after %u ms: %s\n", options.timeout_ms, receiver.last_error().c_str());
        evidence.failure_reason = "connect_timeout";
        write_smoke_evidence(options, evidence);
        return 1;
    }

    while(waited_ms < options.timeout_ms) {
        const juce_nozzle::receiver_poll_result result = receiver.poll(poll_ms);
        waited_ms += poll_ms;
        if(result.has_frame) {
            if(result.frame.width != options.width || result.frame.height != options.height) {
                std::fprintf(stderr, "standalone app receiver smoke size mismatch: got %ux%u expected %ux%u\n", result.frame.width, result.frame.height, options.width, options.height);
                evidence.observed_width = result.frame.width;
                evidence.observed_height = result.frame.height;
                evidence.observed_frame_index = result.frame.frame_index;
                evidence.observed_frame_count = 1u;
                evidence.estimated_fps = result.frame.estimated_fps;
                evidence.observed_storage_format = result.frame.storage_format;
                evidence.observed_semantic_format = result.frame.semantic_format;
                evidence.observed_copied_format = result.frame.copied_format;
                evidence.failure_reason = "dimension_mismatch";
                write_smoke_evidence(options, evidence);
                return 1;
            }
            evidence.observed_width = result.frame.width;
            evidence.observed_height = result.frame.height;
            evidence.observed_frame_index = result.frame.frame_index;
            evidence.observed_frame_count = 1u;
            evidence.estimated_fps = result.frame.estimated_fps;
            evidence.observed_storage_format = result.frame.storage_format;
            evidence.observed_semantic_format = result.frame.semantic_format;
            evidence.observed_copied_format = result.frame.copied_format;
            evidence.dimensions_ok = true;
            verify_corners(result.frame, evidence);
            evidence.passed = evidence.orientation_ok && evidence.channel_order_ok;
            if(!evidence.passed) {
                evidence.failure_reason = "quadrant_semantics_failed";
                write_smoke_evidence(options, evidence);
                return 1;
            }
            if(!write_smoke_evidence(options, evidence)) {
                std::fprintf(stderr, "failed to write receiver smoke evidence: %s\n", options.evidence_path.c_str());
                return 1;
            }
            std::printf(
                "standalone app receiver smoke PASS %ux%u frame=%llu source=%s\n",
                result.frame.width,
                result.frame.height,
                (unsigned long long)result.frame.frame_index,
                options.source_name.c_str()
            );
            return 0;
        }
    }

    std::fprintf(stderr, "standalone app receiver smoke timed out after %u ms: %s\n", options.timeout_ms, receiver.last_error().c_str());
    evidence.failure_reason = "frame_timeout";
    write_smoke_evidence(options, evidence);
    return 1;
}

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
        receiver_ = std::make_unique<juce_nozzle::receiver_client>(juce_nozzle::juce_message_thread_policy());
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
        const smoke_receiver_options smoke_options = parse_smoke_receiver_options(command_line);
        if(smoke_options.enabled) {
            setApplicationReturnValue(run_smoke_receiver(smoke_options));
            quit();
            return;
        }
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
