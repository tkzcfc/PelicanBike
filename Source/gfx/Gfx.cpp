#include "gfx/Gfx.h"
#include "gfx/GfxExt.h"
#include "renderer/Renderer.h"
#include "renderer/PipelineDescriptor.h"
#include "renderer/backend/ProgramManager.h"
#include "renderer/backend/RenderTarget.h"
#include "base/Director.h"
#include "platform/Image.h"
#include "base/Logging.h"
#include <cfloat>
#include <cstring>

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
    shaderSrgb = srgb && !ext::textureToSrgb(t, src, w, h);  // HW sRGB decode where the backend allows it
    if (mips) {
        t->generateMipmaps();
        t->updateSamplerDescriptor(bk::SamplerDescriptor(bk::SamplerFilter::LINEAR, bk::SamplerFilter::LINEAR_MIPMAP_LINEAR, wrap, wrap));
        ext::textureAnisotropy(t, 8.f);  // textures.js toTexture(): anisotropy 8
    }
    return true;
}
void Texture::release() { AX_SAFE_RELEASE_NULL(t); }

// ---------------- RenderTarget ----------------
void RenderTarget::create(int W, int H, ColorFmt fmt, bool withDepth, int smp, bool nearest) {
    destroy();
    w = W; h = H; samples = 0;
    bk::TextureDescriptor td;
    td.textureUsage = bk::TextureUsage::RENDER_TARGET; td.width = w; td.height = h;
    auto f = nearest ? bk::SamplerFilter::NEAREST : bk::SamplerFilter::LINEAR;
    td.samplerDescriptor = bk::SamplerDescriptor(f, f, bk::SamplerAddressMode::CLAMP_TO_EDGE, bk::SamplerAddressMode::CLAMP_TO_EDGE);
    if (fmt != ColorFmt::None) {
        td.textureFormat = fmt == ColorFmt::HDR ? bk::PixelFormat::RGBA32F : bk::PixelFormat::RGBA8;
        color.t = driver()->newTexture(td); color.w = w; color.h = h;
        if (fmt == ColorFmt::HDR) ext::textureToHalfFloat(color.t, w, h);  // RGBA16F where possible (half the bandwidth, = main)
    }
    if (withDepth) {
        td.textureFormat = bk::PixelFormat::D24S8;
        td.samplerDescriptor = bk::SamplerDescriptor(bk::SamplerFilter::NEAREST, bk::SamplerFilter::NEAREST, bk::SamplerAddressMode::CLAMP_TO_EDGE, bk::SamplerAddressMode::CLAMP_TO_EDGE);
        depth.t = driver()->newTexture(td); depth.w = w; depth.h = h;
    }
    rt = driver()->newRenderTarget(color.t, depth.t, nullptr);
    if (smp > 0 && fmt != ColorFmt::None) { msaa = ext::msaaCreate(*this, smp); if (msaa) samples = smp; }
}
void RenderTarget::destroy() {
    if (msaa) { ext::msaaDestroy(msaa); msaa = nullptr; }
    AX_SAFE_RELEASE_NULL(rt);
    color.release(); depth.release();
    samples = 0;
}

// ---------------- passes / draws ----------------
static Mesh s_fsTri;
static State s_state;
static bool s_stateDirty = true;
static RenderTarget* s_pass = nullptr;
static bk::RenderTarget* s_passRT = nullptr;
static int s_passW = 0, s_passH = 0;

void initCommon() {
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
        s_passRT = t->msaa ? ext::msaaTarget(t->msaa) : t->rt;
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
    if (s_pass && s_pass->msaa) ext::msaaResolve(s_pass->msaa, *s_pass);
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
