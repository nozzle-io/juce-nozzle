#include "juce_nozzle/juce_nozzle_sender.hpp"

#include <nozzle/nozzle_c.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <limits>
#include <sstream>
#include <utility>

namespace juce_nozzle {
namespace {

const char *error_name(NozzleErrorCode error) {
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

std::string status_with_error(const char *prefix, NozzleErrorCode error) {
    std::ostringstream stream;
    stream << prefix << ": " << error_name(error) << " (" << (int)error << ")";
    return stream.str();
}

void destroy_sender_resource(void *resource) {
    if(resource != nullptr) {
        nozzle_sender_destroy((NozzleSender *)resource);
    }
}

uint8_t scaled_channel(uint32_t value, uint32_t maximum) {
    if(maximum == 0) return 0;
    return (uint8_t)((uint64_t)value * 255u / maximum);
}

void write_storage_pixel(uint8_t *pixel, NozzleTextureFormat format, uint8_t red, uint8_t green, uint8_t blue) {
    if(format == NOZZLE_FORMAT_BGRA8_UNORM) {
        pixel[0] = blue;
        pixel[1] = green;
        pixel[2] = red;
        pixel[3] = 255u;
        return;
    }

    pixel[0] = red;
    pixel[1] = green;
    pixel[2] = blue;
    pixel[3] = 255u;
}

void write_test_pixel(uint8_t *pixel, NozzleTextureFormat format, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint64_t frame_index) {
    uint8_t red = scaled_channel(x, 1u < width ? width - 1u : 0u);
    uint8_t green = scaled_channel(y, 1u < height ? height - 1u : 0u);
    uint8_t blue = (uint8_t)((frame_index * 7u) & 0xffu);

    const uint32_t left_limit = width / 4u;
    const uint32_t right_limit = width - (width / 4u);
    const uint32_t top_limit = height / 4u;
    const uint32_t bottom_limit = height - (height / 4u);
    if(x < left_limit && y < top_limit) {
        red = 255u;
        green = 0u;
        blue = 0u;
    } else if(right_limit <= x && y < top_limit) {
        red = 0u;
        green = 255u;
        blue = 0u;
    } else if(x < left_limit && bottom_limit <= y) {
        red = 0u;
        green = 0u;
        blue = 255u;
    } else if(right_limit <= x && bottom_limit <= y) {
        red = 255u;
        green = 255u;
        blue = 255u;
    }

    write_storage_pixel(pixel, format, red, green, blue);
}

NozzleErrorCode fill_test_pattern(NozzleMappedPixels *pixels, uint64_t frame_index) {
    if(pixels == nullptr || pixels->data == nullptr) return NOZZLE_ERROR_INVALID_ARGUMENT;
    if(pixels->format != NOZZLE_FORMAT_RGBA8_UNORM && pixels->format != NOZZLE_FORMAT_BGRA8_UNORM) return NOZZLE_ERROR_UNSUPPORTED_FORMAT;
    if(pixels->row_stride_bytes < 0) return NOZZLE_ERROR_INVALID_ARGUMENT;

    const uint64_t minimum_stride = (uint64_t)pixels->width * 4u;
    if((uint64_t)std::numeric_limits<int64_t>::max() < minimum_stride) return NOZZLE_ERROR_INVALID_ARGUMENT;
    if((uint64_t)pixels->row_stride_bytes < minimum_stride) return NOZZLE_ERROR_INVALID_ARGUMENT;
    if(0 < pixels->height) {
        const uint64_t last_row_index = (uint64_t)pixels->height - 1u;
        const uint64_t stride = (uint64_t)pixels->row_stride_bytes;
        if(std::numeric_limits<uint64_t>::max() / stride < last_row_index) return NOZZLE_ERROR_INVALID_ARGUMENT;
        const uint64_t last_row_offset = last_row_index * stride;
        if(std::numeric_limits<uint64_t>::max() - minimum_stride < last_row_offset) return NOZZLE_ERROR_INVALID_ARGUMENT;
        if((uint64_t)std::numeric_limits<size_t>::max() < last_row_offset + minimum_stride) return NOZZLE_ERROR_INVALID_ARGUMENT;
    }

    uint8_t *base = (uint8_t *)pixels->data;
    for(uint32_t y = 0; y < pixels->height; y++) {
        uint8_t *row = base + ((size_t)y * (size_t)pixels->row_stride_bytes);
        for(uint32_t x = 0; x < pixels->width; x++) {
            write_test_pixel(row + ((size_t)x * 4u), pixels->format, x, y, pixels->width, pixels->height, frame_index);
        }
    }
    return NOZZLE_OK;
}

} // namespace

sender_client::sender_client()
: thread_policy_(owner_thread_policy())
{}

sender_client::sender_client(thread_policy policy)
: thread_policy_(policy)
{}

sender_client::~sender_client() {
    if(sender_ == nullptr) return;

    if(thread_policy_.allows_current_thread(allowed_thread_)) {
        destroy_connected_sender();
        return;
    }

    const char *diagnostic = "sender destructor rejected: connected helper destroyed outside the required thread context; call disconnect() on the allowed thread before destruction";
    std::fprintf(stderr, "juce_nozzle: sender destructor: %s\n", diagnostic);
    report_thread_violation("sender destructor", diagnostic);

    if(thread_policy_.rejected_destroy != nullptr) {
        const bool ownership_transferred = thread_policy_.rejected_destroy("sender destructor", sender_, destroy_sender_resource, thread_policy_.rejected_destroy_user_data);
        if(ownership_transferred) {
            sender_ = nullptr;
            allowed_thread_ = std::thread::id{};
            return;
        }
    }

    std::fprintf(stderr, "juce_nozzle: sender destructor: aborting because connected sender destruction was rejected and no safe destroy handoff succeeded\n");
    assert(false && "sender_client destroyed while connected from a disallowed thread; call disconnect() on the allowed thread before destruction");
    std::abort();
}

bool sender_client::validate_thread(const char *operation) {
    if(thread_policy_.allows_current_thread(allowed_thread_)) return true;

    const char *required_context = thread_policy_.required_context != nullptr ? thread_policy_.required_context : "required thread context";
    std::ostringstream stream;
    stream << operation << " rejected: call from " << required_context << "; never call nozzle APIs from processBlock()";
    const std::string message = stream.str();
    set_last_error(message);
    report_thread_violation(operation, message.c_str());
    return false;
}

void sender_client::report_thread_violation(const char *operation, const char *diagnostic) {
    const char *safe_operation = operation != nullptr ? operation : "sender operation";
    const char *safe_diagnostic = diagnostic != nullptr ? diagnostic : "sender thread policy rejected the operation";
    if(thread_policy_.violation_report != nullptr) {
        thread_policy_.violation_report(safe_operation, safe_diagnostic, thread_policy_.violation_user_data);
    }
}

void sender_client::destroy_connected_sender() {
    nozzle_sender_destroy((NozzleSender *)sender_);
    sender_ = nullptr;
    allowed_thread_ = std::thread::id{};
}

void sender_client::set_last_error(std::string message) {
    std::lock_guard<std::mutex> lock(last_error_mutex_);
    last_error_ = std::move(message);
}

bool sender_client::connect(const std::string &sender_name, const std::string &application_name) {
    if(!validate_thread("sender connect")) return false;
    if(!disconnect()) return false;
    sender_name_ = sender_name;
    application_name_ = application_name;
    frame_counter_ = 0;

    NozzleSenderDesc desc{};
    desc.name = sender_name_.c_str();
    desc.application_name = application_name_.c_str();
    desc.ring_buffer_size = 3;
    desc.fallback_flags_valid = 1;
    desc.fallback_flags = NOZZLE_FALLBACK_STORAGE_COMPATIBLE;

    NozzleSender *created_sender = nullptr;
    const NozzleErrorCode error = nozzle_sender_create(&desc, &created_sender);
    if(error != NOZZLE_OK || created_sender == nullptr) {
        set_last_error(status_with_error("sender_create failed", error));
        sender_ = nullptr;
        return false;
    }

    sender_ = created_sender;
    allowed_thread_ = std::this_thread::get_id();
    set_last_error("sender connected");
    return true;
}

bool sender_client::disconnect() {
    if(sender_ == nullptr) return true;
    if(!validate_thread("sender disconnect")) return false;

    destroy_connected_sender();
    set_last_error("sender disconnected");
    return true;
}

sender_publish_result sender_client::publish_test_pattern(uint32_t width, uint32_t height) {
    sender_publish_result result{};
    if(sender_ == nullptr) {
        result.status = "sender is not connected";
        set_last_error(result.status);
        return result;
    }
    if(!validate_thread("sender publish")) {
        result.status = last_error();
        return result;
    }
    if(width == 0 || height == 0) {
        result.status = "invalid sender dimensions";
        set_last_error(result.status);
        return result;
    }

    NozzleFrame *frame = nullptr;
    NozzleErrorCode error = nozzle_sender_acquire_writable_frame((NozzleSender *)sender_, width, height, NOZZLE_FORMAT_RGBA8_UNORM, &frame);
    if(error != NOZZLE_OK || frame == nullptr) {
        result.status = status_with_error("acquire_writable_frame failed", error);
        set_last_error(result.status);
        return result;
    }

    NozzlePixelMapping *mapping = nullptr;
    NozzleMappedPixels pixels{};
    error = nozzle_frame_lock_writable_pixels_mapping_with_origin(frame, NOZZLE_ORIGIN_TOP_LEFT, &mapping, &pixels);
    if(error == NOZZLE_OK) {
        error = fill_test_pattern(&pixels, frame_counter_);
    }
    if(mapping != nullptr) {
        const NozzleErrorCode unlock_error = nozzle_pixel_mapping_unlock_checked(&mapping);
        if(error == NOZZLE_OK) error = unlock_error;
    }
    if(error != NOZZLE_OK) {
        nozzle_sender_discard_frame((NozzleSender *)sender_, frame);
        nozzle_frame_release(frame);
        result.status = status_with_error("write_test_pattern failed", error);
        set_last_error(result.status);
        return result;
    }

    error = nozzle_sender_commit_frame((NozzleSender *)sender_, frame);
    nozzle_frame_release(frame);
    if(error != NOZZLE_OK) {
        result.status = status_with_error("commit_frame failed", error);
        set_last_error(result.status);
        return result;
    }

    result.published = true;
    result.frame_index = frame_counter_;
    frame_counter_ += 1u;
    result.status = "frame published";
    set_last_error(result.status);
    return result;
}

bool sender_client::is_connected() const {
    return sender_ != nullptr;
}

std::string sender_client::last_error() const {
    std::lock_guard<std::mutex> lock(last_error_mutex_);
    return last_error_;
}

} // namespace juce_nozzle
