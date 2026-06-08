#pragma once

#include <thread>

namespace juce_nozzle {

using thread_policy_check = bool (*)(void *user_data);
using thread_policy_violation_report = void (*)(const char *operation, const char *diagnostic, void *user_data);
using thread_policy_destroy_resource = void (*)(void *resource);
using thread_policy_rejected_destroy = bool (*)(const char *operation, void *resource, thread_policy_destroy_resource destroy_resource, void *user_data);

struct thread_policy {
    const char *required_context{"creator thread"};
    thread_policy_check is_allowed{nullptr};
    void *user_data{nullptr};
    // Called whenever a normal operation or destructor is rejected by the policy.
    thread_policy_violation_report violation_report{nullptr};
    void *violation_user_data{nullptr};
    // Optional destructor-only escape hatch. Return true only after the raw
    // nozzle resource is either destroyed or ownership has been safely accepted
    // by the allowed thread. Returning false is a hard lifecycle failure.
    thread_policy_rejected_destroy rejected_destroy{nullptr};
    void *rejected_destroy_user_data{nullptr};

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
