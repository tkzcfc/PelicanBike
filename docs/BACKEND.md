# Rendering backend (ax::backend port)

All 3D rendering goes through `ax::backend`, with no raw GL anywhere in `Source`.

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

## No raw GL
There are no `gl*` calls, GL headers or GL-only `#if` paths in `Source`. The old `GfxExt` extension layer is gone.
Each missing backend feature is replaced as follows:

| Feature (old GL extension) | Replacement on every backend |
|---|---|
| RGBA16F HDR targets | `PixelFormat::RGBA32F`, the only float colour format in ax::backend (see below) |
| SRGB8_ALPHA8 textures | Shader decode (`uOpt.z` bit 0). The mip chain of sRGB textures is built on the CPU in linear space and re-encoded, so minified textures match hardware sRGB mipmaps |
| Anisotropic filtering x8 | Shader emulation (`uOpt.z` bit 1): up to 8 `textureGrad` taps along the major axis of the UV footprint. Only applies to mipmapped textures. `PB2_NOANISO=1` disables it |
| 4x MSAA on the HDR target | FXAA 3.11 (quality path, preset-29 search steps) on the tonemapped sRGB image. The final pass writes into an RGBA8 target, with luma in alpha, and `fxaa.frag` resolves into the window. Set `settings.aa = 0` or `PB2_AA=0` to render straight to the window |
| Float readback (`PB2_DUMPHDR`, `PB2_SWEEP`) | Removed. These were debug tools only |

### RGBA32F requirements, probed via `DriverBase` (`gfx::caps()`)
- **Rendering:** WebGL2/GLES3 need `EXT_color_buffer_float`, and `EXT_float_blend` for the additive and alpha passes. Emscripten enables both automatically when present; they are near-universal on desktop browsers. They are not optional: RGBM or shared-exponent RGBA8 encodings cannot be blended, so they are no replacement.
- **Linear filtering:** WebGL2/GLES3 need `OES_texture_float_linear`. On Metal, iOS/tvOS GPUs before Apple7 cannot filter RGBA32Float.
  - When the probe says filtering is missing, HDR targets are created with NEAREST sampling. This avoids the GLES incomplete-texture rule.
  - The bright, blur and final passes then do the bilinear filtering in the shader (`include/pb_post.glsl`, 4 `texelFetch`).
  - Desktop GL 3.3 and macOS Metal use hardware filtering. `PB2_MANUAL_BILINEAR=1/0` forces either path.

### Shadow map and normals (unchanged)
- The shadow depth is packed into an RGB8 target, with slope-scaled bias in the shader. The lit shader emulates a 2x2 LEQUAL compare with bilinear weights over 5 taps.
- Signed-byte normals are uploaded with XOR 0x80 and decoded by `pbNormal()`.

## Backends
- Linux uses GL 3.3 and WASM uses WebGL2/GLES3. Both are verified.
- macOS/iOS use Metal. It compiles via axslcc MSL output but is unverified at runtime.
  - Y-flips are handled with `#ifdef METAL`: post UV, shadow lookup, and gl_FragCoord in the final pass.
- Windows uses the axmol default `AX_USE_COMPAT_GL=ON`, which is ANGLE on D3D11. axmol 2.x has no native D3D backend.
- Android is kept on GLES 3.0. axmol's GLES 2.0 default lacks instancing and UBOs.
