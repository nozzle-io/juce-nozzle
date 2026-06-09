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

This default CI command still does not prove a human-visible GUI session, `nozzle-viewer` interop, `nozzle-tester` interop, DAW/plugin-host behavior, or a JUCE GPU/native texture path.

On macOS CI, a second smoke invocation covers the OpenGL sender path:

```sh
python3 scripts/standalone-app-smoke.py \
  --build-dir build \
  --evidence-dir build/opengl-standalone-app-smoke-evidence \
  --skip-juce-pair \
  --include-opengl-sender
```

This launches `Nozzle Sender Standalone --smoke-opengl-sender` and `Nozzle Receiver Standalone --smoke-receiver` for `320x240` and `641x479`. The receiver evidence must pass the strict corner oracle and must record macOS CGL/IOSurface metadata as storage `bgra8_unorm`, semantic `rgba8_unorm`, and copied format `bgra8_unorm`. The OpenGL smoke is macOS-only; Windows/Linux are not silently counted as covered.

## External app interop smoke

For issue-level evidence that includes external apps, run the same harness with explicit external executables and `--require-external`:

```sh
python3 scripts/standalone-app-smoke.py \
  --build-dir build \
  --viewer-executable ../nozzle-viewer/build/nozzle-viewer.app/Contents/MacOS/nozzle-viewer \
  --nozzle-tester-cli ../nozzle-tester/build/nozzle-tester-cli \
  --viewer-repo-dir ../nozzle-viewer \
  --tester-repo-dir ../nozzle-tester \
  --evidence-dir build/standalone-app-smoke-evidence \
  --require-external
```

On Windows, pass the corresponding `.exe` paths. On Linux, pass the built ELF executable paths.

The external mode records JSON evidence and stdout/stderr logs under `--evidence-dir`, including:

- `Nozzle Sender Standalone` -> `nozzle-viewer` at `320x240` and `641x479`;
- `nozzle-tester sender --sender-pattern juce-quadrants` -> `Nozzle Receiver Standalone` at `320x240` and `641x479`;
- the baseline `Nozzle Sender Standalone` -> `Nozzle Receiver Standalone` at both sizes unless `--skip-juce-pair` is used.

`--sender-pattern juce-quadrants` is a JUCE-compatible smoke fixture for the required quadrant semantics. It is not evidence that the canonical nozzle-tester gradient/marker oracle is accepted by the JUCE receiver.

If the external command fails, report that failure as the evidence. Do not substitute the default CI smoke pass for external-app interop.

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
