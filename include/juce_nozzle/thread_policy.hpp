#pragma once

#include <thread>

namespace juce_nozzle {

using thread_policy_check = bool (*)(void *user_data);

struct thread_policy {
    const char *required_context{"creator thread"};
    thread_policy_check is_allowed{nullptr};
    void *user_data{nullptr};

    bool allows_current_thread(std::thread::id owner_thread) const {
        if(is_allowed != nullptr) return is_allowed(user_data);
        if(owner_thread == std::thread::id{}) return true;
        return std::this_thread::get_id() == owner_thread;
    }
};

inline thread_policy owner_thread_policy() {
    return thread_policy{};
}

} // namespace juce_nozzle
