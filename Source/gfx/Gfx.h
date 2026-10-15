// PelicanBike render layer ("gfx") on top of Axmol's renderer backend (ax::backend).
// Everything 3D is recorded inside one Axmol CallbackCommand per frame, directly on the Renderer's
// backend::CommandBuffer (beginRenderPass / updatePipelineState / setProgramState / draw* / endRenderPass),
// so the same code drives OpenGL 3.3 core, GLES 3 / WebGL2 and Metal. Shaders are GLSL 310 es sources in
// Source/shaders, cross-compiled per backend by axslcc at build time (axslc/custom/<name>_vs|_fs).
// Features the backend has no API for live in gfx/GfxExt.* (clearly isolated, per backend).
#pragma once
#include "renderer/backend/Backend.h"
#include "renderer/backend/ProgramState.h"
#include "renderer/backend/Program.h"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace gfx {
namespace bk = ax::backend;

struct Stats {
    int draws = 0, tris = 0, passes = 0, stateChanges = 0;
    void reset() { draws = tris = passes = stateChanges = 0; }
};
extern Stats g_stats;

// Vertex streams a program can be fed with
enum class VL { Packed, Post, Particle };

// Packed vertex: pos f32x3 | normal s8x3+flags u8 | color sRGB u8x4 | mat u8x4  = 24 bytes (CPU / .pbm layout).
// On upload the normal bytes are re-biased to unsigned (+128) because the backend has no signed-byte vertex
// format; the shaders decode it back to the exact GL snorm value (pbNormal in pb_frame.glsl).
struct PVertex {
    float p[3];
    int8_t n[3];
    uint8_t flags;
    uint8_t c[4];
    uint8_t m[4];
};
static_assert(sizeof(PVertex) == 24, "PVertex");

// A cross-compiled program + its (single, reused) ProgramState. Uniforms are written into the ProgramState
// right before each draw; both backends snapshot them at draw time (GL: UBO upload, Metal: set*Bytes).
struct Shader {
    bk::Program* prog = nullptr;
    bk::ProgramState* ps = nullptr;
    std::string name;
    bool build(const char* vs, const char* fs, VL layout = VL::Packed);  // vs/fs: Source/shaders basenames
    const bk::UniformLocation& loc(const char* n) const;
    void set(const char* n, const void* d, size_t bytes) const;
    void set4(const char* n, float x, float y = 0, float z = 0, float w = 0) const { float v[4] = {x, y, z, w}; set(n, v, 16); }
    void setMat(const char* n, const float* m16) const { set(n, m16, 64); }
    void setFrame(const void* f, size_t bytes) const;  // vF (vertex) + fF (fragment) copies of FrameUBO
    void tex(const char* n, int slot, bk::TextureBackend* t) const;
    bool hasFrame() const { return bool(lFrameV) || bool(lFrameF); }
 private:
    mutable std::unordered_map<std::string, bk::UniformLocation> _locs;
    bk::UniformLocation lFrameV, lFrameF;
};

struct Mesh {
    bk::Buffer* vb = nullptr;
    bk::Buffer* ib = nullptr;
    uint32_t count = 0;
    bk::PrimitiveType prim = bk::PrimitiveType::TRIANGLE;
    float bmin[3] = {0, 0, 0}, bmax[3] = {0, 0, 0};
    void createPacked(const PVertex* v, size_t nv, const uint32_t* idx, size_t ni, bool dynamic = false);
    void updatePacked(const PVertex* v, size_t nv);
    void createRaw(const void* data, size_t bytes, uint32_t vertices, bool dynamic);  // custom stream (VL::Particle/Post)
    void updateRaw(const void* data, size_t bytes);
};

// Per-instance stream, 64 B: the backend's instancing input is one mat4 (`a_instance` on GL, a storage buffer
// `u_instance[]` on Metal). Columns: aI0, aI1, aI2 (12 floats from the game), 4th column unused.
struct InstanceBuf {
    bk::Buffer* buf = nullptr;
    int capacity = 0, count = 0;
    void create(int cap);
    void upload(const float* data, int n);  // n instances * 12 floats
};

struct Texture {
    bk::TextureBackend* t = nullptr;
    int w = 0, h = 0;
    bool shaderSrgb = false;  // sampled values still need sRGB->linear in the shader (no HW sRGB on this backend)
    bool loadPNG(const std::string& path, bool srgb, bool mips, bool repeat);
    void release();
};

enum class ColorFmt { None, RGBA8, HDR };  // HDR = RGBA16F (GL ext) or RGBA32F (backend core format)

struct RenderTarget {
    bk::RenderTarget* rt = nullptr;
    Texture color, depth;  // depth: D24S8, attachment only (never sampled)
    int w = 0, h = 0, samples = 0;
    void* msaa = nullptr;   // GfxExt multisample companion (GL only)
    void create(int w, int h, ColorFmt fmt, bool withDepth, int samples = 0, bool nearest = false);
    void destroy();
};

// ---- render passes / draws on the Renderer's command buffer ----
enum class Blend { Off, Alpha, Add, PremulAdd };
struct State {
    bool depthTest = true, depthWrite = true;
    bk::CompareFunction depthFunc = bk::CompareFunction::LESS;
    Blend blend = Blend::Off;
};
void initCommon();
// rt == nullptr: Axmol's current render target (the window) with the Renderer's viewport
void beginPass(RenderTarget* rt, bool clearColor, const float* rgba, bool clearDepth);
void endPass();
void setState(const State& s);
const State& state();
void draw(const Shader& s, const Mesh& m, const InstanceBuf* inst = nullptr);
void drawArrays(const Shader& s, const Mesh& m, bk::PrimitiveType prim, uint32_t first, uint32_t count);
void drawFullscreen(const Shader& s);
int passW();
int passH();  // size of the current pass (framebuffer px)

}  // namespace gfx
