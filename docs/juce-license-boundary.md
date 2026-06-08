# JUCE license and distribution boundary

`juce-nozzle` currently pins JUCE `8.0.13` in `CMakeLists.txt`.

The pinned JUCE source tree states that JUCE modules are available under AGPLv3 or the commercial JUCE licence. The CI-built `juce-nozzle` binaries are integration/conformance artifacts for this public repository. They are not a downstream licence grant and they are not evidence that a closed-source product may redistribute JUCE without satisfying JUCE's licence terms.

Project stance for #127:

- Source and sample code in this repository are intended to be usable for open integration experiments.
- Binary release zips are experimental sample artifacts, not a supported SDK distribution channel for closed-source JUCE products.
- Downstream projects must choose and satisfy their own JUCE licence path before redistributing derived binaries.
- If this repo later publishes a formal JUCE module/package, the release notes must restate the JUCE licence boundary instead of relying on a generic disclaimer.

Do not remove this boundary when adding plugin-host support. VST3/AU packaging increases, not decreases, the need for explicit licence review.
