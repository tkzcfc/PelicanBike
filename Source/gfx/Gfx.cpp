#include "gfx/Gfx.h"
#include "platform/Image.h"
#include "platform/FileUtils.h"
#include "base/Logging.h"
#include <cfloat>
#if AX_TARGET_PLATFORM == AX_PLATFORM_WASM
#include <emscripten.h>
#endif

namespace gfx {
Stats g_stats;
static GLuint s_emptyVao = 0;

const char* glslHeader() {
#if !PB_DESKTOP_GL
    return "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\nprecision highp sampler2DShadow;\n";
#else
    return "#version 330 core\n";
#endif
}

static GLuint compile(GLenum type, const std::string& src, const char* name) {
    GLuint s = glCreateShader(type);
    std::string full = std::string(glslHeader()) + src;
    const char* p = full.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        AXLOGE("[gfx] shader {} ({}) compile error:\n{}", name, type == GL_VERTEX_SHADER ? "vs" : "fs", log);
    }
    return s;
}

bool Shader::build(const char* name, const std::string& vs, const std::string& fs) {
    GLuint v = compile(GL_VERTEX_SHADER, vs, name), f = compile(GL_FRAGMENT_SHADER, fs, name);
    prog = glCreateProgram();
    glAttachShader(prog, v);
    glAttachShader(prog, f);
    glBindAttribLocation(prog, A_POS, "aPos");
    glBindAttribLocation(prog, A_NRM, "aNrm");
    glBindAttribLocation(prog, A_COL, "aCol");
    glBindAttribLocation(prog, A_MAT, "aMat");
    glBindAttribLocation(prog, A_POS2, "aPos2");
    glBindAttribLocation(prog, A_NRM2, "aNrm2");
    glBindAttribLocation(prog, A_I0, "aI0");
    glBindAttribLocation(prog, A_I1, "aI1");
    glBindAttribLocation(prog, A_I2, "aI2");
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    glDeleteShader(v);
    glDeleteShader(f);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        AXLOGE("[gfx] program {} link error:\n{}", name, log);
        return false;
    }
    return true;
}
void Shader::use() const { glUseProgram(prog); g_stats.stateChanges++; }

static void setupPackedAttribs(GLuint base, GLuint posLoc, GLuint nrmLoc, bool full) {
    const GLsizei S = sizeof(PVertex);
    glEnableVertexAttribArray(posLoc);
    glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, S, (void*)(uintptr_t)(base + 0));
    glEnableVertexAttribArray(nrmLoc);
    glVertexAttribPointer(nrmLoc, 4, GL_BYTE, GL_TRUE, S, (void*)(uintptr_t)(base + 12));
    if (full) {
        glEnableVertexAttribArray(A_COL);
        glVertexAttribPointer(A_COL, 4, GL_UNSIGNED_BYTE, GL_TRUE, S, (void*)(uintptr_t)(base + 16));
        glEnableVertexAttribArray(A_MAT);
        glVertexAttribPointer(A_MAT, 4, GL_UNSIGNED_BYTE, GL_TRUE, S, (void*)(uintptr_t)(base + 20));
    }
}

void Mesh::createPacked(const PVertex* v, size_t nv, const uint32_t* idx, size_t ni, bool dynamic) {
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, nv * sizeof(PVertex), v, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    setupPackedAttribs(0, A_POS, A_NRM, true);
    // aPos2/aNrm2 default to the same stream (rider frame lerp overrides)
    setupPackedAttribs(0, A_POS2, A_NRM2, false);
    if (idx && ni) {
        glGenBuffers(1, &ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, ni * 4, idx, GL_STATIC_DRAW);
        count = (GLsizei)ni;
    } else
        count = (GLsizei)nv;
    glBindVertexArray(0);
    for (int k = 0; k < 3; k++) { bmin[k] = FLT_MAX; bmax[k] = -FLT_MAX; }
    for (size_t i = 0; i < nv; i++)
        for (int k = 0; k < 3; k++) { bmin[k] = std::min(bmin[k], v[i].p[k]); bmax[k] = std::max(bmax[k], v[i].p[k]); }
}
void Mesh::updatePacked(const PVertex* v, size_t nv) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, nv * sizeof(PVertex), v);
}
void Mesh::draw(GLsizei instances) const {
    glBindVertexArray(vao);
    if (ibo) {
        if (instances) glDrawElementsInstanced(prim, count, indexType, nullptr, instances);
        else glDrawElements(prim, count, indexType, nullptr);
    } else {
        if (instances) glDrawArraysInstanced(prim, 0, count, instances);
        else glDrawArrays(prim, 0, count);
    }
    g_stats.draws++;
    g_stats.tris += (count / 3) * (instances ? instances : 1);
}
void Mesh::drawRange(GLsizei first, GLsizei cnt) const {
    glBindVertexArray(vao);
    glDrawElements(prim, cnt, indexType, (void*)(uintptr_t)(first * 4));
    g_stats.draws++;
    g_stats.tris += cnt / 3;
}

void InstanceBuf::create(int cap) {
    capacity = cap;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, cap * 48, nullptr, GL_DYNAMIC_DRAW);
}
void InstanceBuf::upload(const float* d, int n) {
    count = std::min(n, capacity);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, count * 48, d);
}
void InstanceBuf::attach(GLuint vao) const {
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(A_I0);
    glVertexAttribPointer(A_I0, 4, GL_FLOAT, GL_FALSE, 48, (void*)0);
    glVertexAttribDivisor(A_I0, 1);
    glEnableVertexAttribArray(A_I1);
    glVertexAttribPointer(A_I1, 4, GL_FLOAT, GL_FALSE, 48, (void*)16);
    glVertexAttribDivisor(A_I1, 1);
    glEnableVertexAttribArray(A_I2);
    glVertexAttribPointer(A_I2, 4, GL_FLOAT, GL_FALSE, 48, (void*)32);
    glVertexAttribDivisor(A_I2, 1);
    glBindVertexArray(0);
}

bool Texture::loadPNG(const std::string& path, bool srgb, bool mips, bool repeat) {
    ax::Image img;
    if (!img.initWithImageFile(path)) { AXLOGE("[gfx] cannot load {}", path); return false; }
    w = img.getWidth(); h = img.getHeight();
    bool alpha = img.hasAlpha();
    int bpp = img.getBitPerPixel();
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    (void)alpha;
    std::vector<uint8_t> rgba;
    const uint8_t* src = img.getData();
    if (bpp != 32) {  // expand RGB -> RGBA (SRGB8 is not colour-renderable => no mip generation on WebGL2)
        rgba.resize(size_t(w) * h * 4);
        for (size_t i = 0, n = size_t(w) * h; i < n; i++) { rgba[i * 4] = src[i * 3]; rgba[i * 4 + 1] = src[i * 3 + 1]; rgba[i * 4 + 2] = src[i * 3 + 2]; rgba[i * 4 + 3] = 255; }
        src = rgba.data();
    }
    glTexImage2D(GL_TEXTURE_2D, 0, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, src);
    if (mips) glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mips ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    if (mips) {  // textures.js toTexture(): anisotropy 8 (EXT_texture_filter_anisotropic)
      static float maxA = -1;
      if (maxA < 0) {
#if AX_TARGET_PLATFORM == AX_PLATFORM_WASM
        EM_ASM({ try { GLctx.getExtension('EXT_texture_filter_anisotropic'); } catch (e) {} });
#endif
        maxA = 0; glGetFloatv(0x84FF, &maxA); while (glGetError() != GL_NO_ERROR) {}
      }
      if (maxA > 1) { glTexParameterf(GL_TEXTURE_2D, 0x84FE, std::min(8.f, maxA)); while (glGetError() != GL_NO_ERROR) {} }
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLenum wrap = repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    return true;
}
void Texture::create(int W, int H, GLenum ifmt, GLenum fmt, GLenum type, bool linear, bool clamp) {
    w = W; h = H;
    if (!id) glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, ifmt, w, h, 0, fmt, type, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
}

void RenderTarget::create(int W, int H, GLenum colorFmt, bool withDepthTex, int smp, bool depthRB) {
    destroy();
    w = W; h = H; samples = smp;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    if (colorFmt) {
        GLenum type = (colorFmt == GL_RGBA16F || colorFmt == GL_R11F_G11F_B10F) ? GL_HALF_FLOAT : GL_UNSIGNED_BYTE;
        GLenum fmt = colorFmt == GL_R11F_G11F_B10F ? GL_RGB : GL_RGBA;
        color.create(w, h, colorFmt, fmt, type, true);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color.id, 0);
    }
    if (withDepthTex) {
        depth.create(w, h, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, true);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth.id, 0);
    } else if (depthRB && !smp) {
        glGenRenderbuffers(1, &msaaDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, msaaDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msaaDepth);
    }
    if (!colorFmt) {
        GLenum none = GL_NONE;
        glDrawBuffers(1, &none);
        glReadBuffer(GL_NONE);
    }
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (st != GL_FRAMEBUFFER_COMPLETE) AXLOGE("[gfx] FBO incomplete 0x{:x} ({}x{})", st, w, h);
    if (smp > 0) {
        glGenFramebuffers(1, &msaaFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, msaaFbo);
        glGenRenderbuffers(1, &msaaColor);
        glBindRenderbuffer(GL_RENDERBUFFER, msaaColor);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, smp, colorFmt, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msaaColor);
        glGenRenderbuffers(1, &msaaDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, msaaDepth);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, smp, GL_DEPTH_COMPONENT24, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msaaDepth);
        st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (st != GL_FRAMEBUFFER_COMPLETE) AXLOGE("[gfx] MSAA FBO incomplete 0x{:x}", st);
    }
}
void RenderTarget::destroy() {
    if (fbo) glDeleteFramebuffers(1, &fbo);
    if (msaaFbo) glDeleteFramebuffers(1, &msaaFbo);
    if (msaaColor) glDeleteRenderbuffers(1, &msaaColor);
    if (msaaDepth) glDeleteRenderbuffers(1, &msaaDepth);
    if (color.id) glDeleteTextures(1, &color.id);
    if (depth.id) glDeleteTextures(1, &depth.id);
    fbo = msaaFbo = msaaColor = msaaDepth = 0;
    color.id = depth.id = 0;
}
void RenderTarget::bindDraw() const {
    glBindFramebuffer(GL_FRAMEBUFFER, msaaFbo ? msaaFbo : fbo);
    glViewport(0, 0, w, h);
    g_stats.passes++;
}
void RenderTarget::resolve() const {
    if (!msaaFbo) return;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

void initCommon() { glGenVertexArrays(1, &s_emptyVao); }
void drawFullscreen() {
    glBindVertexArray(s_emptyVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    g_stats.draws++;
}
}  // namespace gfx
