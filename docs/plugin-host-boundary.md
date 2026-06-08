# Plugin host boundary

`Nozzle Receiver` is a minimal experimental receiver plugin sample, not a DAW support claim. Standalone sender/receiver runtime smoke is the center of #127; plugin-host support remains a separate evidence track.

The safe boundary is:

- no nozzle API calls from `processBlock()`;
- receiver lifecycle is tied to the editor/sample UI;
- frame acquire/copy is driven by a message-thread timer;
- helper clients accept a `juce_nozzle::thread_policy`;
- the sample editor/standalone components construct helpers with the reusable `juce_nozzle::juce_message_thread_policy()`, so connect/create, poll/publish, and disconnect reject calls from non-message threads;
- the default owner-thread policy remains available only for low-level non-GUI smoke/test code and is not a plugin/UI boundary; if first used from an audio callback it can still make that callback thread the owner;
- helper objects are not thread-safe. The policy guard is not synchronization, so one helper must not be accessed concurrently from multiple threads;
- unsupported formats are reported as status, not silently converted;
- first sample scope is `rgba8_unorm` only.

Policy rejection is explicit and non-throwing: the helper returns `false` or a failed result with a diagnostic string. That rejected path is still a misuse diagnostic, not a real-time-safe audio callback API; it may update strings and helper-owned diagnostic state.

Destruction has a separate lifecycle contract. Plugin/editor code must stop timers and call explicit successful `disconnect()` on the message thread before object destruction. Destructors no longer delegate to the fallible public `disconnect()` path. If a connected helper is destroyed from a rejected context, the helper emits a hard diagnostic and invokes the policy violation callback. A policy may also provide `rejected_destroy` to marshal resource destruction back to the allowed context; the callback must return `true` only after destruction completed or ownership was safely transferred. If no callback exists, or if the callback fails, the destructor aborts instead of silently leaking the resource. The reusable JUCE message-thread policy queues rejected teardown onto the JUCE message thread, but that is not a DAW/plugin-host unload guarantee; host-specific lifecycle smoke is still required. This prevents an accidental `processBlock()`-owned helper from becoming a valid same-thread nozzle caller, while also making wrong-thread teardown visible instead of silently leaking.

Runtime support still needs host-specific smoke evidence: host name/version, plugin format, editor open/close lifecycle, sender name, frame count, source format, dimensions, and failure status.
