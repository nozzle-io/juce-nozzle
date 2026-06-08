#include <juce_nozzle/juce_nozzle_receiver.hpp>
#include <juce_nozzle/juce_nozzle_sender.hpp>

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

namespace {

bool expect_pixel(const juce_nozzle::receiver_frame &frame, uint32_t x, uint32_t y, uint8_t red, uint8_t green, uint8_t blue) {
    if(frame.width <= x || frame.height <= y) return false;
    const size_t offset = ((size_t)y * frame.width + x) * 4u;
    const uint8_t actual_red = frame.rgba8[offset + 0u];
    const uint8_t actual_green = frame.rgba8[offset + 1u];
    const uint8_t actual_blue = frame.rgba8[offset + 2u];
    if(actual_red == red && actual_green == green && actual_blue == blue) return true;
    std::fprintf(stderr, "pixel mismatch at %u,%u: got rgba(%u,%u,%u), expected rgb(%u,%u,%u)\n", x, y, actual_red, actual_green, actual_blue, red, green, blue);
    return false;
}

bool verify_corners(const juce_nozzle::receiver_frame &frame) {
    const uint32_t left_x = 0;
    const uint32_t right_x = frame.width - 1u;
    const uint32_t top_y = 0;
    const uint32_t bottom_y = frame.height - 1u;
    bool ok = true;
    ok = expect_pixel(frame, left_x, top_y, 255u, 0u, 0u) && ok;
    ok = expect_pixel(frame, right_x, top_y, 0u, 255u, 0u) && ok;
    ok = expect_pixel(frame, left_x, bottom_y, 0u, 0u, 255u) && ok;
    ok = expect_pixel(frame, right_x, bottom_y, 255u, 255u, 255u) && ok;
    return ok;
}

bool run_size(uint32_t width, uint32_t height) {
    const std::string source_name = "juce_nozzle_smoke_" + std::to_string(width) + "x" + std::to_string(height);

    juce_nozzle::sender_client sender;
    if(!sender.connect(source_name, "juce-nozzle smoke sender")) {
        std::fprintf(stderr, "sender connect failed: %s\n", sender.last_error().c_str());
        return false;
    }

    juce_nozzle::receiver_client receiver;
    if(!receiver.connect(source_name, "juce-nozzle smoke receiver")) {
        std::fprintf(stderr, "receiver connect failed: %s\n", receiver.last_error().c_str());
        return false;
    }

    for(int attempt = 0; attempt < 10; attempt++) {
        const juce_nozzle::sender_publish_result publish_result = sender.publish_test_pattern(width, height);
        if(!publish_result.published) {
            std::fprintf(stderr, "publish failed: %s\n", publish_result.status.c_str());
            return false;
        }

        const juce_nozzle::receiver_poll_result poll_result = receiver.poll(250);
        if(poll_result.has_frame) {
            if(poll_result.frame.width != width || poll_result.frame.height != height) {
                std::fprintf(stderr, "frame size mismatch: got %ux%u expected %ux%u\n", poll_result.frame.width, poll_result.frame.height, width, height);
                return false;
            }
            if(!verify_corners(poll_result.frame)) return false;
            std::printf("standalone smoke PASS %ux%u frame=%llu\n", width, height, (unsigned long long)poll_result.frame.frame_index);
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    std::fprintf(stderr, "receiver did not observe frame for %ux%u: %s\n", width, height, receiver.last_error().c_str());
    return false;
}

} // namespace

int main() {
    bool ok = true;
    ok = run_size(320u, 240u) && ok;
    ok = run_size(641u, 479u) && ok;
    return ok ? 0 : 1;
}
