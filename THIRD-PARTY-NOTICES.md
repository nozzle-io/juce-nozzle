# Third-party notices

## nozzle

`nozzle` is included as a git submodule at `nozzle/` and is distributed under its repository license.

## JUCE

JUCE is fetched by CMake from `https://github.com/juce-framework/JUCE` at the pinned tag declared by `JUCE_NOZZLE_JUCE_TAG`.

The pinned JUCE source states that JUCE modules are dual-licensed under AGPLv3 and a commercial JUCE licence. This repository does not relicense JUCE. The CI-built binaries are experimental integration/conformance artifacts, not a downstream licence grant. Anyone distributing JUCE-based binaries or source packages must satisfy the applicable JUCE licence terms for their use case.

See `docs/juce-license-boundary.md` for the project stance used by #127.
