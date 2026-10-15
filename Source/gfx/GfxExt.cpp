#include "gfx/GfxExt.h"
#include "base/Logging.h"
#include "renderer/backend/RenderTarget.h"
#if defined(AX_USE_GL)
#  include "platform/GL.h"
#  include "renderer/backend/opengl/OpenGLState.h"
#  include "renderer/backend/opengl/RenderTargetGL.h"
#  if AX_TARGET_PLATFORM == AX_PLATFORM_WASM
#    include <emscripten.h>
#  endif
#endif

namespace gfx::ext {
#if defined(AX_USE_GL)
using namespace ax::backend;
static void drainErrors() { while (glGetError() != GL_NO_ERROR) {} }

const char* backendName() {
#  if defined(AX_GLES_PROFILE) && AX_GLES_PROFILE
    return "GLES3";
#  else
    return "OpenGL";
#  endif
}

bool textureToHalfFloat(TextureBackend* t, int w, int h) {
    static int ok = -1;
    if (ok == 0) return false;
    drainErrors();
    __gl->bindTexture(GL_TEXTURE_2D, GLuint(t->getHandler()));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
    bool good = glGetError() == GL_NO_ERROR;
    if (!good) {  // keep the backend's RGBA32F storage
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        drainErrors();
    }
    if (ok < 0) AXLOGI("[gfx] HDR targets: {}", good ? "RGBA16F (GL ext)" : "RGBA32F (backend)");
    ok = good;
    return good;
}

bool textureToSrgb(TextureBackend* t, const uint8_t* rgba, int w, int h) {
    drainErrors();
    __gl->bindTexture(GL_TEXTURE_2D, GLuint(t->getHandler()));
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    bool good = glGetError() == GL_NO_ERROR;
    if (!good) { glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba); drainErrors(); }
    return good;
}

void textureAnisotropy(TextureBackend* t, float a) {
    static float maxA = -1;
    if (maxA < 0) {
#  if AX_TARGET_PLATFORM == AX_PLATFORM_WASM
        EM_ASM({ try { GLctx.getExtension('EXT_texture_filter_anisotropic'); } catch (e) {} });
#  endif
        maxA = 0; glGetFloatv(0x84FF /*MAX_TEXTURE_MAX_ANISOTROPY*/, &maxA); drainErrors();
    }
    if (maxA <= 1) return;
    __gl->bindTexture(GL_TEXTURE_2D, GLuint(t->getHandler()));
    glTexParameterf(GL_TEXTURE_2D, 0x84FE /*TEXTURE_MAX_ANISOTROPY*/, std::min(a, maxA));
    drainErrors();
}

// MSAA: an axmol RenderTarget without textures whose FBO gets multisample renderbuffers. Axmol clears and
// draws into it like any other target (beginRenderPass binds its FBO); endPass() blits it into the resolve
// target's colour texture.
struct Msaa { ax::backend::RenderTarget* rt = nullptr; GLuint color = 0, depth = 0; int w = 0, h = 0; };

void* msaaCreate(gfx::RenderTarget& target, int samples) {
    GLint maxS = 0; glGetIntegerv(GL_MAX_SAMPLES, &maxS);
    samples = std::min(samples, int(maxS));
    if (samples <= 1) return nullptr;
    GLint fmt = 0;  // match the resolve texture's internal format (blit from MSAA requires equal formats)
#  if !defined(AX_GLES_PROFILE) || !AX_GLES_PROFILE
    __gl->bindTexture(GL_TEXTURE_2D, GLuint(target.color.t->getHandler()));
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &fmt);
#  endif
    if (!fmt) fmt = target.color.t->getTextureFormat() == PixelFormat::RGBA8 ? GL_RGBA8 : GL_RGBA16F;
    {  // the resolve target is never bound by a render pass: attach its texture now
        auto* t = static_cast<RenderTargetGL*>(target.rt); t->bindFrameBuffer(); t->update(); }
    auto* m = new Msaa();
    m->w = target.w; m->h = target.h;
    m->rt = DriverBase::getInstance()->newRenderTarget(nullptr, nullptr, nullptr);
    auto* rtGL = static_cast<RenderTargetGL*>(m->rt);
    drainErrors();
    rtGL->bindFrameBuffer();
    glGenRenderbuffers(1, &m->color);
    glBindRenderbuffer(GL_RENDERBUFFER, m->color);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, fmt, m->w, m->h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m->color);
    glGenRenderbuffers(1, &m->depth);
    glBindRenderbuffer(GL_RENDERBUFFER, m->depth);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, m->w, m->h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m->depth);
    GLenum db = GL_COLOR_ATTACHMENT0; glDrawBuffers(1, &db);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (st != GL_FRAMEBUFFER_COMPLETE || glGetError() != GL_NO_ERROR) {
        AXLOGW("[gfx] MSAA x{} unavailable (0x{:x}), rendering without", samples, st);
        drainErrors(); msaaDestroy(m); return nullptr;
    }
    return m;
}
ax::backend::RenderTarget* msaaTarget(void* p) { return static_cast<Msaa*>(p)->rt; }
void msaaResolve(void* p, gfx::RenderTarget& target) {
    auto* m = static_cast<Msaa*>(p);
    GLuint src = static_cast<RenderTargetGL*>(m->rt)->_FBO, dst = static_cast<RenderTargetGL*>(target.rt)->_FBO;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, src);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst);
    glBlitFramebuffer(0, 0, m->w, m->h, 0, 0, target.w, target.h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, src);  // = axmol's cached binding (the MSAA FBO bound by beginRenderPass)
}
void msaaDestroy(void* p) {
    auto* m = static_cast<Msaa*>(p);
    if (m->color) glDeleteRenderbuffers(1, &m->color);
    if (m->depth) glDeleteRenderbuffers(1, &m->depth);
    AX_SAFE_RELEASE(m->rt);
    delete m;
}
bool readFloatRGBA(gfx::RenderTarget& target, std::vector<float>& out) {
    out.resize(size_t(target.w) * target.h * 4);
    auto* rtGL = static_cast<RenderTargetGL*>(target.rt);
    rtGL->bindFrameBuffer(); rtGL->update();
    glReadPixels(0, 0, target.w, target.h, GL_RGBA, GL_FLOAT, out.data());
    return glGetError() == GL_NO_ERROR;
}
#else  // Metal: no extensions, portable fallbacks are used
const char* backendName() { return "Metal"; }
bool textureToHalfFloat(bk::TextureBackend*, int, int) { return false; }
bool textureToSrgb(bk::TextureBackend*, const uint8_t*, int, int) { return false; }
void textureAnisotropy(bk::TextureBackend*, float) {}
void* msaaCreate(gfx::RenderTarget&, int) { return nullptr; }
bk::RenderTarget* msaaTarget(void*) { return nullptr; }
void msaaResolve(void*, gfx::RenderTarget&) {}
void msaaDestroy(void*) {}
bool readFloatRGBA(gfx::RenderTarget&, std::vector<float>&) { return false; }
#endif
}  // namespace gfx::ext
