# Standalone smoke procedure

This document separates runtime evidence from build/package evidence. CI runs both the in-process `NozzleStandaloneSmoke` helper baseline and a separate-process standalone app smoke using the built sender/receiver app binaries. These tests do not prove DAW/plugin-host behavior or a JUCE GPU/native texture path.

## Current standalone targets

- `Nozzle Sender Standalone`
  - publishes `rgba8_unorm` CPU-filled frames through `juce_nozzle::sender_client`.
  - permits storage-compatible fallback so macOS Metal can store the IOSurface as `bgra8_unorm` while preserving `rgba8_unorm` semantics.
  - default source name: `juce_nozzle_sender`.
  - default size: `320x240`.
  - test-pattern corners: top-left red, top-right green, bottom-left blue, bottom-right white.
- `Nozzle Receiver Standalone`
  - receives `rgba8_unorm` semantic frames through `juce_nozzle::receiver_client`.
  - accepts `rgba8_unorm` and `bgra8_unorm` storage and exposes converted RGBA bytes to the sample UI.
  - default source name: `juce_nozzle_sender`.

Both standalone examples run nozzle work from the JUCE message-thread timer. They are not audio processors and have no audio callback.

## CI runtime baseline

`NozzleStandaloneSmoke` runs in CI on the build matrix. It creates a sender and receiver in one process, publishes CPU-filled frames at `320x240` and `641x479`, copies them back through the receiver helper, and verifies corner colors:

- top-left red;
- top-right green;
- bottom-left blue;
- bottom-right white.

This catches the basic no-flip and no-R/B-swap failure modes for the CPU writable-frame path.

`python3 scripts/standalone-app-smoke.py --build-dir build` also runs in CI. It launches the built `Nozzle Sender Standalone` and `Nozzle Receiver Standalone` binaries as separate processes for `320x240` and `641x479`, then verifies the same corner colors through the receiver app smoke mode.

This still does not prove a human-visible GUI session, `nozzle-viewer` interop, `nozzle-tester` interop, DAW/plugin-host behavior, or a JUCE GPU/native texture path.

## Required manual runtime evidence before claiming broader GUI/app support

Record the following for each platform/backend tested:

1. Platform, OS version, GPU, and nozzle backend.
2. Sender process and receiver process.
3. Source name, dimensions, and format.
4. Observed frame count and approximate fps.
5. Screenshot or log evidence that:
   - top-left is red;
   - top-right is green;
   - bottom-left is blue;
   - bottom-right is white;
   - no vertical flip is visible;
   - no red/blue channel swap is visible.
6. Repeat open/close or connect/disconnect at least 10 times and record whether frames continue after reconnect.
7. Record failures as failures; do not convert them into support claims.

## Minimal smoke matrix

- `Nozzle Sender Standalone` -> `nozzle-viewer`, `320x240`.
- `Nozzle Sender Standalone` -> `nozzle-viewer`, `641x479`.
- `nozzle-tester` sender -> `Nozzle Receiver Standalone`, `320x240`.
- `nozzle-tester` sender -> `Nozzle Receiver Standalone`, `641x479`.
- `Nozzle Sender Standalone` -> `Nozzle Receiver Standalone`, both sizes. This is covered by CI headless app smoke; add screenshots/manual logs before making a GUI-visible support claim.

Plugin-host smoke is separate. A standalone pass does not imply DAW/VST3/AU host support.
