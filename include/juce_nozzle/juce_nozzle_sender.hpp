#pragma once

#include <cstdint>
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

    void *sender_{nullptr};
    std::string sender_name_;
    std::string application_name_;
    std::thread::id allowed_thread_{};
    thread_policy thread_policy_{};
    std::string last_error_;
    uint64_t frame_counter_{0};
};

} // namespace juce_nozzle
