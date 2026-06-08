# Plugin host boundary

`Nozzle Receiver` is a minimal experimental receiver plugin sample, not a DAW support claim. Standalone sender/receiver runtime smoke is the center of #127; plugin-host support remains a separate evidence track.

The safe boundary is:

- no nozzle API calls from `processBlock()`;
- receiver lifecycle is tied to the editor/sample UI;
- frame acquire/copy is driven by a message-thread timer;
- helper clients reject cross-thread polling/publishing after creation;
- unsupported formats are reported as status, not silently converted;
- first sample scope is `rgba8_unorm` only.

Runtime support still needs host-specific smoke evidence: host name/version, plugin format, editor open/close lifecycle, sender name, frame count, source format, dimensions, and failure status.
