# Plugin host boundary

`Nozzle Receiver` is a minimal receiver sample, not a DAW support claim.

The safe boundary is:

- no nozzle API calls from `processBlock()`;
- receiver lifecycle is tied to the editor/sample UI;
- frame acquire/copy is driven by a message-thread timer;
- unsupported formats are reported as status, not silently converted;
- first sample scope is `rgba8_unorm` only.

Runtime support still needs host-specific smoke evidence: host name/version, plugin format, editor open/close lifecycle, sender name, frame count, source format, dimensions, and failure status.
