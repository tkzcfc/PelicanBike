// PelicanBike lightweight GL renderer layer ("gfx").
// Runs inside one Axmol CallbackCommand per frame; owns all 3D GL state and restores
// Axmol's GL state cache afterwards (OpenGLState::reset), so Axmol keeps doing 2D/HUD.
#pragma once
#include "platform/GL.h"
// desktop GL 3.3 core (Linux/Windows/macOS) vs GLES 3.0 (Android, WebGL2): gates desktop-only calls (timer queries, PROGRAM_POINT_SIZE)
#if AX_TARGET_PLATFORM == AX_PLATFORM_WASM || (defined(AX_GLES_PROFILE) && AX_GLES_PROFILE)
#  define PB_DESKTOP_GL 0
#else
#  define PB_DESKTOP_GL 1
#endif
#include <cstdint>
#include <string>
#include <vector>

namespace gfx {

struct Stats {
    int draws = 0, tris = 0, passes = 0, stateChanges = 0;
    void reset() { draws = tris = passes = stateChanges = 0; }
};
extern Stats g_stats;

// GLSL source prefix: "#version 330 core" (desktop) or "#version 300 es" (WebGL2)
const char* glslHeader();

struct Shader {
    GLuint prog = 0;
    bool build(const char* name, const std::string& vs, const std::string& fs);
    GLint loc(const char* n) const { return glGetUniformLocation(prog, n); }
    void use() const;
};

enum Attr : GLuint { A_POS = 0, A_NRM = 1, A_COL = 2, A_MAT = 3, A_POS2 = 4, A_NRM2 = 5, A_I0 = 6, A_I1 = 7, A_I2 = 8 };

// Packed vertex: pos f32x3 | normal s8x3+flags u8 | color sRGB u8x4 | mat u8x4  = 24 bytes
struct PVertex {
    float p[3];
    int8_t n[3];
    uint8_t flags;
    uint8_t c[4];
    uint8_t m[4];
};
static_assert(sizeof(PVertex) == 24, "PVertex");

struct Mesh {
    GLuint vao = 0, vbo = 0, ibo = 0;
    GLsizei count = 0;
    GLenum indexType = GL_UNSIGNED_INT;
    GLenum prim = GL_TRIANGLES;
    float bmin[3] = {0, 0, 0}, bmax[3] = {0, 0, 0};
    void createPacked(const PVertex* v, size_t nv, const uint32_t* idx, size_t ni, bool dynamic = false);
    void updatePacked(const PVertex* v, size_t nv);
    void draw(GLsizei instances = 0) const;
    void drawRange(GLsizei first, GLsizei cnt) const;
};

// Per-instance stream (48 B): aI0 = pos.xyz, scale | aI1 = rotY, yScale, -, - | aI2 = tint.rgb, sway phase
struct InstanceBuf {
    GLuint vbo = 0;
    int capacity = 0, count = 0;
    void create(int cap);
    void upload(const float* data, int n);  // n instances * 12 floats
    void attach(GLuint vao) const;          // bind to A_I0/A_I1 with divisor 1
};

struct Texture {
    GLuint id = 0;
    int w = 0, h = 0;
    bool loadPNG(const std::string& path, bool srgb, bool mips, bool repeat);
    void create(int w, int h, GLenum ifmt, GLenum fmt, GLenum type, bool linear, bool clamp = true);
};

struct RenderTarget {
    GLuint fbo = 0;
    Texture color;   // optional
    Texture depth;   // optional (depth texture)
    GLuint msaaFbo = 0, msaaColor = 0, msaaDepth = 0;
    int w = 0, h = 0, samples = 0;
    void create(int w, int h, GLenum colorFmt, bool withDepthTex, int samples = 0, bool depthRB = false);
    void destroy();
    void bindDraw() const;   // binds MSAA fbo if present
    void resolve() const;    // MSAA -> color texture
};

// cheap fullscreen triangle
void drawFullscreen();
void initCommon();

}  // namespace gfx
