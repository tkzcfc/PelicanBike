# Rendering backend (ax::backend port)

All 3D rendering goes through `ax::backend`. There are no raw GL calls outside `Source/gfx/GfxExt.cpp`.

## Layers
- `Source/shaders/*.vert|*.frag`: ESSL 310 sources, plus shared code in `Source/shaders/include`.
  - axslcc compiles them at build time to `axslc/custom/<name>_vs|_fs` for GLSL 330, ESSL 300 and MSL.
  - Note: axslcc only tracks the main file. After you edit an include, touch the shaders that use it.
- `Source/gfx/Gfx.{h,cpp}`: a thin wrapper over the backend.
  - Shader = Program + ProgramState, with a uniform-location cache.
  - Mesh = vertex/index `backend::Buffer`. InstanceBuf = an instance buffer with a 64 B mat4 stride.
  - Texture = `TextureBackend`. RenderTarget = `backend::RenderTarget` plus attachments.
  - Draws are recorded directly on `Renderer::getCommandBuffer()` inside one `CallbackCommand`, with one `beginRenderPass`/`endRenderPass` per pass.
- Uniforms:
  - Per-frame data lives in `vec4 vF[38]` (VS block) and `fF[38]` (FS block); see `include/pb_frame.glsl`.
  - The two blocks need different names because the GL backend flattens block member names.
  - Both backends snapshot uniforms at draw time, so one ProgramState per program is mutated between draws.
- Instancing:
  - GL: a per-instance mat4 attribute `a_instance`.
  - Metal: storage buffer `u_instance[gl_InstanceIndex]`. axmol's convention handles this.

## Emulations, which apply on every backend
- Signed-byte normals: the bytes are uploaded with XOR 0x80 as UBYTE4N and decoded by `pbNormal()`.
- Shadow map: the depth is packed into an RGB8 colour target (24 bit). The shader adds a slope-scaled bias, replacing polygon offset. The lit shader emulates `sampler2DShadow` with linear filtering: a 2x2 `texelFetch` LEQUAL compare with bilinear weights, at 5 taps.
- The shader decodes sRGB when the backend cannot provide sRGB textures (`uOpt.z`).

## Per-backend extensions (`Source/gfx/GfxExt.cpp`)
These are isolated, and each is a no-op that returns false on non-GL backends:

| feature | GL / GLES3 / WebGL2 | Metal |
|---|---|---|
| RGBA16F HDR targets | the texture is re-specified as RGBA16F | RGBA32F through the backend's own format, so no extension is needed |
| sRGB albedo textures | re-uploaded as SRGB8_ALPHA8 | decoded in the shader |
| anisotropic filtering | GL_EXT_texture_filter_anisotropic | none |
| MSAA scene target | multisample renderbuffers in an axmol RenderTarget; `glBlitFramebuffer` resolve at endPass | none, because axmol 2.x Metal MSAA is disabled |
| float readback (debug only) | glReadPixels | not supported |

## Backends
- Linux uses GL 3.3 and WASM uses WebGL2/GLES3. Both are verified.
- macOS/iOS use Metal. It compiles via axslcc MSL output but is unverified at runtime.
  - Y-flips are handled with `#ifdef METAL`: post UV, shadow lookup, and gl_FragCoord in the final pass.
- Windows uses the axmol default `AX_USE_COMPAT_GL=ON`, which is ANGLE on D3D11. axmol 2.x has no native D3D backend.
- Android is kept on GLES 3.0. axmol's GLES 2.0 default lacks instancing and UBOs.
