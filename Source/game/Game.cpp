#include "game/Game.h"
#include "game/Shaders.h"
#include "platform/FileUtils.h"
#include "base/Logging.h"
#include <chrono>
#include "renderer/backend/opengl/OpenGLState.h"
#include <cstring>
#include <cstdlib>

using namespace gfx;
namespace pb {

static double nowMs() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

bool loadPbm(const std::string& file, std::vector<PbmPart>& out) {
  auto data = ax::FileUtils::getInstance()->getDataFromFile(file);
  const uint8_t* p = data.getBytes(); size_t n = data.getSize();
  if (n < 8 || memcmp(p, "PBM1", 4)) { AXLOGE("bad pbm {}", file); return false; }
  uint32_t parts; memcpy(&parts, p + 4, 4); size_t o = 8;
  for (uint32_t k = 0; k < parts; k++) {
    PbmPart part; char nm[33] = {0}; memcpy(nm, p + o, 32); part.name = nm;
    uint32_t nv, ni; memcpy(&nv, p + o + 32, 4); memcpy(&ni, p + o + 36, 4); o += 44;
    part.v.resize(nv); memcpy(part.v.data(), p + o, nv * 24); o += nv * 24;
    part.i.resize(ni); memcpy(part.i.data(), p + o, ni * 4); o += ni * 4;
    out.push_back(std::move(part));
  }
  return true;
}

static void packV(PVertex& v, float x, float y, float z, V3 n, Col c, float rough, float metal, uint8_t flags = 0) {
  auto l2s = [](float c) { c = clampf(c, 0, 1); return uint8_t(std::lround(255 * (c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1 / 2.4f) - 0.055f))); };
  v.p[0] = x; v.p[1] = y; v.p[2] = z;
  v.n[0] = int8_t(std::lround(n.x * 127)); v.n[1] = int8_t(std::lround(n.y * 127)); v.n[2] = int8_t(std::lround(n.z * 127)); v.flags = flags;
  v.c[0] = l2s(c.r); v.c[1] = l2s(c.g); v.c[2] = l2s(c.b); v.c[3] = 255;
  v.m[0] = uint8_t(rough * 255); v.m[1] = uint8_t(metal * 255); v.m[2] = 0; v.m[3] = 255;
}

// ---- HSL helpers on linear values (THREE.Color working space) ----
static void rgb2hsl(const Col& c, float& h, float& s, float& l) {
  float mx = std::max({c.r, c.g, c.b}), mn = std::min({c.r, c.g, c.b});
  l = (mx + mn) / 2; if (mx == mn) { h = s = 0; return; }
  float d = mx - mn; s = l <= 0.5f ? d / (mx + mn) : d / (2 - mx - mn);
  if (mx == c.r) h = (c.g - c.b) / d + (c.g < c.b ? 6 : 0); else if (mx == c.g) h = (c.b - c.r) / d + 2; else h = (c.r - c.g) / d + 4;
  h /= 6;
}
static float hue2rgb(float p, float q, float t) { if (t < 0) t += 1; if (t > 1) t -= 1; if (t < 1.f / 6) return p + (q - p) * 6 * t; if (t < 0.5f) return q; if (t < 2.f / 3) return p + (q - p) * 6 * (2.f / 3 - t); return p; }
static Col hsl2rgb(float h, float s, float l) {
  h = h - std::floor(h); s = clampf(s, 0, 1); l = clampf(l, 0, 1);
  if (s == 0) return {l, l, l};
  float p = l <= 0.5f ? l * (1 + s) : l + s - l * s, q = 2 * l - p;
  return {hue2rgb(q, p, h + 1.f / 3), hue2rgb(q, p, h), hue2rgb(q, p, h - 1.f / 3)};
}

// ---- terrain (world.js buildTerrainGeometry/fillTerrain/terrainColor) ----
static std::vector<float> s_xs, s_zs;
static void terrainGrid() {
  if (!s_xs.empty()) return;
  for (int x = 0; x <= 72; x += 2) s_xs.push_back(float(x));
  for (float z = -34; z < -16; z += 2) s_zs.push_back(z);
  for (float z = -16; z < -3.4f; z += 0.7f) s_zs.push_back(z);
  for (float z : {-3.3f, -2.9f, 2.9f, 3.4f, 3.8f}) s_zs.push_back(z);
  for (float z = 4.6f; z < 22; z += 1.2f) s_zs.push_back(z);
  for (float z = 22; z < 60; z += 2.4f) s_zs.push_back(z);
  for (float z = 60; z <= 136; z += 4) s_zs.push_back(z);
}
static Col terrainColor(double x, float z, float h, V3 n) {
  static const Col grassA = hexLin(0x5d8a34), grassB = hexLin(0x93b453), grassDry = hexLin(0xb8b064), rock = hexLin(0x8c867c), dirt = hexLin(0x9a8866),
                   sand = hexLin(0xead7a6), sandWet = hexLin(0xb89f74), sandDeep = hexLin(0x7d6c52), gravel = hexLin(0x8f877a);
  Col o;
  if (z >= 0) {
    float v = fbm2(x * 0.05, z * 0.05, 3, 3.3) + 0.5f;
    o = lerpc(grassA, grassB, clampf(v, 0, 1));
    o = lerpc(o, grassDry, smoothstepf(9, 22, h) * 0.55f);
    o = lerpc(o, dirt, 1 - smoothstepf(3.6f, 6.5f, z));
    o = lerpc(o, rock, smoothstepf(0.86f, 0.7f, n.y));
  } else {
    float s = -z;
    o = lerpc(gravel, sand, smoothstepf(3.2f, 4.2f, s));
    o = lerpc(o, sandWet, smoothstepf(-1.1f, -1.55f, h));
    o = lerpc(o, sandDeep, smoothstepf(-1.7f, -2.6f, h));
    float g = fbm2(x * 0.3, z * 0.3, 2, 2.2) * 0.12f;
    float hh, ss, ll; rgb2hsl(o, hh, ss, ll); o = hsl2rgb(hh, ss, ll + g);
  }
  return o;
}

void Game::rebuildChunk(int slot, int index) {
  terrainGrid();
  chunkIndex[slot] = index;
  double x0 = double(index) * CHUNK;
  int nx = (int)s_xs.size(), nz = (int)s_zs.size(), W = nx + 2;
  static std::vector<float> H; H.resize(W * nz);
  for (int j = 0; j < nz; j++) for (int i = -1; i <= nx; i++) H[j * W + i + 1] = terrainHeight(x0 + i * 2.0, s_zs[j]);
  terrainScratch.resize(nx * nz);
  for (int j = 0; j < nz; j++) {
    int j0 = std::max(0, j - 1), j1 = std::min(nz - 1, j + 1); float dz = s_zs[j1] - s_zs[j0];
    for (int i = 0; i < nx; i++) {
      float h = H[j * W + i + 1];
      float dhx = (H[j * W + i + 2] - H[j * W + i]) / 4.f, dhz = (H[j1 * W + i + 1] - H[j0 * W + i + 1]) / dz;
      V3 n = V3(-dhx, 1, -dhz).norm();
      Col c = terrainColor(x0 + s_xs[i], s_zs[j], h, n);
      packV(terrainScratch[j * nx + i], s_xs[i], h, s_zs[j], n, c, 0.95f, 0);
    }
  }
  if (!terrain[slot].vao) {
    if (terrainIdx.empty())
      for (int j = 0; j < nz - 1; j++) for (int i = 0; i < nx - 1; i++) { uint32_t a = j * nx + i, b = a + nx; terrainIdx.insert(terrainIdx.end(), {a, b, a + 1, b, b + 1, a + 1}); }
    terrain[slot].createPacked(terrainScratch.data(), terrainScratch.size(), terrainIdx.data(), terrainIdx.size(), true);
  } else terrain[slot].updatePacked(terrainScratch.data(), terrainScratch.size());
  terrain[slot].bmin[0] = 0; terrain[slot].bmax[0] = CHUNK; terrain[slot].bmin[1] = -6; terrain[slot].bmax[1] = 40; terrain[slot].bmin[2] = -34; terrain[slot].bmax[2] = 136;

  // ---- prop placement (identical RNG order to world.js rebuild) ----
  Mulberry rnd(uint32_t(index * 9973 + 17));
  static const float DEN[4] = {0.5f, 0.8f, 1.f, 1.25f}; float den = DEN[std::max(0, std::min(3, settings.quality))];  // main.js QUALITY.density
  const int MAXN[P_N] = {int(std::lround(34 * den)), int(std::lround(26 * den)), 6, 8, 2};
  auto tnorm = [](double wx, float z) { const float e = 0.6f; float hx = terrainHeight(wx - e, z) - terrainHeight(wx + e, z), hz = terrainHeight(wx, z - e) - terrainHeight(wx, z + e); return V3(hx, 2 * e, hz).norm(); };
  auto place = [&](int key, int n, float zmin, float zmax, float sMin, float sMax, auto test) {
    auto& out = chunkInst[slot][key]; out.clear(); int c = 0;
    for (int tries = 0; tries < n * 4 && c < n; tries++) {
      float x = rnd() * CHUNK;
      float z = lerpf(zmin, zmax, std::pow(rnd(), (key == P_ROUND || key == P_PINE) ? 1.6f : 1.f));
      double wx = x0 + x; float h = terrainHeight(wx, z); V3 nn = tnorm(wx, z);
      if (!test(x, z, h, nn)) continue;
      float s = lerpf(sMin, sMax, rnd()); float ry = rnd() * TAU; float ym = lerpf(0.85f, 1.2f, rnd());
      float th = lerpf(-0.03f, 0.05f, rnd()), ts = lerpf(0.f, 0.25f, rnd()), tl = lerpf(0.85f, 1.1f, rnd());
      Col tint = hsl2rgb(th, ts, tl);
      out.insert(out.end(), {x, h - 0.05f, z, s, ry, ym, 0, 0, tint.r, tint.g, tint.b, x});
      c++;
    }
  };
  place(P_ROUND, MAXN[P_ROUND], 7, 125, 0.8f, 1.6f, [&](float x, float z, float, V3 n) { return n.y > 0.8f && perlin().noise((x0 + x) * 0.03, z * 0.03, 4.2) > -0.15; });
  place(P_PINE, MAXN[P_PINE], 16, 130, 0.9f, 1.7f, [](float, float, float h, V3 n) { return n.y > 0.75f && h > 4; });
  place(P_PALM, MAXN[P_PALM], -5.2f, -11.5f, 0.8f, 1.1f, [](float x, float, float, V3) { return std::fabs(x - 30) > 2; });
  place(P_ROCK, MAXN[P_ROCK], -30, 60, 0.3f, 1.1f, [](float, float z, float, V3) { return std::fabs(z) > 4.5f && (z < -12 || z > 5); });
  int nUmb = rnd() < 0.7f ? 2 : 0;
  place(P_UMB, nUmb, -7, -11, 0.9f, 1.05f, [](float, float, float, V3) { return true; });
  auto& pm = chunkInst[slot][P_PALM];
  for (size_t i = 0; i < pm.size(); i += 12) { float lean = (rnd() - 0.5f) * 0.3f; pm[i + 4] = PI / 2 + 0.3f + lean; }
  auto& um = chunkInst[slot][P_UMB];
  for (size_t i = 0; i < um.size(); i += 12) { Col t = hsl2rgb(rnd(), 0.7f, 0.62f); um[i + 8] = t.r; um[i + 9] = t.g; um[i + 10] = t.b; }
  instDirty = true;
}

void Game::uploadInstances() {
  int base = chunkIndex[0];
  for (int s = 1; s < 7; s++) base = std::min(base, chunkIndex[s]);
  instBase = base;
  for (int k = 0; k < P_N; k++) {
    auto& d = instData[k]; d.clear();
    for (int s = 0; s < 7; s++) {
      float off = float(chunkIndex[s] - base) * CHUNK;
      const auto& src = chunkInst[s][k];
      for (size_t i = 0; i < src.size(); i += 12) { d.insert(d.end(), src.begin() + i, src.begin() + i + 12); d[d.size() - 12] += off; }
    }
    inst[k].upload(d.data(), int(d.size() / 12));
  }
  instDirty = false;
}

GLuint makeProgX(Shader& s, const char* name, int mode, const char* vs, const char* fs);
static GLuint makeProg(Shader& s, const char* name, int mode, const char* vs, const char* fs, bool skyfnVS = false) { (void)skyfnVS; return makeProgX(s, name, mode, vs, fs); }
GLuint makeProgX(Shader& s, const char* name, int mode, const char* vs, const char* fs) {
  bool skyfnVS = false;
  std::string def = "#define MODE " + std::to_string(mode) + "\n";
  std::string V = def + glsl::FRAME + (skyfnVS ? glsl::SKYFN : "") + vs;
  std::string Fr = def + glsl::FRAME + glsl::SKYFN + fs;
  s.build(name, V, Fr);
  GLuint bi = glGetUniformBlockIndex(s.prog, "Frame");
  if (bi != GL_INVALID_INDEX) glUniformBlockBinding(s.prog, bi, 0);
  glUseProgram(s.prog);
  GLint l;
  if ((l = s.loc("uTex")) >= 0) glUniform1i(l, 0);
  if ((l = s.loc("uShadow")) >= 0) glUniform1i(l, 1);
  if ((l = s.loc("uSrc")) >= 0) glUniform1i(l, 0);
  if ((l = s.loc("uHdr")) >= 0) glUniform1i(l, 0);
  const char* b[5] = {"uB0", "uB1", "uB2", "uB3", "uB4"};
  for (int i = 0; i < 5; i++) if ((l = s.loc(b[i])) >= 0) glUniform1i(l, 2 + i);
  if ((l = s.loc("uTint")) >= 0) glUniform4f(l, 1, 1, 1, 1);
  if ((l = s.loc("uDbg")) >= 0) glUniform1i(l, std::getenv("PB2_DBG") ? std::atoi(std::getenv("PB2_DBG")) : 0);
  if ((l = s.loc("uDfg")) >= 0) glUniform1f(l, std::getenv("PB2_DFG") ? float(std::atof(std::getenv("PB2_DFG"))) : 0.f);
  if ((l = s.loc("uModel")) >= 0) { ax::Mat4 I; glUniformMatrix4fv(l, 1, GL_FALSE, I.m); }
  return s.prog;
}

void Game::buildStatic() {
  // sky sphere (r=400, 48x24) — positions only; rendered around the camera
  {
    std::vector<PVertex> v; std::vector<uint32_t> idx; const int WS = 48, HS = 24;
    for (int y = 0; y <= HS; y++) for (int x = 0; x <= WS; x++) {
      float th = PI * y / HS, ph = TAU * x / WS; PVertex p{};
      p.p[0] = -400 * std::cos(ph) * std::sin(th); p.p[1] = 400 * std::cos(th); p.p[2] = 400 * std::sin(ph) * std::sin(th); v.push_back(p);
    }
    for (int y = 0; y < HS; y++) for (int x = 0; x < WS; x++) { uint32_t a = y * (WS + 1) + x, b = a + WS + 1; idx.insert(idx.end(), {a, b, a + 1, b, b + 1, a + 1}); }
    sky.createPacked(v.data(), v.size(), idx.data(), idx.size());
  }
  // ocean grid (ocean.js createOcean)
  {
    std::vector<float> xs, zs;
    for (float x = -260; x <= 420; x += (x > -60 && x < 120) ? 1.2f : 4.f) xs.push_back(x);
    for (float z = -8; z >= -520; z -= z > -70 ? 0.9f : (z > -160 ? 2.5f : 8.f)) zs.push_back(z);
    std::vector<PVertex> v; std::vector<uint32_t> idx;
    for (float z : zs) for (float x : xs) { PVertex p{}; p.p[0] = x; p.p[1] = 0; p.p[2] = z; v.push_back(p); }
    uint32_t nx = (uint32_t)xs.size();
    for (uint32_t j = 0; j + 1 < zs.size(); j++) for (uint32_t i = 0; i + 1 < nx; i++) { uint32_t a = j * nx + i, b = a + nx; idx.insert(idx.end(), {a, a + 1, b, b, a + 1, b + 1}); }
    ocean.createPacked(v.data(), v.size(), idx.data(), idx.size());
  }
  // road: 7 chunks long plane (5.64 wide), static
  {
    std::vector<PVertex> v; std::vector<uint32_t> idx;
    for (int k = 0; k <= 7 * 3; k++) for (float z : {-2.82f, 2.82f}) { PVertex p; packV(p, k * 24.f, 0.005f, z, V3(0, 1, 0), Col{1, 1, 1}, 0.82f, 0.02f); v.push_back(p); }
    for (uint32_t k = 0; k < 21; k++) { uint32_t a = k * 2; idx.insert(idx.end(), {a, a + 1, a + 2, a + 1, a + 3, a + 2}); }
    road.createPacked(v.data(), v.size(), idx.data(), idx.size());
  }
  // per-chunk static props replicated over 7 chunks -> one buffer, one draw
  std::vector<PbmPart> props; loadPbm("data/props.pbm", props);
  for (auto& part : props) {
    int key = part.name == "round" ? P_ROUND : part.name == "pine" ? P_PINE : part.name == "palm" ? P_PALM : part.name == "rock" ? P_ROCK : part.name == "umb" ? P_UMB : -1;
    if (key >= 0) {
      prop[key].createPacked(part.v.data(), part.v.size(), part.i.data(), part.i.size());
      inst[key].create(7 * 48);
      inst[key].attach(prop[key].vao);
    } else {
      std::vector<PVertex> v; std::vector<uint32_t> idx;
      for (int c = 0; c < 7; c++) { uint32_t b = (uint32_t)v.size(); for (auto pv : part.v) { pv.p[0] += c * CHUNK; v.push_back(pv); } for (auto i : part.i) idx.push_back(b + i); }
      (part.name == "bulbs" ? bulbs : strip).createPacked(v.data(), v.size(), idx.data(), idx.size());
    }
  }
  if (!rider.load("data/rider2.pbr")) AXLOGE("[pb2] rider2.pbr load failed");
  texRoad.loadPNG("tex/road.png", true, true, true);
  texDetail.loadPNG("tex/detail.png", false, true, true);
}

bool Game::init(int winW, int winH) {
  GLint prevVao = 0, prevFbo = 0; glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVao); glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
  initCommon();
  glGenBuffers(1, &ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(FrameUBO), nullptr, GL_DYNAMIC_DRAW);
  makeProg(skyS, "sky", 0, glsl::SKY_VS, glsl::SKY_FS);
  makeProg(oceanS, "ocean", 0, glsl::OCEAN_VS, glsl::OCEAN_FS);
  makeProg(litStatic, "lit0", 0, glsl::LIT_VS, glsl::LIT_FS);
  makeProg(litInst, "lit1", 1, glsl::LIT_VS, glsl::LIT_FS);
  makeProg(litRider, "lit2", 2, glsl::LIT_VS, glsl::LIT_FS);
  makeProg(litTerrain, "lit3", 3, glsl::LIT_VS, glsl::LIT_FS);
  makeProg(litRoad, "lit4", 4, glsl::LIT_VS, glsl::LIT_FS);
  makeProg(shStatic, "sh0", 0, glsl::LIT_VS, glsl::SHADOW_FS);
  makeProg(shInst, "sh1", 1, glsl::LIT_VS, glsl::SHADOW_FS);
  makeProg(shRider, "sh2", 2, glsl::LIT_VS, glsl::SHADOW_FS);
  makeProg(brightS, "bright", 0, glsl::FS_VS, glsl::BRIGHT_FS);
  makeProg(blurS, "blur", 0, glsl::FS_VS, glsl::BLUR_FS);
  makeProg(finalS, "final", 0, glsl::FS_VS, glsl::FINAL_FS);
  buildStatic();
  buildExtras();
  buildFx();
  audioOk = std::getenv("PB2_NOAUDIO") ? false : audio.init();
  AXLOGI("[pb2] audio {}", audioOk ? "OpenAL stream" : "no device (silent)");
  makeProgX(shBird, "sh6", 6, glsl::LIT_VS, glsl::SHADOW_FS);
  for (int s = 0; s < 7; s++) chunkIndex[s] = -99;
  shadowRT.create(settings.shadowSize, settings.shadowSize, 0, true);
  createTargets(winW, winH);
#ifdef GL_TIME_ELAPSED
#  if PB_DESKTOP_GL
  glGenQueries(2, gpuQuery); gpuQueryOk = true;
#  endif
#endif
  update(0);
  glBindVertexArray(prevVao); glBindFramebuffer(GL_FRAMEBUFFER, prevFbo); glUseProgram(0);
  ax::backend::OpenGLState::reset();
  AXLOGI("[pb2] init done, axmol vao {}", prevVao);
  return true;
}

void Game::createTargets(int w, int h) {
  rtW = std::max(16, int(w * settings.renderScale)); rtH = std::max(16, int(h * settings.renderScale));
  hdrRT.create(rtW, rtH, GL_RGBA16F, false, settings.msaa, true);
  int bw = (rtW + 1) / 2, bh = (rtH + 1) / 2;
  { int qw = std::max(1, rtW / 4), qh = std::max(1, rtH / 4); blurA.create(qw, qh, GL_RGBA16F, false); blurB.create(qw, qh, GL_RGBA16F, false); }
  for (int i = 0; i < 5; i++) { bloomA[i].create(std::max(1, bw), std::max(1, bh), GL_RGBA16F, false); bloomB[i].create(std::max(1, bw), std::max(1, bh), GL_RGBA16F, false); bw /= 2; bh /= 2; }
}

// ---------------- simulation (main.js) ----------------
static const int GEARS[5][2] = {{44, 28}, {44, 22}, {44, 18}, {44, 15}, {44, 12}};
static const float REAR_HUB_X = -0.46f;

void Game::jump() { if (S.airborne || !started) return; S.airborne = true; S.vy = 3.6f; S.jumps++; audio.whoosh(); unlock("jump"); }
void Game::doTrick() { if (S.trickT > 0 || !started) return; S.trickT = 3.4f; S.tricks++; honkReq = true; audio.honk(); trickConfetti = 0.6f; unlock("trick"); if (S.tricks >= 3) unlock("trick3"); }
void Game::doHonk() { honkReq = true; audio.honk(); S.honks++; unlock("honk"); }
void Game::ringBell() { audio.bell(); bellPulse = 0.12f; S.bells++; if (S.bells >= 10) unlock("bell10"); }
void Game::clickAt(float nx, float ny) {
  // ray from camera; pelican (sphere around body/head) -> honk, bike -> bell, sea -> jumping fish
  ax::Mat4 inv = viewProj; inv.inverse(); ax::Vec4 a(nx, ny, -1, 1), b(nx, ny, 1, 1); inv.transformVector(&a); inv.transformVector(&b);
  V3 o(a.x / a.w, a.y / a.w, a.z / a.w), e(b.x / b.w, b.y / b.w, b.z / b.w); V3 d = (e - o).norm();
  auto hit = [&](V3 c, float r) { V3 m = c - o; float t = m.dot(d); return t > 0 && (m - d * t).len() < r; };
  auto w = [&](V3 p) { ax::Vec3 q(p.x, p.y, p.z); riderM.transformPoint(&q); return V3(q.x, q.y, q.z); };
  
  if (hit(w(V3(0.3f, 0.86f, -0.085f)), 0.06f)) { AXLOGI("[pb2] hit bell"); ringBell(); return; }
  if (hit(w(rider.bodyPos()), 0.32f) || hit(w(rider.headPos()), 0.16f)) { AXLOGI("[pb2] hit pelican"); doHonk(); return; }
  if (hit(w(V3(0, 0.45f, 0)), 0.7f)) { ringBell(); return; }
  if (d.y < -1e-3f) { float t = (SEA_LEVEL - o.y) / d.y; V3 p = o + d * t; if (p.z < -12) launchJumper(p.x, p.z); }
}
std::string Game::camName() const { static const char* N[5] = {"orbit", "chase", "side", "cine", "pov"}; return N[camMode]; }

void Game::updateRide(float dt) {
  float acc;
  if (S.airborne) acc = -0.15f; else if (keys.down) acc = -6; else if (keys.up) acc = 2.4f; else acc = (settings.cruise - S.speed) * 0.7f;
  if (S.trickT > 0) acc = std::min(acc, 0.2f);
  S.accel = damp(S.accel, acc, 6, dt);
  S.speed = clampf(S.speed + acc * dt, 0, 15.5f);
  S.pedaling = !S.airborne && !keys.down && S.speed > 0.25f && acc > -0.4f;
  S.distance += double(S.speed) * dt;
  S.wheel += (S.speed / WHEEL_R) * dt;
  float wheelRpm = (S.speed / WHEEL_R) * 60 / TAU;
  if (settings.autoGear && S.time - S.lastGearManual > 8) {
    int best = S.gear; float bestErr = 1e9f;
    for (int i = 0; i < 5; i++) { float cad = wheelRpm / (float(GEARS[i][0]) / GEARS[i][1]); float e = std::fabs(cad - 86); if (e < bestErr) { bestErr = e; best = i; } }
    float cur = wheelRpm / (float(GEARS[S.gear][0]) / GEARS[S.gear][1]);
    if (best != S.gear && (cur > 102 || cur < 68)) { S.gear += best > S.gear ? 1 : -1; audio.click(0.18f); }
  }
  float ratio = float(GEARS[S.gear][0]) / GEARS[S.gear][1];
  float crankW = S.pedaling ? S.speed / WHEEL_R / ratio : 0;
  S.crank += crankW * dt; S.cadence = crankW * 60 / TAU;
  int steerIn = (keys.right ? 1 : 0) - (keys.left ? 1 : 0);
  if (steerIn) { S.lastSteer = S.time; S.laneTarget = clampf(S.laneTarget + steerIn * 2.4f * dt, -2.15f, 2.15f); }
  else if (settings.autopilot && S.time - S.lastSteer > 4) { int nf = nextFish(40); float want = nf >= 0 ? fishes[nf].z : 0.9f + std::sin(S.time * 0.23f) * 0.7f; S.laneTarget = damp(S.laneTarget, want, nf >= 0 ? 2.2f : 0.8f, dt); }
  const float k = 10;
  S.laneV += ((S.laneTarget - S.lane) * k - S.laneV * 2 * std::sqrt(k)) * dt;
  S.lane += S.laneV * dt;
  S.latAcc = damp(S.latAcc, (S.laneV - S.prevLaneV) / std::max(dt, 1e-3f), 8, dt);
  S.prevLaneV = S.laneV;
  float wobble = std::sin(S.time * 1.7f) * 0.02f * clampf(1.5f - S.speed * 0.2f, 0.2f, 1.5f);
  S.lean = damp(S.lean, clampf(std::atan(S.latAcc / 9.8f) * 1.4f, -0.4f, 0.4f) + wobble * 0.5f, 7, dt);
  float steerT = S.trickT > 0 ? 0 : -clampf(S.latAcc * 0.05f + S.laneV * 0.12f / std::max(1.f, S.speed * 0.3f), -0.35f, 0.35f) + wobble;
  S.steer = damp(S.steer, steerT, 10, dt);
  if (S.airborne) { S.vy -= 13 * dt; S.y += S.vy * dt; if (S.y <= 0) { S.y = 0; S.airborne = false; burstDust(24); audio.thump(); } }
  if (trickConfetti > 0 && (trickConfetti -= dt) <= 0) confettiBurst(V3(0.2f, 1.8f, S.lane));
  if (S.trickT > 0) S.trickT -= dt;
  S.trick = damp(S.trick, S.trickT > 0 ? 1.f : 0.f, 4, dt);
  S.wheelie = damp(S.wheelie, S.trick * 0.3f + (S.airborne ? S.vy * 0.025f : 0), 6, dt);
  riderPos = V3(0, S.y, S.lane);
  float yaw = -std::atan2(S.laneV, std::max(S.speed, 1.5f));
  ax::Mat4 T, Ry, Rx, W1, Rz, W2;
  ax::Mat4::createTranslation(riderPos.x, riderPos.y, riderPos.z, &T);
  ax::Mat4::createRotationY(yaw, &Ry); ax::Mat4::createRotationX(S.lean, &Rx);
  ax::Mat4::createTranslation(REAR_HUB_X, 0, 0, &W1); ax::Mat4::createRotationZ(S.wheelie, &Rz); ax::Mat4::createTranslation(-REAR_HUB_X, 0, 0, &W2);
  riderM = T * Ry * Rx * W1 * Rz * W2;
}

void Game::updateLighting(float dt) {
  if (settings.timeFlow && started) settings.hour = std::fmod(settings.hour + dt * settings.dayRate, 24.f);
  V3 sd = sunDirection(settings.hour), md = moonDirection(settings.hour);
  elev = std::asin(clampf(sd.y, -1, 1)) * 180 / PI;
  samplePalette(elev, pal);
  night = nightFactor(elev);
  bool useMoon = elev < -4;
  V3 ld = useMoon ? md : sd;
  Col lc = useMoon ? hexLin(0x9fb6ff) : pal.sun;
  float li = useMoon ? 0.8f * smoothstepf(-4, -12, elev) * clampf(md.y * 3, 0, 1) : pal.li;
  auto s4 = [](float* d, float a, float b, float c, float w) { d[0] = a; d[1] = b; d[2] = c; d[3] = w; };
  s4(F.sunDir, sd.x, sd.y, sd.z, night);
  s4(F.moonDir, md.x, md.y, md.z, 1 - night * 0.9f);
  s4(F.zenith, pal.zen.r, pal.zen.g, pal.zen.b, settings.cloud);
  s4(F.horizon, pal.hor.r, pal.hor.g, pal.hor.b, 0.0052f * settings.fog);
  s4(F.ground, pal.gnd.r, pal.gnd.g, pal.gnd.b, 0.0036f * settings.fog);
  s4(F.sunColor, pal.sun.r, pal.sun.g, pal.sun.b, clampf(pal.li / 3 + pal.hi * 0.25f, 0.08f, 1));
  s4(F.lightDir, ld.x, ld.y, ld.z, li);
  s4(F.lightColor, lc.r, lc.g, lc.b, lerpf(1.0f, 0.22f, night));
  s4(F.hemiSky, pal.hs.r, pal.hs.g, pal.hs.b, pal.hi);
  s4(F.hemiGround, pal.hg.r, pal.hg.g, pal.hg.b, settings.waveAmp);
  s4(F.fogColor, pal.fog.r, pal.fog.g, pal.fog.b, float(std::fmod(S.distance, 36000.0)));
  s4(F.cloudTint, pal.ct.r, pal.ct.g, pal.ct.b, 1.0f / settings.shadowSize);
  s4(F.cloudShade, pal.cs.r, pal.cs.g, pal.cs.b, 0);
  // wave phases in double precision (ocean.js wavePhases)
  static const float WAVES[5][4] = {{0.18f, 1.0f, 0.2f, 21.0f}, {-0.45f, 1.0f, 0.17f, 12.5f}, {0.75f, 0.62f, 0.13f, 7.0f}, {-0.9f, 0.45f, 0.1f, 4.2f}, {0.3f, 1.0f, 0.07f, 2.6f}};
  float ph[5];
  for (int i = 0; i < 5; i++) {
    double kk = 2 * M_PI / WAVES[i][3], c = std::sqrt(9.8 / kk), dx = WAVES[i][0] / std::hypot(WAVES[i][0], WAVES[i][1]);
    ph[i] = float(std::fmod(kk * dx * S.distance - kk * c * S.t, 2 * M_PI));
  }
  s4(F.phaseA, ph[0], ph[1], ph[2], ph[3]);
  s4(F.phaseB, ph[4], night, lerpf(0.f, 6.f, smoothstepf(0.15f, 0.6f, night)), 0);
  exposureVal = pal.exp * settings.exposure;
  if (std::fabs(settings.hour - envHour) > 0.18f || envHour < 0) { envHour = settings.hour; refreshAmbientCube(); }
  bloomStrength = (settings.bloom && qualityBloom ? 1.f : 0.f) * pal.bloom * settings.bloomBoost * lerpf(1, 0.7f, night);
  bloomThreshold = lerpf(1.05f, 0.88f, night);
  warm = smoothstepf(25, 3, elev) * (1 - night) * 0.8f;
  // shadow camera: ortho +-7.5, near 1 far 80, centred on rider, texel-snapped (stable)
  V3 eye = riderPos + ld * 40.f;
  ax::Mat4 lv, lp;
  ax::Mat4::createLookAt(ax::Vec3(eye.x, eye.y, eye.z), ax::Vec3(riderPos.x, riderPos.y, riderPos.z), ax::Vec3(0, 1, 0), &lv);
  ax::Mat4::createOrthographicOffCenter(-7.5f, 7.5f, -7.5f, 7.5f, 1.f, 80.f, &lp);
  ax::Mat4 vp = lp * lv;
  ax::Vec4 o(0, 0, 0, 1); vp.transformVector(&o);
  float texel = 2.f / settings.shadowSize;
  float sx = std::round(o.x / texel) * texel - o.x, sy = std::round(o.y / texel) * texel - o.y;
  ax::Mat4 snap; ax::Mat4::createTranslation(sx, sy, 0, &snap);
  shadowVP = snap * vp;
  ax::Mat4 bias(0.5f, 0, 0, 0.5f, 0, 0.5f, 0, 0.5f, 0, 0, 0.5f, 0.5f, 0, 0, 0, 1);  // row-major ctor
  ax::Mat4 sm = bias * shadowVP;
  memcpy(F.shadowMat, sm.m, 64);
  double shift = double(instBase) * CHUNK - S.distance;
  {  // night lights: two nearest street lamps, bike headlight (spot), camera fill, emissive levels
    double base = std::floor((S.distance - 12) / 36) * 36 + 12; float li2 = smoothstepf(0.25f, 0.75f, night) * 18;
    s4(F.lamp0, float(base - S.distance), 5.2f, 1.95f, li2); s4(F.lamp1, float(base + 36 - S.distance), 5.2f, 1.95f, li2);
    ax::Vec3 hp(0.725f, 0.745f, 0), hd(5, -0.9f, 0); riderM.transformPoint(&hp); riderM.transformVector(&hd); hd.normalize();
    s4(F.headPos, hp.x, hp.y, hp.z, night * 9); s4(F.headDir, hd.x, hd.y, hd.z, night * 2.2f);
    ax::Mat4 cw = view; cw.inverse(); ax::Vec3 fp(0.6f, 0.8f, 0.5f); cw.transformPoint(&fp);
    bool blink = std::sin(S.time * 1000 * 0.008f) > 0.3f;
    s4(F.fillPos, fp.x, fp.y, fp.z, 0); s4(F.nightE, 0.3f + night * 7, 0.4f + night * 5 + (blink ? night * 3 : 0), 0.25f + night * 0.8f, night);
  }
  s4(F.cloudOff, float(S.distance * 0.0009 + S.t * 0.004), S.t * 0.0015f, float(shift), float(std::fmod(S.distance, 200.0)));
}

float Game::groundAt(V3 p) const {
  double wx = p.x + S.distance;
  if (std::fabs(p.z) < 2.9f) return 0.02f;
  if (p.z < -13) return std::max(SEA_LEVEL + 0.4f, terrainHeight(float(wx), p.z));
  return terrainHeight(float(wx), p.z);
}

static ax::Quaternion lookQuat(V3 pos, V3 look) {
  ax::Mat4 m; ax::Mat4::createLookAt(ax::Vec3(pos.x, pos.y, pos.z), ax::Vec3(look.x, look.y, look.z), ax::Vec3(0, 1, 0), &m);
  m.inverse(); ax::Quaternion q; m.getRotation(&q); return q;
}

void Game::setCamMode(int id, bool quick) {
  if (id == camMode && !quick) return;
  camMode = id; blendT = quick ? 0.999f : 0; blendPos = camPos; blendQuat = camQuat; blendFov = fov;
  if (id == CAM_ORBIT) {
    V3 focus(riderPos.x + 0.05f, riderPos.y + 0.92f, riderPos.z);
    V3 d = camPos - focus; float r = d.len();
    if (r > 14 || r < 1.2f || camPos.y < 0.3f) { d = V3(2.3f, 0.45f, 3.7f); r = d.len(); }
    orbR = r; orbTheta = std::atan2(d.x, d.z); orbPhi = std::acos(clampf(d.y / r, -1, 1));
    orbVT = orbVP = 0; orbZoom = 1; lastOrbitInput = S.time;
  }
  if (id == CAM_CINE) cineT = 0;
}
void Game::cycleCam() { setCamMode((camMode + 1) % CAM_N); }
void Game::orbitDrag(float dx, float dy) {  // pixels; OrbitControls rotateSpeed 1: 2*PI*dx/height
  if (camMode != CAM_ORBIT) setCamMode(CAM_ORBIT);
  float h = float(std::max(1, rtH));
  orbVT -= 2 * PI * dx / h * 0.08f * 12.5f; orbVP -= 2 * PI * dy / h * 0.08f * 12.5f;  // damped deltas
  lastOrbitInput = S.time;
}
void Game::orbitZoom(float steps) { if (camMode != CAM_ORBIT) setCamMode(CAM_ORBIT); orbZoom *= std::pow(0.95f, steps); lastOrbitInput = S.time; }

void Game::povPose(V3& pos, V3& look, float& f) {
  ax::Mat4 H = riderM * rider.headM();
  ax::Vec3 p(0.07f, 0.05f, 0), l(1.2f, -0.12f, 0); H.transformPoint(&p); H.transformPoint(&l);
  pos = V3(p.x, p.y, p.z); look = V3(l.x, l.y, l.z); f = 68;
}

void Game::updateCamera(float dt) {
  V3 focus(riderPos.x + 0.05f, riderPos.y + 0.92f, riderPos.z), pos, look; float tf = fov;
  bool fixedPose = true, pov = false; ax::Quaternion q;
  if (camMode == CAM_ORBIT) {
    // OrbitControls with damping 0.08 + autoRotate (0.55) after 7 s idle; maxPolar 0.495 PI, distance 1.1..28
    if (S.time - lastOrbitInput > 7) orbTheta -= 2 * PI / 60 * 0.55f * dt;
    orbTheta += orbVT * 0.08f; orbPhi += orbVP * 0.08f; orbVT *= 1 - 0.08f; orbVP *= 1 - 0.08f;
    orbPhi = clampf(orbPhi, 0.02f, PI * 0.495f);
    orbR = clampf(orbR * orbZoom, 1.1f, 28); orbZoom = 1;
    pos = focus + V3(orbR * std::sin(orbPhi) * std::sin(orbTheta), orbR * std::cos(orbPhi), orbR * std::sin(orbPhi) * std::cos(orbTheta));
    pos.y = std::max(pos.y, groundAt(pos) + 0.2f);
    look = focus; tf = 42; fixedPose = false; q = lookQuat(pos, look);
  } else if (camMode == CAM_CHASE) { pos = focus + V3(-3.5f, 0.7f, 1.2f - S.lane * 0.15f); look = focus + V3(1.6f, 0.05f, 0); tf = 46; }
  else if (camMode == CAM_SIDE) { pos = focus + V3(0.35f + std::sin(S.time * 0.2f) * 0.5f, -0.1f, 3.3f); look = focus + V3(0.1f, -0.05f, 0); tf = 36; }
  else if (camMode == CAM_POV) { povPose(pos, look, tf); pov = true; }
  else {  // cine: 8 scripted shots
    struct Shot { float d; };
    static const float D[8] = {6.5f, 6, 5, 7, 6, 5, 6.5f, 5.5f};
    cineT += dt;
    if (cineT > D[cineShot % 8]) { cineT = 0; cineShot = (cineShot + 1) % 8; cutFlash = 0.4f; }
    float u = cineT / D[cineShot]; V3 p, l;
    switch (cineShot) {
      case 0: p = V3(lerpf(-2.6f, 1.4f, u), -0.5f, 2.0f); l = V3(0.25f, -0.1f, 0); tf = 38; break;
      case 1: p = V3(7.5f - u * 1.2f, -0.05f, -1.1f + u * 0.5f); l = V3(0.05f, 0.35f, 0); tf = 17; break;
      case 2: p = V3(-0.2f + u * 0.3f, -0.5f, 0.95f); l = V3(-0.3f, -0.55f, 0.05f); tf = 34; break;
      case 3: p = V3(-4.5f + u * 2.5f, 1.4f + u * 5, 5 - u * 1.8f); l = V3(0.3f, -0.2f, 0); tf = 40; break;
      case 4: p = V3(2.2f - u * 3, 0.1f, -3.4f); l = V3(0, 0.25f, 0); tf = 30; break;
      case 5: pov = true; break;
      case 6: p = V3(std::cos(u * 1.6f + 0.4f) * 3.2f, 2.6f, std::sin(u * 1.6f + 0.4f) * 3.2f); l = V3(0.2f, 0.1f, 0); tf = 36; break;
      default: p = V3(1.6f, 1.6f - u * 0.4f, 0.35f); l = V3(-0.2f, 0.35f, 0); tf = 55; break;
    }
    if (pov) povPose(pos, look, tf); else { pos = focus + p; look = focus + l; }
  }
  if (fixedPose) {
    if (camMode == CAM_CHASE || camMode == CAM_SIDE) {
      if (!camSmoothInit) { camSmooth = pos; camSmoothInit = true; }
      camSmooth.x = damp(camSmooth.x, pos.x, 6, dt); camSmooth.y = damp(camSmooth.y, pos.y, 4, dt); camSmooth.z = damp(camSmooth.z, pos.z, 3, dt);
      pos = camSmooth;
    } else camSmoothInit = false;
    if (!pov) pos.y = std::max(pos.y, groundAt(pos) + 0.12f);
    q = lookQuat(pos, look);
  }
  if (blendT < 1) {
    blendT = std::min(1.f, blendT + dt / 1.3f); float e = smoothstepf(0, 1, blendT);
    pos = blendPos + (pos - blendPos) * e; ax::Quaternion::slerp(blendQuat, q, e, &q); tf = lerpf(blendFov, tf, e);
  }
  cutFlash = std::max(0.f, cutFlash - dt);
  camPos = pos; camQuat = q; fov = tf;
  ax::Mat4 R, T; ax::Mat4::createRotation(q, &R); ax::Mat4::createTranslation(pos.x, pos.y, pos.z, &T);
  view = T * R; view.inverse();
}

void Game::updateRiderPose(float dt) {
  // computeLook (main.js): periodic glances
  glanceT -= dt;
  if (glanceT <= 0) { glanceKind = (glanceKind + 1) % 4; glanceT = glanceKind == 0 ? 3.5f : 2.2f; }
  V3 lw;
  if (glanceKind == 1 && camMode != CAM_POV) lw = camPos;
  else if (glanceKind == 3) lw = V3(riderPos.x + 2, 1.2f, -8);
  else lw = V3(riderPos.x + 6, 1.15f, riderPos.z);
  { int nf = nextFish(28); if (nf >= 0) lw = fishes[nf].pos; }
  if (lookOverride) lw = lookOverrideP;
  ax::Mat4 inv = riderM; inv.inverse(); ax::Vec3 l(lw.x, lw.y, lw.z); inv.transformPoint(&l);
  RiderPoseIn in{S.crank, S.trick, S.steer, S.wheel, dt, S.t, S.airborne, V3(l.x, l.y, l.z), honkReq, gulpReq};
  honkReq = gulpReq = false;
  rider.showHelmet = settings.helmet; rider.showGlasses = settings.glasses == 0 ? night < 0.3f : settings.glasses == 1;
  rider.update(in);
}

void Game::update(float dt) {
  double t0 = nowMs();
  dt = std::min(dt, 1.f / 20);
  if (paused) dt = 0;
  S.t += dt; S.time += dt;
  updateRide(dt);
  // chunk recycling (world.js update)
  int base = int(std::floor(S.distance / CHUNK)) - 3;
  for (int k = 0; k < 7; k++) { int want = base + k; int slot = ((want % 7) + 7) % 7; if (chunkIndex[slot] != want) rebuildChunk(slot, want); }
  if (instDirty) uploadInstances();
  updateLighting(dt);
  updateCamera(dt);
  updateRiderPose(dt);
  updateExtras(dt);
  updateFx(dt);
  audio.p.speed = S.speed; audio.p.pedaling = S.pedaling; audio.p.night = night; audio.p.cadence = S.cadence; audio.p.airborne = S.airborne;
  audio.volume = settings.volume; audio.musicVolume = settings.musicVolume; audio.musicOn = settings.music; audio.muted = paused || !started;
  audio.pump(dt);
  cpuUpdateMs = float(nowMs() - t0);
}

// CPU twin of skyBase() (+ average cloud layer) integrated into a cosine-weighted ambient cube.
void Game::refreshAmbientCube() {
  // lobe sharpness/tint fitted to three's PMREM roughness-1 irradiance (tools/js_hdr_split.mjs + amb_fit.py)
  static float ambP = std::getenv("PB2_AMBP") ? std::atof(std::getenv("PB2_AMBP")) : 1.f;
  static float ambTint[6] = {0.73f, 0.81f, 0.95f, 0.91f, 0.90f, 0.88f};
  static bool tintInit = false; if (!tintInit) { tintInit = true; if (const char* t = std::getenv("PB2_AMBT")) sscanf(t, "%f,%f,%f,%f,%f,%f", &ambTint[0], &ambTint[1], &ambTint[2], &ambTint[3], &ambTint[4], &ambTint[5]); }
  V3 sd(F.sunDir[0], F.sunDir[1], F.sunDir[2]), md(F.moonDir[0], F.moonDir[1], F.moonDir[2]);
  auto sky = [&](V3 d) {
    float y = d.y, h = std::pow(1 - clampf(y, 0, 1), 3.5f);
    Col c = lerpc(pal.zen, pal.hor, h);
    if (y < 0) c = lerpc(pal.hor, pal.gnd, smoothstepf(0, -0.3f, y));
    float s = std::max(d.dot(sd), 0.f);
    float glow = (std::pow(s, 6.f) * 0.28f + std::pow(s, 48.f) * 0.55f + std::pow(s, 400.f) * 1.2f) * F.moonDir[3] * (0.45f + 0.55f * h);
    c.r += pal.sun.r * glow; c.g += pal.sun.g * glow; c.b += pal.sun.b * glow;
    if (y > 0) {  // mean cloud coverage
      float cov = smoothstepf(0, 0.14f, y) * 0.92f * clampf(settings.cloud * 0.55f, 0, 1);
      Col cc = lerpc(pal.cs, pal.ct, 0.55f);
      float ss = std::pow(s, 3.f) * 0.5f * (1 - night);
      cc.r += pal.sun.r * ss; cc.g += pal.sun.g * ss; cc.b += pal.sun.b * ss;
      c = lerpc(c, cc, cov);
    }
    return c;
  };
  static std::vector<V3> dirs;
  if (dirs.empty()) {  // fibonacci sphere
    const int N = 512;
    for (int i = 0; i < N; i++) { float y = 1 - (i + 0.5f) * 2.f / N, r = std::sqrt(1 - y * y), ph = i * 2.39996323f; dirs.push_back(V3(std::cos(ph) * r, y, std::sin(ph) * r)); }
  }
  static const V3 ax6[6] = {V3(1, 0, 0), V3(-1, 0, 0), V3(0, 1, 0), V3(0, -1, 0), V3(0, 0, 1), V3(0, 0, -1)};
  Col acc[6] = {}; float wsum[6] = {};
  for (auto& d : dirs) {
    Col c = sky(d);
    for (int k = 0; k < 6; k++) { float w = std::pow(std::max(0.f, d.dot(ax6[k])), ambP); acc[k].r += c.r * w; acc[k].g += c.g * w; acc[k].b += c.b * w; wsum[k] += w; }
  }
  for (int k = 0; k < 6; k++) { F.amb[k][0] = acc[k].r / wsum[k]; F.amb[k][1] = acc[k].g / wsum[k]; F.amb[k][2] = acc[k].b / wsum[k]; F.amb[k][3] = 0; }
  for (int k = 0; k < 6; k++) F.amb[k][3] = ambTint[k];
}

// ---------------- frame graph ----------------
static bool aabbVisible(const ax::Mat4& vp, const float* mn, const float* mx, float ox) {
  int out[6] = {0};
  for (int c = 0; c < 8; c++) {
    ax::Vec4 p((c & 1 ? mx[0] : mn[0]) + ox, c & 2 ? mx[1] : mn[1], c & 4 ? mx[2] : mn[2], 1); vp.transformVector(&p);
    out[0] += p.x < -p.w; out[1] += p.x > p.w; out[2] += p.y < -p.w; out[3] += p.y > p.w; out[4] += p.z < -p.w; out[5] += p.z > p.w;
  }
  for (int i = 0; i < 6; i++) if (out[i] == 8) return false;
  return true;
}

void Game::drawScene(bool shadow) {
  const ax::Mat4& vp = shadow ? shadowVP : viewProj;
  float shift = F.cloudOff[2];
  auto setModel = [](const Shader& s, const ax::Mat4& m) { glUniformMatrix4fv(s.loc("uModel"), 1, GL_FALSE, m.m); };
  // rider
  {
    const Shader& s = shadow ? shRider : litRider; s.use();
    setModel(s, riderM); rider.draw(s);
    const Shader& s0 = shadow ? shStatic : litStatic; s0.use();
    // static world strips (guard rail, curb, lamps)
    ax::Mat4 M; ax::Mat4::createTranslation(shift, 0, 0, &M);
    setModel(s0, M); strip.draw();
  }
  // instanced vegetation / props: one draw per prototype for all 7 chunks
  {
    const Shader& s = shadow ? shInst : litInst; s.use();
    static const float SW[P_N][2] = {{0.018f, 1.4f}, {0.018f, 1.4f}, {0.03f, 3.0f}, {0, 99}, {0, 99}};
    for (int k = 0; k < P_N; k++) {
      if (!inst[k].count) continue;
      if (shadow && (k == P_PINE)) continue;
      glUniform2f(s.loc("uSway"), SW[k][0], SW[k][1]);
      prop[k].draw(inst[k].count);
    }
  }
  drawExtras(shadow);
  drawFx(shadow);
  if (shadow) return;
  // terrain chunks (culled)
  {
    litTerrain.use();
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texDetail.id);
    for (int s = 0; s < 7; s++) {
      float ox = float(double(chunkIndex[s]) * CHUNK - S.distance);
      if (!aabbVisible(vp, terrain[s].bmin, terrain[s].bmax, ox)) continue;
      ax::Mat4 M; ax::Mat4::createTranslation(ox, 0, 0, &M); setModel(litTerrain, M); terrain[s].draw();
    }
    litRoad.use();
    glBindTexture(GL_TEXTURE_2D, texRoad.id);
    ax::Mat4 M; ax::Mat4::createTranslation(shift, 0, 0, &M); setModel(litRoad, M); road.draw();
    litStatic.use();
    glUniform4f(litStatic.loc("uTint"), 1, 1, 1, -std::max(1e-4f, F.phaseB[2] / 8.f));
    setModel(litStatic, M); bulbs.draw();
    glUniform4f(litStatic.loc("uTint"), 1, 1, 1, 1);
  }
  oceanS.use(); ocean.draw();
  // sky last: depth LEQUAL at far plane, no depth write (early-z rejects covered pixels)
  glDepthMask(GL_FALSE); glDepthFunc(GL_LEQUAL);
  skyS.use(); sky.draw();
  glDepthMask(GL_TRUE); glDepthFunc(GL_LESS);
  drawAdditive();
  glEnable(GL_BLEND); glDepthMask(GL_FALSE); drawFxTransparent(); glDisable(GL_BLEND); glDepthMask(GL_TRUE);
}

void Game::render(int winW, int winH) {
  double t0 = nowMs();
  g_stats.reset();
  GLint prevFbo = 0, prevVp[4], prevVao = 0, prevProg = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo); glGetIntegerv(GL_VIEWPORT, prevVp); glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVao); glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
  if (targetsDirty) { shadowRT.create(settings.shadowSize, settings.shadowSize, 0, true); createTargets(winW, winH); targetsDirty = false; }
  if (int(winW * settings.renderScale) != rtW || int(winH * settings.renderScale) != rtH) createTargets(winW, winH);
#if PB_DESKTOP_GL
  if (gpuQueryOk && frameNo > 2) { GLuint64 ns = 0; GLint avail = 0; glGetQueryObjectiv(gpuQuery[(frameNo + 1) & 1], GL_QUERY_RESULT_AVAILABLE, &avail); if (avail) { glGetQueryObjectui64v(gpuQuery[(frameNo + 1) & 1], GL_QUERY_RESULT, &ns); gpuMs = ns / 1e6f; } }
#endif
#if defined(GL_TIME_ELAPSED) && PB_DESKTOP_GL
  if (gpuQueryOk) glBeginQuery(GL_TIME_ELAPSED, gpuQuery[frameNo & 1]);
#endif
  ax::Mat4::createPerspective(fov, float(winW) / winH, 0.05f, 900.f, &proj);
  viewProj = proj * view;
  memcpy(F.viewProj, viewProj.m, 64);
  { ax::Vec3 fw(-view.m[2], -view.m[6], -view.m[10]); F.camFwd[0] = fw.x; F.camFwd[1] = fw.y; F.camFwd[2] = fw.z; F.camFwd[3] = rtH / (2 * std::tan(fov * PI / 360)); }
  F.camPos[0] = camPos.x; F.camPos[1] = camPos.y; F.camPos[2] = camPos.z; F.camPos[3] = S.t;
  glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(FrameUBO), &F);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);

  glDisable(GL_BLEND); glDisable(GL_SCISSOR_TEST); glDisable(GL_STENCIL_TEST); glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE); glColorMask(1, 1, 1, 1);

  // 1) shadow map
  shadowRT.bindDraw();
  glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(1.5f, 3.f);
  {  // shadow pass uses the light VP as uViewProj
    FrameUBO tmp = F; memcpy(tmp.viewProj, shadowVP.m, 64);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 64, tmp.viewProj);
    drawScene(true);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 64, F.viewProj);
  }
  glDisable(GL_POLYGON_OFFSET_FILL);
  // 2) HDR scene
  hdrRT.bindDraw();
  glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, shadowRT.depth.id); glActiveTexture(GL_TEXTURE0);
  drawScene(false);
  hdrRT.resolve();
  if (frameNo == (std::getenv("PB2_DUMPFRAME") ? std::atoi(std::getenv("PB2_DUMPFRAME")) : 30) && std::getenv("PB2_DUMPHDR")) {
    std::vector<float> px(size_t(rtW) * rtH * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrRT.fbo); glReadPixels(0, 0, rtW, rtH, GL_RGBA, GL_FLOAT, px.data());
    FILE* f = fopen(std::getenv("PB2_DUMPHDR"), "wb"); if (f) { fwrite(&rtW, 4, 1, f); fwrite(&rtH, 4, 1, f); fwrite(px.data(), 4, px.size(), f); fclose(f); }
  }
  static const bool sweepOn = std::getenv("PB2_SWEEP") != nullptr;
  if (sweepOn) {  // NaN/Inf audit while sweeping hour (PB2_SWEEP=step)
    settings.hour = std::fmod(settings.hour + float(std::atof(std::getenv("PB2_SWEEP"))), 24.f);
    std::vector<float> px(size_t(rtW) * rtH * 4); glBindFramebuffer(GL_FRAMEBUFFER, hdrRT.fbo); glReadPixels(0, 0, rtW, rtH, GL_RGBA, GL_FLOAT, px.data());
    int bad = 0, fb = -1; float mx = 0; for (size_t i = 0; i < px.size(); i++) { if (!std::isfinite(px[i])) { if (!bad) fb = int(i / 4); bad++; } else mx = std::max(mx, px[i]); }
    AXLOGI("[pb2] sweep h {:.3f} bad {} max {:.1f} at {} {} a {}", settings.hour, bad, mx, fb % rtW, fb / rtW, fb >= 0 ? px[fb * 4 + 3] : 0.f);
  }
  // 3) bloom (UnrealBloomPass: high-pass + 5 mip separable gaussian)
  glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE);
  static const int KR[5] = {6, 10, 14, 18, 22};  // UnrealBloomPass (r18x): sigma = r/3, paired bilinear taps
  bloomA[0].bindDraw(); brightS.use(); glUniform1f(brightS.loc("uThreshold"), bloomThreshold);
  glBindTexture(GL_TEXTURE_2D, hdrRT.color.id); drawFullscreen();
  blurS.use(); GLint lDir = blurS.loc("uDir"), lP = blurS.loc("uPairs"), lCW = blurS.loc("uCW"), lO = blurS.loc("uOff"), lW = blurS.loc("uW");
  GLuint input = bloomA[0].color.id;
  for (int i = 0; i < 5; i++) {
    float c[24]; float sg = KR[i] / 3.f; for (int k = 0; k < KR[i]; k++) c[k] = 0.39894f * std::exp(-0.5f * k * k / (sg * sg)) / sg;
    float off[11], w[11]; int np = 0;
    for (int k = 1; k < KR[i]; k += 2) { float wa = c[k], wb = k + 1 < KR[i] ? c[k + 1] : 0; off[np] = (k * wa + (k + 1) * wb) / (wa + wb); w[np] = wa + wb; np++; }
    glUniform1i(lP, np); glUniform1f(lCW, c[0]); glUniform1fv(lO, np, off); glUniform1fv(lW, np, w);
    bloomB[i].bindDraw(); glUniform2f(lDir, 1.f / bloomB[i].w, 0); glBindTexture(GL_TEXTURE_2D, input); drawFullscreen();
    bloomA[i].bindDraw(); glUniform2f(lDir, 0, 1.f / bloomA[i].h); glBindTexture(GL_TEXTURE_2D, bloomB[i].color.id); drawFullscreen();
    input = bloomA[i].color.id;
  }
  // 3b) backdrop blur source (only when some HUD glass is on screen): 1/4-res HDR, separable gaussian sigma ~3.3 texels (~13 px)
  int nBlur = std::min<int>(int(blurRects.size()), 24);
  if (nBlur > 0) {
    float c[10]; const int KRb = 10; float sg = KRb / 3.f; for (int k = 0; k < KRb; k++) c[k] = 0.39894f * std::exp(-0.5f * k * k / (sg * sg)) / sg;
    float off[5], w[5]; int np = 0; for (int k = 1; k < KRb; k += 2) { float wa = c[k], wb = k + 1 < KRb ? c[k + 1] : 0; off[np] = (k * wa + (k + 1) * wb) / (wa + wb); w[np] = wa + wb; np++; }
    glUniform1i(lP, np); glUniform1f(lCW, c[0]); glUniform1fv(lO, np, off); glUniform1fv(lW, np, w);
    blurB.bindDraw(); glUniform2f(lDir, 1.f / blurB.w, 0); glBindTexture(GL_TEXTURE_2D, hdrRT.color.id); drawFullscreen();
    blurA.bindDraw(); glUniform2f(lDir, 0, 1.f / blurA.h); glBindTexture(GL_TEXTURE_2D, blurB.color.id); drawFullscreen();
  }
  // 4) composite + grade + ACES + sRGB into Axmol's framebuffer
  glBindFramebuffer(GL_FRAMEBUFFER, prevFbo); glViewport(prevVp[0], prevVp[1], prevVp[2], prevVp[3]); g_stats.passes++;
  finalS.use();
  float bf[5]; static const float FAC[5] = {1.0f, 0.8f, 0.6f, 0.4f, 0.2f};
  static bool noBloom = std::getenv("PB2_NOBLOOM") != nullptr;
  static float bloomK = std::getenv("PB2_BLOOMK") ? std::atof(std::getenv("PB2_BLOOMK")) : 1.f;
  for (int i = 0; i < 5; i++) bf[i] = noBloom ? 0.f : 3.f * lerpf(FAC[i], 1.2f - FAC[i], 0.55f) * bloomStrength * bloomK;  // r186: "3.0 * bloomStrength"
  glUniform4f(finalS.loc("uBloomF"), bf[0], bf[1], bf[2], bf[3]);
  glUniform4f(finalS.loc("uGrade"), bf[4], 0.28f, warm, 0.035f);
  glUniform2f(finalS.loc("uExpTime"), exposureVal, S.t); if (frameNo == 60) AXLOGI("[pb2] exp {} warm {} bloomS {} thr {}", exposureVal, warm, bloomStrength, bloomThreshold);
  { float R[24 * 4], P[24 * 4]; float sx = float(prevVp[2]) / std::max(1, winW), sy = float(prevVp[3]) / std::max(1, winH);
    for (int i = 0; i < nBlur; i++) { auto& b = blurRects[i]; R[i * 4] = b.x0 * sx + prevVp[0]; R[i * 4 + 1] = b.y0 * sy + prevVp[1]; R[i * 4 + 2] = b.x1 * sx + prevVp[0]; R[i * 4 + 3] = b.y1 * sy + prevVp[1];
      P[i * 4] = b.rad * sx; P[i * 4 + 1] = b.amount; P[i * 4 + 2] = sx; P[i * 4 + 3] = 0; }
    glUniform1i(finalS.loc("uNR"), nBlur); if (nBlur) { glUniform4fv(finalS.loc("uRects"), nBlur, R); glUniform4fv(finalS.loc("uRectP"), nBlur, P); }
    glUniform1i(finalS.loc("uBlur"), 7); glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, blurA.color.id); }
  glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, hdrRT.color.id);
  for (int i = 0; i < 5; i++) { glActiveTexture(GL_TEXTURE2 + i); glBindTexture(GL_TEXTURE_2D, bloomA[i].color.id); }
  glActiveTexture(GL_TEXTURE0);
  drawFullscreen();
#if defined(GL_TIME_ELAPSED) && PB_DESKTOP_GL
  if (gpuQueryOk) glEndQuery(GL_TIME_ELAPSED);
#endif
  // restore for Axmol
  glDepthMask(GL_TRUE); glEnable(GL_DEPTH_TEST);
  glBindVertexArray(prevVao); glUseProgram(prevProg);
  for (int i = 7; i >= 0; i--) { glActiveTexture(GL_TEXTURE0 + i); glBindTexture(GL_TEXTURE_2D, 0); }
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  lastStats = g_stats; frameNo++;
  cpuRenderMs = float(nowMs() - t0);
}
}  // namespace pb

void pb::Game::fxTrigger(const std::string& k) {
  if (k == "jump") jump(); else if (k == "trick") doTrick(); else if (k == "confetti") confettiBurst(V3(0.3f, 1.9f, S.lane));
  else if (k == "splash") splashAt(riderPos.x + 4, -16, 26); else if (k == "jumper") launchJumper(riderPos.x + 6, -16);
}

std::string pb::Game::poseDebug() const {
  char b[256]; snprintf(b, sizeof b, "yaw %.4f pitch %.4f jaw %.3f cam %.4f %.4f %.4f glance %d", rider.yaw, rider.pitch, rider.jaw, camPos.x, camPos.y, camPos.z, glanceKind); return b;
}
