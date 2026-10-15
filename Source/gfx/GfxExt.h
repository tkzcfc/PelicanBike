// Minimal per-backend extensions for features ax::backend (axmol release/2.x) has no API for.
// Everything here is optional: each call reports whether it took effect and the caller falls back to
// the portable path otherwise. Only the OpenGL / GLES backend implements them (GfxExt.cpp); on Metal
// every function is a no-op returning false/nullptr.
//
//  feature                  | backend core API          | GL extension here                         | Metal
//  -------------------------|---------------------------|-------------------------------------------|------------------------
//  HDR RGBA16F target       | RGBA32F only              | re-specify the RT texture as RGBA16F       | RGBA32F (core)
//  sRGB texture decode      | none                      | re-upload as SRGB8_ALPHA8                  | decode in shader
//  anisotropic filtering    | none                      | TEXTURE_MAX_ANISOTROPY (if exposed)        | off
//  MSAA scene target        | none (MTL code is #if 0)  | multisample renderbuffer FBO + blit resolve| off (samples = 0)
//  float readback (debug)   | RGBA8 readPixels only     | glReadPixels(GL_FLOAT)                     | n/a
#pragma once
#include "gfx/Gfx.h"
#include <vector>

namespace gfx::ext {
bool textureToHalfFloat(bk::TextureBackend* t, int w, int h);
bool textureToSrgb(bk::TextureBackend* t, const uint8_t* rgba, int w, int h);
void textureAnisotropy(bk::TextureBackend* t, float maxAniso);
void* msaaCreate(RenderTarget& target, int samples);  // nullptr: unsupported
bk::RenderTarget* msaaTarget(void* m);
void msaaResolve(void* m, RenderTarget& target);
void msaaDestroy(void* m);
bool readFloatRGBA(RenderTarget& target, std::vector<float>& out);
const char* backendName();
}  // namespace gfx::ext
