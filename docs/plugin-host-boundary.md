# Plugin host boundary

`Nozzle Receiver` is a minimal experimental receiver plugin sample, not a DAW support claim. Standalone sender/receiver runtime smoke is the center of #127; plugin-host support remains a separate evidence track.

The safe boundary is:

- no nozzle API calls from `processBlock()`;
- receiver lifecycle is tied to the editor/sample UI;
- frame acquire/copy is driven by a message-thread timer;
- helper clients accept a `juce_nozzle::thread_policy`;
- the sample editor/standalone components construct helpers with a JUCE message-thread policy, so connect/create, poll/publish, and disconnect/destroy reject calls from non-message threads;
- the default owner-thread policy remains available only for low-level non-GUI smoke code and is not the recommended plugin/UI boundary;
- unsupported formats are reported as status, not silently converted;
- first sample scope is `rgba8_unorm` only.

Policy rejection is explicit and non-throwing: the helper returns `false` or a failed result with a diagnostic string. Destructors do not bypass the policy, so plugin/editor code must stop timers and disconnect helpers on the message thread before object destruction. This prevents an accidental `processBlock()`-owned helper from becoming a valid same-thread nozzle caller.

Runtime support still needs host-specific smoke evidence: host name/version, plugin format, editor open/close lifecycle, sender name, frame count, source format, dimensions, and failure status.
