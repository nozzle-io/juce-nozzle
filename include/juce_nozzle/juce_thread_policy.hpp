#pragma once

#include "juce_nozzle/thread_policy.hpp"

#include <cstdio>

#include <juce_events/juce_events.h>

namespace juce_nozzle {

inline bool is_juce_message_thread(void *user_data) {
    juce::ignoreUnused(user_data);
    return juce::MessageManager::getInstance()->isThisTheMessageThread();
}

inline bool juce_defer_rejected_destroy(const char *operation, void *resource, thread_policy_destroy_resource destroy_resource, void *user_data) {
    juce::ignoreUnused(operation, user_data);
    if(resource == nullptr) return true;
    if(destroy_resource == nullptr) return false;
    const bool queued = juce::MessageManager::callAsync([resource, destroy_resource]() {
        destroy_resource(resource);
    });
    if(!queued) {
        std::fprintf(stderr, "juce_nozzle: failed to queue rejected destroy on the JUCE message thread; resource remains live\n");
    }
    return queued;
}

inline thread_policy juce_message_thread_policy() {
    thread_policy policy;
    policy.required_context = "JUCE message thread";
    policy.is_allowed = is_juce_message_thread;
    policy.rejected_destroy = juce_defer_rejected_destroy;
    return policy;
}

} // namespace juce_nozzle
