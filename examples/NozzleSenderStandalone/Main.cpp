#include <JuceHeader.h>
#include <juce_nozzle/juce_nozzle_sender.hpp>
#include <juce_nozzle/juce_thread_policy.hpp>

#include <nozzle/nozzle_c.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

namespace {


struct smoke_sender_options {
    bool enabled{false};
    bool opengl_enabled{false};
    std::string source_name{"juce_nozzle_app_smoke"};
    uint32_t width{320};
    uint32_t height{240};
    uint32_t frames{240};
    uint32_t interval_ms{16};
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

smoke_sender_options parse_smoke_sender_options(const juce::String &command_line) {
    smoke_sender_options options;
    const juce::StringArray tokens = juce::StringArray::fromTokens(command_line, true);
    options.enabled = tokens.contains("--smoke-sender") || tokens.contains("--smoke-opengl-sender");
    options.opengl_enabled = tokens.contains("--smoke-opengl-sender");
    if(!options.enabled) return options;
    options.source_name = parse_string_option(tokens, "--source", "juce_nozzle_app_smoke").toStdString();
    options.width = parse_uint_option(tokens, "--width", 320u);
    options.height = parse_uint_option(tokens, "--height", 240u);
    options.frames = parse_uint_option(tokens, "--frames", 240u);
    options.interval_ms = parse_uint_option(tokens, "--interval-ms", 16u);
    return options;
}

const char *nozzle_error_name(NozzleErrorCode error) {
    switch(error) {
        case NOZZLE_OK: return "NOZZLE_OK";
        case NOZZLE_ERROR_UNKNOWN: return "NOZZLE_ERROR_UNKNOWN";
        case NOZZLE_ERROR_INVALID_ARGUMENT: return "NOZZLE_ERROR_INVALID_ARGUMENT";
        case NOZZLE_ERROR_UNSUPPORTED_BACKEND: return "NOZZLE_ERROR_UNSUPPORTED_BACKEND";
        case NOZZLE_ERROR_UNSUPPORTED_FORMAT: return "NOZZLE_ERROR_UNSUPPORTED_FORMAT";
        case NOZZLE_ERROR_DEVICE_MISMATCH: return "NOZZLE_ERROR_DEVICE_MISMATCH";
        case NOZZLE_ERROR_RESOURCE_CREATION_FAILED: return "NOZZLE_ERROR_RESOURCE_CREATION_FAILED";
        case NOZZLE_ERROR_SHARED_HANDLE_FAILED: return "NOZZLE_ERROR_SHARED_HANDLE_FAILED";
        case NOZZLE_ERROR_SENDER_NOT_FOUND: return "NOZZLE_ERROR_SENDER_NOT_FOUND";
        case NOZZLE_ERROR_SENDER_CLOSED: return "NOZZLE_ERROR_SENDER_CLOSED";
        case NOZZLE_ERROR_TIMEOUT: return "NOZZLE_ERROR_TIMEOUT";
        case NOZZLE_ERROR_BACKEND_ERROR: return "NOZZLE_ERROR_BACKEND_ERROR";
        case NOZZLE_ERROR_COMMAND_FAILED: return "NOZZLE_ERROR_COMMAND_FAILED";
        default: return "NOZZLE_ERROR_UNRECOGNIZED";
    }
}

const char *opengl_transfer_mode() {
#if JUCE_MAC
    return "gpu_copy_cgl_iosurface";
#elif JUCE_WINDOWS
    return "cpu_readback_d3d11_staging";
#elif JUCE_LINUX
    return "unsupported_linux_gl_dmabuf";
#else
    return "unsupported_platform";
#endif
}

int run_smoke_sender(const smoke_sender_options &options) {
    juce_nozzle::sender_client sender(juce_nozzle::juce_message_thread_policy());
    if(!sender.connect(options.source_name, "juce-nozzle sender standalone smoke")) {
        std::fprintf(stderr, "standalone app sender smoke connect failed: %s\n", sender.last_error().c_str());
        return 1;
    }

    for(uint32_t frame = 0; frame < options.frames; frame++) {
        const juce_nozzle::sender_publish_result result = sender.publish_test_pattern(options.width, options.height);
        if(!result.published) {
            std::fprintf(stderr, "standalone app sender smoke publish failed: %s\n", result.status.c_str());
            return 1;
        }
        if(frame == 0 || frame + 1u == options.frames) {
            std::printf(
                "standalone app sender smoke published %ux%u frame=%llu source=%s\n",
                options.width,
                options.height,
                (unsigned long long)result.frame_index,
                options.source_name.c_str()
            );
            std::fflush(stdout);
        }
        juce::Thread::sleep((int)options.interval_ms);
    }

    sender.disconnect();
    return 0;
}

} // namespace

class opengl_smoke_component final : public juce::Component, private juce::OpenGLRenderer {
public:
    explicit opengl_smoke_component(smoke_sender_options options)
    : options_(std::move(options))
    {
        setSize((int)options_.width, (int)options_.height);
        context_.setRenderer(this);
        context_.setContinuousRepainting(true);
        context_.attachTo(*this);
    }

    ~opengl_smoke_component() override {
        context_.detach();
    }

    int result_code() const noexcept { return result_code_.load(); }

private:
    void newOpenGLContextCreated() override {
        NozzleSenderDesc desc{};
        desc.name = options_.source_name.c_str();
        desc.application_name = "juce-nozzle OpenGL sender standalone smoke";
        desc.ring_buffer_size = 3;
        desc.fallback_flags_valid = 1;
        desc.fallback_flags = NOZZLE_FALLBACK_STORAGE_COMPATIBLE;
        NozzleSender *created_sender = nullptr;
        const NozzleErrorCode error = nozzle_sender_create(&desc, &created_sender);
        if(error != NOZZLE_OK || created_sender == nullptr) {
            fail(juce::String("opengl sender create failed: ") + nozzle_error_name(error));
            return;
        }
        sender_ = created_sender;
    }

    void openGLContextClosing() override {
        frame_buffer_.release();
        if(sender_ != nullptr) {
            nozzle_sender_destroy(sender_);
            sender_ = nullptr;
        }
    }

    void renderOpenGL() override {
        if(finished_.load() || sender_ == nullptr) return;
        if(!frame_buffer_.isValid()) {
            if(!frame_buffer_.initialise(context_, (int)options_.width, (int)options_.height)) {
                fail("opengl framebuffer initialise failed");
                return;
            }
        }

        if(!frame_buffer_.makeCurrentRenderingTarget()) {
            fail("opengl framebuffer target failed");
            return;
        }

        render_quadrants_for_nozzle_oracle();
        frame_buffer_.releaseAsRenderingTarget();

        const NozzleErrorCode error = nozzle_sender_publish_gl_texture(
            sender_,
            (uint32_t)frame_buffer_.getTextureID(),
            (uint32_t)juce::gl::GL_TEXTURE_2D,
            options_.width,
            options_.height,
            NOZZLE_FORMAT_RGBA8_UNORM
        );
        if(error != NOZZLE_OK) {
            fail(juce::String("opengl publish failed: ") + nozzle_error_name(error));
            return;
        }

        const uint32_t frame = published_frames_++;
        if(frame == 0u || frame + 1u == options_.frames) {
            std::printf(
                "standalone opengl sender smoke published %ux%u frame=%u source=%s transfer_mode=%s\n",
                options_.width,
                options_.height,
                frame,
                options_.source_name.c_str(),
                opengl_transfer_mode()
            );
            std::fflush(stdout);
        }
        if(options_.frames <= published_frames_) {
            finish(0);
        }
        juce::Thread::sleep((int)options_.interval_ms);
    }

    void render_quadrants_for_nozzle_oracle() {
        juce::gl::glViewport(0, 0, (int)options_.width, (int)options_.height);
        juce::gl::glDisable(juce::gl::GL_SCISSOR_TEST);
        juce::gl::glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        juce::gl::glClear(juce::gl::GL_COLOR_BUFFER_BIT);
        juce::gl::glEnable(juce::gl::GL_SCISSOR_TEST);

        const int width = (int)options_.width;
        const int height = (int)options_.height;
        const int marker_width = std::max(1, width / 4);
        const int marker_height = std::max(1, height / 4);

        // nozzle's macOS CGL/IOSurface publish path maps GL row 0 to canonical top row.
        // Therefore GL bottom-left becomes receiver top-left.
        clear_rect(0, 0, marker_width, marker_height, 1.0f, 0.0f, 0.0f); // receiver TL red
        clear_rect(width - marker_width, 0, marker_width, marker_height, 0.0f, 1.0f, 0.0f); // receiver TR green
        clear_rect(0, height - marker_height, marker_width, marker_height, 0.0f, 0.0f, 1.0f); // receiver BL blue
        clear_rect(width - marker_width, height - marker_height, marker_width, marker_height, 1.0f, 1.0f, 1.0f); // receiver BR white

        juce::gl::glDisable(juce::gl::GL_SCISSOR_TEST);
        juce::gl::glFlush();
    }

    static void clear_rect(int x, int y, int width, int height, float red, float green, float blue) {
        juce::gl::glScissor(x, y, width, height);
        juce::gl::glClearColor(red, green, blue, 1.0f);
        juce::gl::glClear(juce::gl::GL_COLOR_BUFFER_BIT);
    }

    void fail(const juce::String &message) {
        std::fprintf(stderr, "standalone opengl sender smoke failed: %s\n", message.toRawUTF8());
        std::fflush(stderr);
        finish(1);
    }

    void finish(int code) {
        if(finished_.exchange(true)) return;
        result_code_.store(code);
        juce::MessageManager::callAsync([code]() {
            if(auto *application = juce::JUCEApplicationBase::getInstance()) {
                application->setApplicationReturnValue(code);
                application->quit();
            }
        });
    }

    smoke_sender_options options_;
    juce::OpenGLContext context_;
    juce::OpenGLFrameBuffer frame_buffer_;
    NozzleSender *sender_{nullptr};
    uint32_t published_frames_{0};
    std::atomic<bool> finished_{false};
    std::atomic<int> result_code_{1};
};

class opengl_smoke_window final : public juce::DocumentWindow {
public:
    explicit opengl_smoke_window(smoke_sender_options options)
    : juce::DocumentWindow("Nozzle OpenGL Sender Smoke", juce::Colours::black, 0)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new opengl_smoke_component(std::move(options)), true);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    void closeButtonPressed() override {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

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

        sender_ = std::make_unique<juce_nozzle::sender_client>(juce_nozzle::juce_message_thread_policy());
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
        const smoke_sender_options smoke_options = parse_smoke_sender_options(command_line);
        if(smoke_options.enabled && smoke_options.opengl_enabled) {
            opengl_smoke_window_ = std::make_unique<opengl_smoke_window>(smoke_options);
            return;
        }
        if(smoke_options.enabled) {
            setApplicationReturnValue(run_smoke_sender(smoke_options));
            quit();
            return;
        }
        main_window_ = std::make_unique<main_window>(getApplicationName());
    }

    void shutdown() override {
        opengl_smoke_window_.reset();
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
    std::unique_ptr<opengl_smoke_window> opengl_smoke_window_;
};

START_JUCE_APPLICATION(sender_application)
