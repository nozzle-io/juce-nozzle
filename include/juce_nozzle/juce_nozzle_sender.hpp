#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include "juce_nozzle/thread_policy.hpp"

namespace juce_nozzle {

struct sender_publish_result {
    bool published{false};
    std::string status;
    uint64_t frame_index{0};
};

class sender_client {
public:
    // Low-level/test policy: the first successful connect() owner thread becomes
    // the only allowed thread. This is not an audio-thread safety boundary for
    // JUCE UI/plugin code; use juce_message_thread_policy() there. This helper
    // is not a synchronization primitive; concurrent calls on one object are
    // unsupported.
    sender_client();
    explicit sender_client(thread_policy policy);
    sender_client(const sender_client &) = delete;
    sender_client(sender_client &&) = delete;
    sender_client &operator=(const sender_client &) = delete;
    sender_client &operator=(sender_client &&) = delete;
    ~sender_client();

    bool connect(const std::string &sender_name, const std::string &application_name);
    bool disconnect();
    sender_publish_result publish_test_pattern(uint32_t width, uint32_t height);
    bool is_connected() const;
    std::string last_error() const;

private:
    bool validate_thread(const char *operation);
    void report_thread_violation(const char *operation, const char *diagnostic);
    void destroy_connected_sender();
    void set_last_error(std::string message);

    void *sender_{nullptr};
    std::string sender_name_;
    std::string application_name_;
    std::thread::id allowed_thread_{};
    thread_policy thread_policy_{};
    mutable std::mutex last_error_mutex_;
    std::string last_error_;
    uint64_t frame_counter_{0};
};

} // namespace juce_nozzle
