# Standalone smoke procedure

This document separates runtime evidence from build/package evidence. A passing CI build does not prove JUCE/nozzle runtime interop.

## Current standalone targets

- `Nozzle Sender Standalone`
  - publishes `rgba8_unorm` CPU-filled frames through `juce_nozzle::sender_client`.
  - default source name: `juce_nozzle_sender`.
  - default size: `320x240`.
  - test-pattern corners: top-left red, top-right green, bottom-left blue, bottom-right white.
- `Nozzle Receiver Standalone`
  - receives `rgba8_unorm` frames through `juce_nozzle::receiver_client`.
  - default source name: `juce_nozzle_sender`.

Both standalone examples run nozzle work from the JUCE message-thread timer. They are not audio processors and have no audio callback.

## Required manual runtime evidence before claiming support

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
- `Nozzle Sender Standalone` -> `Nozzle Receiver Standalone`, both sizes.

Plugin-host smoke is separate. A standalone pass does not imply DAW/VST3/AU host support.
