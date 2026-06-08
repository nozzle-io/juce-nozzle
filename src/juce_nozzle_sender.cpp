#include "juce_nozzle/juce_nozzle_sender.hpp"

#include <nozzle/nozzle_c.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>

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

sender_client::~sender_client() {
    disconnect();
}

bool sender_client::connect(const std::string &sender_name, const std::string &application_name) {
    disconnect();
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
        last_error_ = status_with_error("sender_create failed", error);
        sender_ = nullptr;
        return false;
    }

    sender_ = created_sender;
    allowed_thread_ = std::this_thread::get_id();
    last_error_ = "sender connected";
    return true;
}

void sender_client::disconnect() {
    if(sender_ != nullptr) {
        nozzle_sender_destroy((NozzleSender *)sender_);
        sender_ = nullptr;
        allowed_thread_ = std::thread::id{};
    }
}

sender_publish_result sender_client::publish_test_pattern(uint32_t width, uint32_t height) {
    sender_publish_result result{};
    if(sender_ == nullptr) {
        result.status = "sender is not connected";
        last_error_ = result.status;
        return result;
    }
    if(allowed_thread_ != std::thread::id{} && std::this_thread::get_id() != allowed_thread_) {
        result.status = "sender publish rejected: call from the thread that created the sender; never call from processBlock()";
        last_error_ = result.status;
        return result;
    }
    if(width == 0 || height == 0) {
        result.status = "invalid sender dimensions";
        last_error_ = result.status;
        return result;
    }

    NozzleFrame *frame = nullptr;
    NozzleErrorCode error = nozzle_sender_acquire_writable_frame((NozzleSender *)sender_, width, height, NOZZLE_FORMAT_RGBA8_UNORM, &frame);
    if(error != NOZZLE_OK || frame == nullptr) {
        result.status = status_with_error("acquire_writable_frame failed", error);
        last_error_ = result.status;
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
        last_error_ = result.status;
        return result;
    }

    error = nozzle_sender_commit_frame((NozzleSender *)sender_, frame);
    nozzle_frame_release(frame);
    if(error != NOZZLE_OK) {
        result.status = status_with_error("commit_frame failed", error);
        last_error_ = result.status;
        return result;
    }

    result.published = true;
    result.frame_index = frame_counter_;
    frame_counter_ += 1u;
    result.status = "frame published";
    last_error_ = result.status;
    return result;
}

bool sender_client::is_connected() const {
    return sender_ != nullptr;
}

std::string sender_client::last_error() const {
    return last_error_;
}

} // namespace juce_nozzle
