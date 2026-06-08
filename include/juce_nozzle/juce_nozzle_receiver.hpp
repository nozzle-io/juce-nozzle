#pragma once

#include <cstdint>
#include <string>
#include <thread>
#include <vector>

namespace juce_nozzle {

struct receiver_frame {
    uint32_t width{0};
    uint32_t height{0};
    uint64_t frame_index{0};
    uint32_t dropped_frame_count{0};
    double estimated_fps{0.0};
    std::vector<uint8_t> rgba8;
};

struct receiver_poll_result {
    bool has_frame{false};
    bool connected{false};
    std::string status;
    receiver_frame frame;
};

class receiver_client {
public:
    receiver_client() = default;
    receiver_client(const receiver_client &) = delete;
    receiver_client(receiver_client &&) = delete;
    receiver_client &operator=(const receiver_client &) = delete;
    receiver_client &operator=(receiver_client &&) = delete;
    ~receiver_client();

    bool connect(const std::string &sender_name, const std::string &application_name);
    void disconnect();
    receiver_poll_result poll(uint64_t timeout_ms);
    bool is_connected() const;
    std::string last_error() const;

private:
    void *receiver_{nullptr};
    std::string sender_name_;
    std::string application_name_;
    std::thread::id allowed_thread_{};
    std::string last_error_;
};

} // namespace juce_nozzle
