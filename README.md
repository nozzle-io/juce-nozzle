# juce-nozzle

JUCE integration experiments for [nozzle](https://github.com/nozzle-io/nozzle).

This repository is intentionally conservative: the first usable deliverables are standalone sender/receiver examples plus a simple receiver plugin/example that prove build, packaging, and a safe thread boundary. They do **not** claim that every host graphics lifecycle is solved.

## Current deliverable

- `juce_nozzle` helper library with small `sender_client` and `receiver_client` wrappers around the nozzle C API.
- `Nozzle Sender Standalone` GUI app for publishing `rgba8_unorm` test-pattern frames.
- `Nozzle Receiver Standalone` GUI app for receiving and previewing `rgba8_unorm` frames.
- `Nozzle Receiver` plugin example built as:
  - Standalone plugin wrapper;
  - VST3 plugin;
  - AU component on macOS.

The receiver samples accept `rgba8_unorm` frames only. They receive/copy frames from the JUCE message-thread timer and never perform nozzle work in `processBlock()`. The sender standalone also publishes from the message-thread timer.

## Build

```bash
git submodule update --init --recursive
cmake -S . -B build -DJUCE_NOZZLE_BUILD_EXAMPLES=ON
cmake --build build --config Release
```

CMake fetches JUCE from the pinned `JUCE_NOZZLE_JUCE_TAG` value. The current pin is JUCE `8.0.13`.

On macOS the build produces standalone sender/receiver apps, a standalone plugin wrapper, a VST3 bundle, and an AU component. On Windows it produces standalone sender/receiver apps, a standalone plugin wrapper, and a VST3 bundle. Linux packaging is intentionally deferred until the JUCE/nozzle Linux backend path is clarified.

## Standalone smoke usage

1. Open `Nozzle Sender Standalone`.
2. Use source name `juce_nozzle_sender`, size `320x240` or `641x479`, then press `Start Runtime`.
3. Open `nozzle-viewer` or `Nozzle Receiver Standalone` and connect to `juce_nozzle_sender`.
4. Verify the test pattern corners: top-left red, top-right green, bottom-left blue, bottom-right white. This catches vertical flip and red/blue swap mistakes.

`Nozzle Receiver` can also be opened as the standalone plugin wrapper or as VST3/AU, but DAW/plugin-host runtime support requires separate host smoke evidence. See `docs/standalone-smoke.md` and `docs/plugin-host-boundary.md`.

The preview and diagnostics are intentionally simple. The point is to prove the receive path and thread boundary, not to build a polished host UI.

## Threading boundary

Nozzle calls must not run on an audio callback thread. The example enforces this structurally:

- `processBlock()` only clears audio output;
- receiver creation, acquire, copy, and destruction happen from the editor/message-thread timer;
- standalone sender publish happens from the message-thread timer;
- helper clients reject cross-thread `poll()`/`publish_test_pattern()` calls after creation;
- closing the editor or standalone window destroys the receiver/sender.

This is still an experimental plugin-host sample. Host-specific smoke is required before claiming runtime support for a DAW.

## Release packages

CI publishes moving `latest` packages from `main`:

```text
juce-nozzle-latest-<short_sha>-macos-universal.zip
juce-nozzle-latest-<short_sha>-windows-x64.zip
```

Each package includes README/license docs and the platform build artifacts. macOS packages must contain `Nozzle Receiver.vst3`, `Nozzle Receiver.component`, `Nozzle Receiver.app`, `Nozzle Sender Standalone.app`, and `Nozzle Receiver Standalone.app`. Windows packages must contain `Nozzle Receiver.vst3`, `Nozzle Receiver.exe`, `Nozzle Sender Standalone.exe`, and `Nozzle Receiver Standalone.exe`.

## Licensing boundary

This repository fetches JUCE at build time. JUCE licensing is not hidden by this wrapper: distributing or embedding JUCE-based products must comply with JUCE's AGPLv3 or commercial licence path. The example artifacts are conformance/integration samples, not a licence grant for downstream closed-source products. See `docs/juce-license-boundary.md`.
