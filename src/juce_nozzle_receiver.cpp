#include "juce_nozzle/juce_nozzle_receiver.hpp"

#include <nozzle/nozzle_c.h>

#include <limits>
#include <sstream>
#include <vector>

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

uint64_t rgba8_size(uint32_t width, uint32_t height) {
    if(width == 0 || height == 0) return 0;
    const uint64_t pixel_count = (uint64_t)width * height;
    if(std::numeric_limits<uint64_t>::max() / 4u < pixel_count) return 0;
    return pixel_count * 4u;
}

void convert_bgra_to_rgba(std::vector<uint8_t> &pixels) {
    for(size_t offset = 0; offset + 3u < pixels.size(); offset += 4u) {
        const uint8_t blue = pixels[offset + 0u];
        pixels[offset + 0u] = pixels[offset + 2u];
        pixels[offset + 2u] = blue;
    }
}

} // namespace

receiver_client::~receiver_client() {
    disconnect();
}

bool receiver_client::connect(const std::string &sender_name, const std::string &application_name) {
    disconnect();
    sender_name_ = sender_name;
    application_name_ = application_name;

    NozzleReceiverDesc desc{};
    desc.name = sender_name_.c_str();
    desc.application_name = application_name_.c_str();
    desc.receive_mode = NOZZLE_RECEIVE_LATEST_ONLY;

    NozzleReceiver *created_receiver = nullptr;
    const NozzleErrorCode error = nozzle_receiver_create(&desc, &created_receiver);
    if(error != NOZZLE_OK || created_receiver == nullptr) {
        last_error_ = status_with_error("receiver_create failed", error);
        receiver_ = nullptr;
        return false;
    }

    receiver_ = created_receiver;
    allowed_thread_ = std::this_thread::get_id();
    last_error_ = "receiver connected";
    return true;
}

void receiver_client::disconnect() {
    if(receiver_ != nullptr) {
        nozzle_receiver_destroy((NozzleReceiver *)receiver_);
        receiver_ = nullptr;
        allowed_thread_ = std::thread::id{};
    }
}

receiver_poll_result receiver_client::poll(uint64_t timeout_ms) {
    receiver_poll_result result{};
    if(receiver_ == nullptr) {
        result.status = "receiver is not connected";
        last_error_ = result.status;
        return result;
    }
    if(allowed_thread_ != std::thread::id{} && std::this_thread::get_id() != allowed_thread_) {
        result.connected = true;
        result.status = "receiver poll rejected: call from the thread that created the receiver; never call from processBlock()";
        last_error_ = result.status;
        return result;
    }

    NozzleAcquireDesc acquire_desc{};
    acquire_desc.timeout_ms = timeout_ms;
    NozzleFrame *frame = nullptr;
    const NozzleErrorCode acquire_error = nozzle_receiver_acquire_frame((NozzleReceiver *)receiver_, &acquire_desc, &frame);
    if(acquire_error == NOZZLE_ERROR_TIMEOUT || acquire_error == NOZZLE_ERROR_SENDER_NOT_FOUND) {
        result.status = status_with_error("waiting for frame", acquire_error);
        last_error_ = result.status;
        return result;
    }
    if(acquire_error != NOZZLE_OK || frame == nullptr) {
        result.status = status_with_error("acquire_frame failed", acquire_error);
        last_error_ = result.status;
        return result;
    }

    NozzleFrameInfo frame_info{};
    const NozzleErrorCode info_error = nozzle_frame_get_info(frame, &frame_info);
    if(info_error != NOZZLE_OK) {
        nozzle_frame_release(frame);
        result.status = status_with_error("frame_get_info failed", info_error);
        last_error_ = result.status;
        return result;
    }

    if(frame_info.semantic_format != NOZZLE_FORMAT_RGBA8_UNORM) {
        nozzle_frame_release(frame);
        result.connected = true;
        result.status = "unsupported source semantic format; sample receiver accepts rgba8_unorm only";
        last_error_ = result.status;
        return result;
    }
    if(frame_info.format != NOZZLE_FORMAT_RGBA8_UNORM && frame_info.format != NOZZLE_FORMAT_BGRA8_UNORM) {
        nozzle_frame_release(frame);
        result.connected = true;
        result.status = "unsupported source storage format; sample receiver accepts rgba8_unorm/bgra8_unorm storage only";
        last_error_ = result.status;
        return result;
    }

    const uint64_t byte_count = rgba8_size(frame_info.width, frame_info.height);
    if(byte_count == 0 || (uint64_t)std::numeric_limits<size_t>::max() < byte_count) {
        nozzle_frame_release(frame);
        result.connected = true;
        result.status = "invalid or oversized frame dimensions";
        last_error_ = result.status;
        return result;
    }

    result.frame.width = frame_info.width;
    result.frame.height = frame_info.height;
    result.frame.frame_index = frame_info.frame_index;
    result.frame.dropped_frame_count = frame_info.dropped_frame_count;
    result.frame.rgba8.resize((size_t)byte_count);

    NozzleMappedPixels copied_pixels{};
    const NozzleErrorCode copy_error = nozzle_frame_copy_pixels_with_origin(
        frame,
        NOZZLE_ORIGIN_TOP_LEFT,
        result.frame.rgba8.data(),
        byte_count,
        &copied_pixels
    );
    nozzle_frame_release(frame);
    if(copy_error != NOZZLE_OK) {
        result.frame = receiver_frame{};
        result.connected = true;
        result.status = status_with_error("copy_pixels failed", copy_error);
        last_error_ = result.status;
        return result;
    }
    if(copied_pixels.format == NOZZLE_FORMAT_BGRA8_UNORM) {
        convert_bgra_to_rgba(result.frame.rgba8);
    }

    NozzleConnectedSenderInfo sender_info{};
    const NozzleErrorCode connected_info_error = nozzle_receiver_get_connected_info((NozzleReceiver *)receiver_, &sender_info);
    if(connected_info_error == NOZZLE_OK) {
        result.frame.estimated_fps = sender_info.estimated_fps;
    }

    result.has_frame = true;
    result.connected = true;
    result.status = "frame received";
    last_error_ = result.status;
    return result;
}

bool receiver_client::is_connected() const {
    return receiver_ != nullptr;
}

std::string receiver_client::last_error() const {
    return last_error_;
}

} // namespace juce_nozzle
