# JUCE OpenGL/native texture feasibility

This note records the current JUCE 8.0.13 -> nozzle native/GPU texture feasibility for `juce-nozzle`.

## Boundary

The existing sender path remains a CPU writable-frame baseline:

- `include/juce_nozzle/juce_nozzle_sender.hpp` exposes `sender_client::publish_test_pattern(...)`.
- `src/juce_nozzle_sender.cpp` calls `nozzle_sender_acquire_writable_frame(...)`, locks CPU pixels with `nozzle_frame_lock_writable_pixels_mapping_with_origin(..., NOZZLE_ORIGIN_TOP_LEFT, ...)`, writes pixels on CPU, and commits the frame.

The OpenGL prototype is separate and is only exercised by `Nozzle Sender Standalone --smoke-opengl-sender`.

## JUCE API facts

Local JUCE source is fetched at `JUCE_NOZZLE_JUCE_TAG=8.0.13` in `CMakeLists.txt`.

Relevant JUCE files after configure:

- `build/_deps/juce-src/modules/juce_opengl/opengl/juce_OpenGLContext.h`
- `build/_deps/juce-src/modules/juce_opengl/opengl/juce_OpenGLContext.cpp`
- `build/_deps/juce-src/modules/juce_opengl/opengl/juce_OpenGLFrameBuffer.cpp`
- `build/_deps/juce-src/modules/juce_opengl/native/juce_OpenGL_mac.h`
- `build/_deps/juce-src/modules/juce_opengl/native/juce_OpenGL_windows.h`
- `build/_deps/juce-src/modules/juce_opengl/native/juce_OpenGL_linux.h`

Findings:

- `juce::OpenGLContext::getRawContext()` returns an OS-dependent GL context, not a Metal or D3D texture.
  - macOS: `NSOpenGLContext*`.
  - Windows: WGL/render context pointer.
  - Linux: GLX render context pointer.
- `juce::OpenGLContext::getFrameBufferID()` returns `0` on desktop native contexts.
- `juce::OpenGLFrameBuffer::getTextureID()` is the practical JUCE-owned GL texture handle for a prototype sender.
- JUCE OpenGL does not expose `MTLTexture*` or `ID3D11Texture2D*`; do not call nozzle's native texture API with a JUCE GL texture ID.

## nozzle API facts

The usable nozzle API for JUCE OpenGL is:

- `nozzle_sender_publish_gl_texture(...)` in `nozzle/include/nozzle/nozzle_c.h`.
- `nozzle::gl::publish_gl_texture(...)` in `nozzle/include/nozzle/backends/opengl.hpp`.

Backend behavior in `nozzle/src/backends/opengl/opengl_backend.cpp`:

- macOS: requires a current CGL context, binds the sender writable-frame IOSurface with `CGLTexImageIOSurface2D(...)`, then uses `glBlitFramebuffer(...)`. This is a GPU-side copy/staged GL->IOSurface blit, not zero-copy.
- Windows: requires a current WGL context, uses `glGetTexImage(...)` into CPU memory, then D3D11 staging upload. This is CPU readback + D3D11 staging, not a native GPU path.
- Linux: current GL publish/copy returns unsupported pending DMA-BUF/EGLImage write/read implementation.

## Transfer-mode matrix

| Platform | JUCE standalone path | Transfer mode | Verdict |
|---|---|---|---|
| macOS | `OpenGLFrameBuffer::getTextureID()` -> `nozzle_sender_publish_gl_texture(...)` | `gpu_copy_cgl_iosurface` | Supported prototype. Receiver smoke passes via `Nozzle Receiver Standalone` and `nozzle-viewer`. |
| Windows | `OpenGLFrameBuffer::getTextureID()` -> `nozzle_sender_publish_gl_texture(...)` | `cpu_readback_d3d11_staging` | Build/verification candidate only; do not claim GPU/native texture path. |
| Linux | JUCE GL texture -> nozzle GL publish | unsupported | Current nozzle core returns unsupported for GL/DMA-BUF publish. |
| Plugin hosts | Host/editor-specific OpenGL context | unknown | No DAW/plugin-host support claim without host-specific smoke. |

## Prototype behavior

`Nozzle Sender Standalone --smoke-opengl-sender` attaches a `juce::OpenGLContext`, renders the strict quadrant oracle into a `juce::OpenGLFrameBuffer`, and publishes the FBO texture ID via `nozzle_sender_publish_gl_texture(...)`.

The macOS nozzle CGL/IOSurface path maps GL row 0 to canonical top row. The prototype therefore draws the oracle markers in GL coordinates so receiver-visible top-left/top-right/bottom-left/bottom-right are red/green/blue/white respectively.

The smoke output includes a transfer mode string, e.g. `transfer_mode=gpu_copy_cgl_iosurface`.

## Current evidence

Local macOS evidence after #156:

```text
Nozzle Sender Standalone --smoke-opengl-sender -> nozzle-viewer --smoke-receiver
320x240: PASS, sender_rc=0, viewer_rc=0
641x479: PASS, sender_rc=0, viewer_rc=0

Nozzle Sender Standalone --smoke-opengl-sender -> Nozzle Receiver Standalone --smoke-receiver
320x240: PASS, sender_rc=0, receiver_rc=0
641x479: PASS, sender_rc=0, receiver_rc=0
```

Follow-up issue #156 fixed the macOS CGL/IOSurface metadata boundary: the core GL publish path now preserves the caller-requested `rgba8_unorm` semantic format while using BGRA-compatible IOSurface storage when required by CGL. With that fix, `Nozzle Receiver Standalone --smoke-receiver` also accepts the OpenGL sender and validates the same strict quadrant oracle.

This evidence is a standalone CPU copy-out receiver smoke. It proves the public receiver frame metadata and CPU copy path for the JUCE sample receiver; it is not a claim that every native GPU sampling path has been validated.

## No audio callback claim

This path is a standalone GUI/render-thread prototype. It does not prove safety inside `processBlock()` and makes no DAW/plugin-host support claim.
