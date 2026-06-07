# juce-nozzle

JUCE integration experiments for [nozzle](https://github.com/nozzle-io/nozzle).

This repository is intentionally conservative: the first usable deliverable is a simple receiver plugin/example that proves build, packaging, and a safe thread boundary. It does **not** claim that every host graphics lifecycle is solved.

## Current deliverable

- `juce_nozzle` helper library with a small `receiver_client` wrapper around the nozzle C receiver API.
- `Nozzle Receiver` example built as:
  - Standalone app;
  - VST3 plugin;
  - AU component on macOS.

The sample accepts `rgba8_unorm` frames only. It receives/copies frames from the plugin editor timer/message thread and never performs nozzle work in `processBlock()`.

## Build

```bash
git submodule update --init --recursive
cmake -S . -B build -DJUCE_NOZZLE_BUILD_EXAMPLES=ON
cmake --build build --config Release
```

CMake fetches JUCE from the pinned `JUCE_NOZZLE_JUCE_TAG` value. The current pin is JUCE `8.0.13`.

On macOS the build produces a standalone app, VST3 bundle, and AU component. On Windows it produces a standalone app and VST3 bundle. Linux packaging is intentionally deferred until the JUCE/nozzle Linux backend path is clarified.

## Receiver sample usage

1. Start a nozzle `rgba8_unorm` sender such as `nozzle-tester` sender mode.
2. Open `Nozzle Receiver` as a standalone app or plugin.
3. Enter the nozzle sender name, default `nozzle_tester`.
4. Press `Connect`.

The preview and diagnostics are intentionally simple. The point is to prove the receive path and thread boundary, not to build a polished host UI.

## Threading boundary

Nozzle calls must not run on an audio callback thread. The example enforces this structurally:

- `processBlock()` only clears audio output;
- receiver creation, acquire, copy, and destruction happen from the editor/message-thread timer;
- closing the editor destroys the receiver.

This is still an experimental plugin-host sample. Host-specific smoke is required before claiming runtime support for a DAW.

## Release packages

CI publishes moving `latest` packages from `main`:

```text
juce-nozzle-latest-<short_sha>-macos-universal.zip
juce-nozzle-latest-<short_sha>-windows-x64.zip
```

Each package includes README/license docs and the platform build artifacts. macOS packages must contain `Nozzle Receiver.vst3` and `Nozzle Receiver.component`; Windows packages must contain `Nozzle Receiver.vst3`.

## Licensing boundary

This repository fetches JUCE at build time. JUCE licensing is not hidden by this wrapper: distributing or embedding JUCE-based products must comply with JUCE's license/commercial terms. The example artifacts are conformance/integration samples, not a license grant for downstream closed-source products.
