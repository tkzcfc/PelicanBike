#include "gfx/Gfx.h"
#include "renderer/Renderer.h"
#include "renderer/PipelineDescriptor.h"
#include "renderer/backend/ProgramManager.h"
#include "renderer/backend/RenderTarget.h"
#include "base/Director.h"
#include "platform/Image.h"
#include "base/Logging.h"
#include <cfloat>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <string>

namespace gfx {
Stats g_stats;

static bk::DriverBase* driver() { return bk::DriverBase::getInstance(); }
static ax::Renderer* renderer() { return ax::Director::getInstance()->getRenderer(); }
static bk::CommandBuffer* cmd() { return renderer()->getCommandBuffer(); }

// ---------------- Shader ----------------
bool Shader::build(const char* vs, const char* fs, VL layout) {
    name = std::string(vs) + "+" + fs;
    prog = bk::ProgramManager::getInstance()->loadProgram(std::string("custom/") + vs + "_vs", std::string("custom/") + fs + "_fs");
    if (!prog) { AXLOGE("[gfx] program {} failed to load/link", name); return false; }
    ps = new bk::ProgramState(prog);
    auto* vl = ps->getMutableVertexLayout();
    auto add = [&](const char* n, bk::VertexFormat f, size_t off, bool norm) {
        int l = prog->getAttributeLocation(n);
        if (l >= 0) vl->setAttrib(n, size_t(l), f, off, norm);
    };
    switch (layout) {
    case VL::Packed:
        add("a_position", bk::VertexFormat::FLOAT3, 0, false);
        add("a_normal", bk::VertexFormat::UBYTE4, 12, true);
        add("a_color", bk::VertexFormat::UBYTE4, 16, true);
        add("a_mat", bk::VertexFormat::UBYTE4, 20, true);
        vl->setStride(sizeof(PVertex));
        break;
    case VL::Post:
        add("a_position", bk::VertexFormat::FLOAT2, 0, false);
        vl->setStride(8);
        break;
    case VL::Particle:
        add("a_position", bk::VertexFormat::FLOAT3, 0, false);
        add("a_color", bk::VertexFormat::FLOAT4, 12, false);
        add("a_size", bk::VertexFormat::FLOAT, 28, false);
        vl->setStride(32);
        break;
    }
    lFrameV = prog->getUniformLocation("vF");
    lFrameF = prog->getUniformLocation("fF");
    return true;
}
const bk::UniformLocation& Shader::loc(const char* n) const {
    auto it = _locs.find(n);
    if (it == _locs.end()) it = _locs.emplace(n, prog->getUniformLocation(n)).first;
    return it->second;
}
void Shader::set(const char* n, const void* d, size_t bytes) const {
    const auto& l = loc(n);
    if (l) ps->setUniform(l, d, bytes);
}
void Shader::setFrame(const void* f, size_t bytes) const {
    if (lFrameV) ps->setUniform(lFrameV, f, bytes);
    if (lFrameF) ps->setUniform(lFrameF, f, bytes);
}
void Shader::tex(const char* n, int slot, bk::TextureBackend* t) const {
    const auto& l = loc(n);
    if (l && t) ps->setTexture(l, slot, t);
}

// ---------------- Mesh ----------------
static std::vector<PVertex> s_up;
static const PVertex* biased(const PVertex* v, size_t nv) {  // s8 normal+flags -> u8 (+128), see PVertex
    s_up.assign(v, v + nv);
    for (auto& p : s_up) { uint8_t* b = reinterpret_cast<uint8_t*>(p.n); for (int k = 0; k < 4; k++) b[k] ^= 0x80; }
    return s_up.data();
}
void Mesh::createPacked(const PVertex* v, size_t nv, const uint32_t* idx, size_t ni, bool dynamic) {
    vb = driver()->newBuffer(nv * sizeof(PVertex), bk::BufferType::VERTEX, dynamic ? bk::BufferUsage::DYNAMIC : bk::BufferUsage::STATIC);
    vb->updateData(biased(v, nv), nv * sizeof(PVertex));
    if (idx && ni) {
        ib = driver()->newBuffer(ni * 4, bk::BufferType::INDEX, bk::BufferUsage::STATIC);
        ib->updateData(idx, ni * 4);
        count = uint32_t(ni);
    } else
        count = uint32_t(nv);
    for (int k = 0; k < 3; k++) { bmin[k] = FLT_MAX; bmax[k] = -FLT_MAX; }
    for (size_t i = 0; i < nv; i++)
        for (int k = 0; k < 3; k++) { bmin[k] = std::min(bmin[k], v[i].p[k]); bmax[k] = std::max(bmax[k], v[i].p[k]); }
}
void Mesh::updatePacked(const PVertex* v, size_t nv) { vb->updateData(biased(v, nv), nv * sizeof(PVertex)); }
void Mesh::createRaw(const void* data, size_t bytes, uint32_t vertices, bool dynamic) {
    vb = driver()->newBuffer(bytes, bk::BufferType::VERTEX, dynamic ? bk::BufferUsage::DYNAMIC : bk::BufferUsage::STATIC);
    if (data) vb->updateData(data, bytes);
    count = vertices;
}
void Mesh::updateRaw(const void* data, size_t bytes) { vb->updateSubData(data, 0, bytes); }

// ---------------- InstanceBuf ----------------
static std::vector<float> s_inst;
void InstanceBuf::create(int cap) {
    capacity = cap;
    buf = driver()->newBuffer(size_t(cap) * 64, bk::BufferType::VERTEX, bk::BufferUsage::DYNAMIC);
    s_inst.assign(size_t(cap) * 16, 0.f);
    buf->updateData(s_inst.data(), size_t(cap) * 64);
}
void InstanceBuf::upload(const float* d, int n) {
    count = std::min(n, capacity);
    s_inst.resize(size_t(capacity) * 16);
    for (int i = 0; i < count; i++) { memcpy(&s_inst[i * 16], d + i * 12, 48); s_inst[i * 16 + 12] = s_inst[i * 16 + 13] = s_inst[i * 16 + 14] = 0; s_inst[i * 16 + 15] = 1; }
    buf->updateSubData(s_inst.data(), 0, size_t(count) * 64);
}

// ---------------- Texture ----------------
bool Texture::loadPNG(const std::string& path, bool srgb, bool mips, bool repeat) {
    ax::Image img;
    if (!img.initWithImageFile(path)) { AXLOGE("[gfx] cannot load {}", path); return false; }
    w = img.getWidth(); h = img.getHeight();
    std::vector<uint8_t> rgba;
    const uint8_t* src = img.getData();
    if (img.getBitPerPixel() != 32) {  // expand RGB -> RGBA
        rgba.resize(size_t(w) * h * 4);
        for (size_t i = 0, n = size_t(w) * h; i < n; i++) { rgba[i * 4] = src[i * 3]; rgba[i * 4 + 1] = src[i * 3 + 1]; rgba[i * 4 + 2] = src[i * 3 + 2]; rgba[i * 4 + 3] = 255; }
        src = rgba.data();
    }
    bk::TextureDescriptor td;
    td.textureFormat = bk::PixelFormat::RGBA8; td.textureUsage = bk::TextureUsage::READ; td.width = w; td.height = h;
    auto wrap = repeat ? bk::SamplerAddressMode::REPEAT : bk::SamplerAddressMode::CLAMP_TO_EDGE;
    td.samplerDescriptor = bk::SamplerDescriptor(bk::SamplerFilter::LINEAR, bk::SamplerFilter::LINEAR, wrap, wrap);
    t = driver()->newTexture(td);
    static_cast<bk::Texture2DBackend*>(t)->updateData(const_cast<uint8_t*>(src), w, h, 0);
    shaderSrgb = srgb;  // no sRGB texture format in ax::backend: decoded in the shader
    aniso = mips;
    if (mips && srgb) {
        // sRGB data has no backend format, so build the mip chain on the CPU in linear space (what an SRGB8_ALPHA8
        // texture's glGenerateMipmap would do); averaging the encoded bytes darkens/shifts minified textures.
        static float dec[256]; static bool lut = false;
        if (!lut) { lut = true; for (int i = 0; i < 256; i++) { float c = i / 255.f; dec[i] = c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); } }
        auto enc = [](float l) { l = std::min(1.f, std::max(0.f, l)); float c = l <= 0.0031308f ? l * 12.92f : 1.055f * std::pow(l, 1.f / 2.4f) - 0.055f; return uint8_t(c * 255.f + 0.5f); };
        std::vector<float> cur(size_t(w) * h * 4);
        for (size_t i = 0, n = size_t(w) * h; i < n; i++) { for (int k = 0; k < 3; k++) cur[i * 4 + k] = dec[src[i * 4 + k]]; cur[i * 4 + 3] = src[i * 4 + 3] / 255.f; }
        int cw = w, ch = h; std::vector<float> nxt; std::vector<uint8_t> out;
        for (int level = 1; cw > 1 || ch > 1; level++) {
            int nw = std::max(1, cw / 2), nh = std::max(1, ch / 2);
            nxt.assign(size_t(nw) * nh * 4, 0.f); out.resize(size_t(nw) * nh * 4);
            for (int y = 0; y < nh; y++) for (int x = 0; x < nw; x++) {
                int x0 = std::min(cw - 1, x * 2), x1 = std::min(cw - 1, x * 2 + 1), y0 = std::min(ch - 1, y * 2), y1 = std::min(ch - 1, y * 2 + 1);
                for (int k = 0; k < 4; k++) {
                    float v = 0.25f * (cur[(size_t(y0) * cw + x0) * 4 + k] + cur[(size_t(y0) * cw + x1) * 4 + k] + cur[(size_t(y1) * cw + x0) * 4 + k] + cur[(size_t(y1) * cw + x1) * 4 + k]);
                    nxt[(size_t(y) * nw + x) * 4 + k] = v;
                    out[(size_t(y) * nw + x) * 4 + k] = k < 3 ? enc(v) : uint8_t(std::min(1.f, v) * 255.f + 0.5f);
                }
            }
            static_cast<bk::Texture2DBackend*>(t)->updateData(out.data(), nw, nh, level);
            cur.swap(nxt); cw = nw; ch = nh;
        }
        t->updateSamplerDescriptor(bk::SamplerDescriptor(bk::SamplerFilter::LINEAR, bk::SamplerFilter::LINEAR_MIPMAP_LINEAR, wrap, wrap));
    } else if (mips) {
        t->generateMipmaps();
        t->updateSamplerDescriptor(bk::SamplerDescriptor(bk::SamplerFilter::LINEAR, bk::SamplerFilter::LINEAR_MIPMAP_LINEAR, wrap, wrap));
    }
    return true;
}
void Texture::release() { AX_SAFE_RELEASE_NULL(t); }

// ---------------- RenderTarget ----------------
void RenderTarget::create(int W, int H, ColorFmt fmt, bool withDepth, bool nearest) {
    destroy();
    w = W; h = H;
    bk::TextureDescriptor td;
    td.textureUsage = bk::TextureUsage::RENDER_TARGET; td.width = w; td.height = h;
    if (fmt == ColorFmt::HDR && !caps().hdrLinear) nearest = true;  // incomplete-texture rule: no LINEAR on unfilterable float
    auto f = nearest ? bk::SamplerFilter::NEAREST : bk::SamplerFilter::LINEAR;
    td.samplerDescriptor = bk::SamplerDescriptor(f, f, bk::SamplerAddressMode::CLAMP_TO_EDGE, bk::SamplerAddressMode::CLAMP_TO_EDGE);
    if (fmt != ColorFmt::None) {
        td.textureFormat = fmt == ColorFmt::HDR ? bk::PixelFormat::RGBA32F : bk::PixelFormat::RGBA8;
        color.t = driver()->newTexture(td); color.w = w; color.h = h;
    }
    if (withDepth) {
        td.textureFormat = bk::PixelFormat::D24S8;
        td.samplerDescriptor = bk::SamplerDescriptor(bk::SamplerFilter::NEAREST, bk::SamplerFilter::NEAREST, bk::SamplerAddressMode::CLAMP_TO_EDGE, bk::SamplerAddressMode::CLAMP_TO_EDGE);
        depth.t = driver()->newTexture(td); depth.w = w; depth.h = h;
    }
#ifdef AX_USE_METAL
    // D24S8 is a combined depth+stencil texture on Metal (Depth24Unorm_Stencil8 / Depth32Float_Stencil8 on Apple GPUs).
    // A pipeline whose depthAttachmentPixelFormat is combined but stencilAttachmentPixelFormat is Invalid is rejected,
    // so attach the same texture as stencil too (what ax::RenderTexture does). GL keeps the depth-only attachment.
    rt = driver()->newRenderTarget(color.t, depth.t, depth.t);
#else
    rt = driver()->newRenderTarget(color.t, depth.t, nullptr);
#endif
}
void RenderTarget::destroy() {
    AX_SAFE_RELEASE_NULL(rt);
    color.release(); depth.release();
}

// ---------------- passes / draws ----------------
static Mesh s_fsTri;
static State s_state;
static bool s_stateDirty = true;
static RenderTarget* s_pass = nullptr;
static bk::RenderTarget* s_passRT = nullptr;
static int s_passW = 0, s_passH = 0;

static Caps s_caps; static bool s_capsInit = false;
const Caps& caps() {
    if (s_capsInit) return s_caps;
    s_capsInit = true;
    auto* d = driver();
    std::string v = d->getVersion() ? d->getVersion() : "";
    s_caps.api = v + " / " + (d->getRenderer() ? d->getRenderer() : "");
    auto ext = [&](const char* n) { return d->hasExtension(std::string("GL_") + n) || d->hasExtension(n); };
    if (v.find("OpenGL ES") != std::string::npos || v.find("WebGL") != std::string::npos) {  // GLES 3 / WebGL2
        s_caps.hdrLinear = ext("OES_texture_float_linear");
        s_caps.hdrRenderable = ext("EXT_color_buffer_float");
        s_caps.hdrBlend = ext("EXT_float_blend");
    } else {  // desktop GL 3.3 core: float filtering is core since GL 3.0
        s_caps.hdrLinear = true;
    }
#ifdef AX_USE_METAL
    // Metal: RGBA32Float is only filterable on Apple7/8 when MTLDevice.supports32BitFloatFiltering (Apple9+ always),
    // and ax::backend does not expose that query; the driver version string is a feature-set name ("macOS_GPUFamily..."
    // / "iOS_GPUFamily..."), not something to pattern-match. Always use NEAREST + manual bilinear on Metal (exact same
    // result as hardware LINEAR). Blending into RGBA32Float is supported on all Apple GPU families.
    s_caps.hdrLinear = false;
#endif
    if (const char* f = std::getenv("PB2_MANUAL_BILINEAR")) s_caps.hdrLinear = std::atoi(f) == 0;
    AXLOGI("[gfx] backend {} | RGBA32F linear {} renderable {} blend {}", s_caps.api, s_caps.hdrLinear, s_caps.hdrRenderable, s_caps.hdrBlend);
    if (!s_caps.hdrRenderable || !s_caps.hdrBlend) AXLOGW("[gfx] float render target / float blending not reported by the driver: HDR passes may fail");
    return s_caps;
}
void initCommon() {
    caps();
    const float tri[6] = {-1, -1, 3, -1, -1, 3};
    s_fsTri.createRaw(tri, sizeof(tri), 3, false);
}
int passW() { return s_passW; }
int passH() { return s_passH; }

void beginPass(RenderTarget* t, bool clearColor, const float* rgba, bool clearDepth) {
    auto* cb = cmd();
    bk::RenderPassDescriptor d;
    d.flags.clear = bk::TargetBufferFlags::NONE;
    if (clearColor) { d.flags.clear |= bk::TargetBufferFlags::COLOR; d.clearColorValue = {rgba[0], rgba[1], rgba[2], rgba[3]}; }
    if (clearDepth) { d.flags.clear |= bk::TargetBufferFlags::DEPTH; d.clearDepthValue = 1.f; }
    s_pass = t;
    int vx = 0, vy = 0;
    if (t) {
        s_passRT = t->rt;
        s_passW = t->w; s_passH = t->h;
    } else {
        auto* r = renderer();
        s_passRT = r->getRenderTarget();
        const auto& vp = r->getViewport();
        vx = vp.x; vy = vp.y; s_passW = int(vp.width); s_passH = int(vp.height);
    }
    cb->beginRenderPass(s_passRT, d);
    cb->setViewport(vx, vy, s_passW, s_passH);
    cb->setScissorRect(false, 0, 0, 0, 0);
    cb->setCullMode(bk::CullMode::NONE);
    cb->setWinding(bk::Winding::COUNTER_CLOCK_WISE);
    s_stateDirty = true;
    g_stats.passes++;
}
void endPass() {
    cmd()->endRenderPass();
    s_pass = nullptr; s_passRT = nullptr;
}
void setState(const State& s) {
    if (s.depthTest != s_state.depthTest || s.depthWrite != s_state.depthWrite || s.depthFunc != s_state.depthFunc) s_stateDirty = true;
    s_state = s;
}
const State& state() { return s_state; }

static void applyState() {
    if (!s_stateDirty) return;
    bk::DepthStencilDescriptor dd;
    dd.flags = bk::DepthStencilFlags::NONE;
    if (s_state.depthTest) dd.addFlag(bk::DepthStencilFlags::DEPTH_TEST);
    if (s_state.depthWrite) dd.addFlag(bk::DepthStencilFlags::DEPTH_WRITE);
    dd.depthCompareFunction = s_state.depthFunc;
    cmd()->updateDepthStencilState(dd);
    s_stateDirty = false;
    g_stats.stateChanges++;
}
static ax::PipelineDescriptor pipeline(const Shader& s) {
    ax::PipelineDescriptor pd;
    pd.programState = s.ps;
    auto& b = pd.blendDescriptor;
    if (s_state.blend != Blend::Off) {
        b.blendEnabled = true;
        b.sourceRGBBlendFactor = b.sourceAlphaBlendFactor = bk::BlendFactor::SRC_ALPHA;
        b.destinationRGBBlendFactor = b.destinationAlphaBlendFactor = s_state.blend == Blend::Alpha ? bk::BlendFactor::ONE_MINUS_SRC_ALPHA : bk::BlendFactor::ONE;
    }
    return pd;
}
static void bindCommon(const Shader& s, const Mesh& m) {
    auto* cb = cmd();
    applyState();
    cb->setVertexBuffer(m.vb);
    auto pd = pipeline(s);
    cb->updatePipelineState(s_passRT, pd);
    cb->setProgramState(s.ps);
}
void draw(const Shader& s, const Mesh& m, const InstanceBuf* inst) {
    if (!m.vb || !s.ps) return;
    if (inst && inst->count <= 0) return;
    auto* cb = cmd();
    bindCommon(s, m);
    int n = inst ? inst->count : 1;
    if (m.ib) {
        cb->setIndexBuffer(m.ib);
        if (inst) {
            cb->setInstanceBuffer(inst->buf);
            cb->drawElementsInstanced(m.prim, bk::IndexFormat::U_INT, m.count, 0, inst->count);
        } else
            cb->drawElements(m.prim, bk::IndexFormat::U_INT, m.count, 0);
    } else
        cb->drawArrays(m.prim, 0, m.count);
    g_stats.draws++;
    g_stats.tris += int(m.count / 3) * n;
}
void drawArrays(const Shader& s, const Mesh& m, bk::PrimitiveType prim, uint32_t first, uint32_t count) {
    if (!m.vb || !s.ps || !count) return;
    bindCommon(s, m);
    cmd()->drawArrays(prim, first, count);
    g_stats.draws++;
}
void drawFullscreen(const Shader& s) {
    bindCommon(s, s_fsTri);
    cmd()->drawArrays(bk::PrimitiveType::TRIANGLE, 0, 3);
    g_stats.draws++;
}
}  // namespace gfx
