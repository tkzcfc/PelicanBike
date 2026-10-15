#include "game/Game.h"
#include <cmath>
using namespace gfx;
namespace pb {

void PSys::emit(V3 pos, V3 vel, Col c, float size, float life, float drag, float grav, bool ground) {
  Particle& q = p[cursor]; cursor = (cursor + 1) % int(p.size());
  q.p = pos; q.v = vel; q.r = c.r; q.g = c.g; q.b = c.b; q.size = size; q.life = q.maxLife = life; q.drag = drag; q.grav = grav; q.ground = ground;
}
void PSys::update(float dt, float scroll) {
  for (auto& q : p) {
    if (q.life <= 0) continue;
    q.life -= dt; float d = std::exp(-q.drag * dt);
    q.v.x *= d; q.v.y = q.v.y * d - q.grav * dt; q.v.z *= d;
    q.p.x += q.v.x * dt - (q.ground ? scroll : 0); q.p.y += q.v.y * dt; q.p.z += q.v.z * dt;
  }
}

// particle / speed-line shaders: Source/shaders/part.*, line.*

void Game::buildFx() {
  dust.init(420, 0.35f); sparkles.init(360, 1); splash.init(360, 1);
  std::vector<PbmPart> parts; loadPbm("data/world2.pbm", parts);
  for (auto& p : parts) { if (p.name == "fish") fishMesh.createPacked(p.v.data(), p.v.size(), p.i.data(), p.i.size()); if (p.name == "fishgold") goldMesh.createPacked(p.v.data(), p.v.size(), p.i.data(), p.i.size()); }
  fishI.create(17); goldI.create(12); fishIS.create(17); goldIS.create(12);
  // particle stream: pos3 | rgba | size (32 B)
  { std::vector<float> z(1300 * 8, 0.f); partMesh.createRaw(z.data(), z.size() * 4, 1300, true); partMesh.prim = bk::PrimitiveType::POINT; }
  makeProg(partS, "part", "part", gfx::VL::Particle);
  makeProg(lineS, "line", "line");
  // confetti (220 quads, CPU-transformed), scarf (16x2 strip), speed lines (70 segments)
  { Mulberry r(3); static const uint32_t PAL[7] = {0xff595e, 0xffca3a, 0x8ac926, 0x1982c4, 0x6a4c93, 0xffffff, 0xff8fab};
    confetti.resize(220); confV.resize(220 * 4); std::vector<uint32_t> idx;
    for (int i = 0; i < 220; i++) { auto& c = confetti[i]; c.rot = V3(r() * 6, r() * 6, r() * 6); c.spin = V3(r() * 12 - 6, r() * 12 - 6, r() * 12 - 6);
      for (int k = 0; k < 4; k++) { PVertex& v = confV[i * 4 + k]; v = PVertex{}; uint32_t h = PAL[i % 7]; v.c[0] = h >> 16; v.c[1] = (h >> 8) & 255; v.c[2] = h & 255; v.c[3] = 255;
        v.m[0] = 128; v.m[1] = 51; v.m[2] = 0; v.m[3] = 255; v.flags = 1; }
      uint32_t b = i * 4; idx.insert(idx.end(), {b, b + 1, b + 2, b + 2, b + 1, b + 3}); }
    confMesh.createPacked(confV.data(), confV.size(), idx.data(), idx.size(), true); }
  { scarfP.assign(32, V3()); scarfQ.assign(32, V3()); scarfV.resize(32); std::vector<uint32_t> idx;
    for (int i = 0; i < 16; i++) { bool stripe = i > 11 && i % 2 == 0; for (int k = 0; k < 2; k++) { PVertex& v = scarfV[i * 2 + k]; v = PVertex{};
        if (stripe) { v.c[0] = 255; v.c[1] = 244; v.c[2] = 230; } else { v.c[0] = 0xd7; v.c[1] = 0x26; v.c[2] = 0x3d; } v.c[3] = 255; v.m[0] = 191; v.m[1] = 0; v.m[2] = 0; v.m[3] = 255; v.flags = 1; } }
    for (uint32_t i = 0; i < 15; i++) { uint32_t a = i * 2; idx.insert(idx.end(), {a, a + 1, a + 2, a + 1, a + 3, a + 2}); }
    scarfMesh.createPacked(scarfV.data(), 32, idx.data(), idx.size(), true); }
  { Mulberry r(77); lineSeeds.resize(70); for (auto& s : lineSeeds) { float a = r() * TAU, rr = 0.9f + r() * 2.4f; s = V3(std::cos(a) * rr, std::sin(a) * rr * 0.65f, -r() * 14 - 1); }
    std::vector<PVertex> v(140, PVertex{}); lineMesh.createPacked(v.data(), 140, nullptr, 0, true); lineMesh.prim = bk::PrimitiveType::LINE; }
}

int Game::nextFish(float range) const {
  int best = -1;
  for (int i = 0; i < 12; i++) { const Fish& f = fishes[i]; if (!f.active || f.caught >= 0) continue; double rel = f.x - S.distance;
    if (rel > 0.2 && rel < range && (best < 0 || rel < fishes[best].x - S.distance)) best = i; }
  return best;
}
void Game::unlock(const char* id) {
  for (auto& a : achieved) if (a == id) return;
  achieved.push_back(id); fxEv.unlocks.push_back(id);
  static const char* BIG[7] = {"golden", "fish10", "speed45", "km1", "trick3", "fish50", "km5"};
  for (auto* b : BIG) if (!strcmp(b, id)) confettiBurst(V3(0.3f, 1.9f, S.lane));
}
void Game::confettiBurst(V3 c) {
  for (auto& p : confetti) { p.pos = V3(c.x + (rnd() - 0.5f) * 0.6f, c.y + rnd() * 0.4f, c.z + (rnd() - 0.5f) * 0.6f); float a = rnd() * TAU, sp = 1.5f + rnd() * 3.5f;
    p.vel = V3(std::cos(a) * sp * 0.6f, 3 + rnd() * 4.5f, std::sin(a) * sp * 0.6f); p.life = 3 + rnd() * 2; }
  confActive = true;
}
void Game::spawnFish(float ahead) {
  for (auto& f : fishes) if (!f.active) {
    static const float LANES[4] = {-1.8f, -0.6f, 0.6f, 1.8f};
    f.active = true; f.caught = -1; f.x = S.distance + ahead; f.z = LANES[int(rnd() * 4) % 4]; f.y = 1.3f + rnd() * 0.25f; f.golden = rnd() < 0.12f; f.phase = rnd() * TAU; f.scale = 1.35f; return;
  }
}
static float waveH(double x, double z, double t, float amp) {
  static const float W[5][4] = {{0.18f, 1.0f, 0.2f, 21.0f}, {-0.45f, 1.0f, 0.17f, 12.5f}, {0.75f, 0.62f, 0.13f, 7.0f}, {-0.9f, 0.45f, 0.1f, 4.2f}, {0.3f, 1.0f, 0.07f, 2.6f}};
  double off = std::min(1.0, std::max(0.0, (SHORE_Z + 1 - z) / 31)); double a0 = amp * (0.35 + 0.65 * off * off * (3 - 2 * off)); double y = 0;
  for (auto& w : W) { double k = 2 * M_PI / w[3], c = std::sqrt(9.8 / k), l = std::hypot(w[0], w[1]); y += w[2] * a0 / k * std::sin(k * (w[0] / l * x + w[1] / l * z - c * t)); }
  return float(SEA_LEVEL + y);
}
void Game::splashAt(float x, float z, int n) {
  float y = waveH(x + S.distance, z, S.t, settings.waveAmp);
  for (int i = 0; i < n; i++) { float a = rnd() * TAU, sp = 0.5f + rnd() * 1.5f;
    splash.emit(V3(x, y + 0.05f, z), V3(std::cos(a) * sp, 2 + rnd() * 2.5f, std::sin(a) * sp), Col{1, 1, 1}, 0.06f + rnd() * 0.06f, 0.8f + rnd() * 0.5f, 0.6f, 9.8f, true); }
}
void Game::launchJumper(float x, float z) {
  for (auto& j : jumpers) if (j.t < 0) { j.t = 0; j.x = x + S.distance; j.z = z; j.dur = 1.0f + rnd() * 0.5f; j.h = 1.2f + rnd() * 1.3f; j.dir = rnd() < 0.5f ? 1 : -1; splashAt(x, z, 26); audio.splash(); return; }
}
void Game::burstDust(int n) {
  Col c = hexLin(0xcdbb9a);
  for (int i = 0; i < n; i++) dust.emit(V3(-0.46f + (rnd() - 0.5f) * 0.3f, 0.03f, S.lane + (rnd() - 0.5f) * 0.25f), V3((rnd() - 0.5f) * 1.5f, rnd() * 0.9f, (rnd() - 0.5f) * 1.5f), c, 0.12f + rnd() * 0.12f, 0.8f + rnd() * 0.6f, 1.5f, -0.1f, true);
}

void Game::updateFx(float dt) {
  // fish pickups
  if (S.distance > nextFishAt) { spawnFish(nextFishAt == 30 ? 32 : 85); nextFishAt = S.distance + 16 + rnd() * 18; }
  ax::Vec3 mw = ax::Vec3(rider.mouthPos().x, rider.mouthPos().y, rider.mouthPos().z); riderM.transformPoint(&mw); V3 mouthW(mw.x, mw.y, mw.z);
  for (auto& f : fishes) {
    if (!f.active) continue;
    float rel = float(f.x - S.distance);
    if (f.caught >= 0) { f.caught += dt; float u = std::min(1.f, f.caught / 0.16f); f.pos = f.pos + (mouthW - f.pos) * u; f.scale = 1.35f * (1 - u * 0.9f); if (u >= 1) f.active = false; continue; }
    f.pos = V3(rel, f.y + std::sin(S.t * 3 + f.phase) * 0.08f, f.z); f.rx = std::sin(S.t * 9 + f.phase) * 0.15f; f.ry = PI + std::sin(S.t * 1.5f + f.phase) * 0.6f;
    f.haloA = 0.45f + std::sin(S.t * 5 + f.phase) * 0.15f;
    if (rel < -12) { f.active = false; continue; }
    V3 d = f.pos - mouthW;
    if (std::fabs(d.x) < 0.45f + S.speed * dt && std::fabs(d.z) < 0.52f && std::fabs(d.y) < 0.6f) {
      f.caught = 0; audio.gulp(f.golden); S.fish += f.golden ? 5 : 1; if (f.golden) S.golden++; gulpReq = true;
      Col gold = hexLin(0xffd257), cyan = hexLin(0x9fefff), white{1, 1, 1};
      for (int i = 0; i < (f.golden ? 70 : 36); i++) { V3 v = V3(rnd() - 0.5f, rnd() * 0.9f - 0.2f, rnd() - 0.5f).norm() * (0.8f + rnd() * 1.6f);
        sparkles.emit(mouthW, v, rnd() < 0.5f ? (f.golden ? gold : cyan) : white, 0.035f + rnd() * 0.03f, 0.6f + rnd() * 0.6f, 3); }
      fxEv.pops.push_back({f.golden ? "+5 金鱼!" : "+1", f.golden}); fxEv.popWorld = mouthW;
      unlock("fish1"); if (f.golden) unlock("golden"); if (S.fish >= 10) unlock("fish10"); if (S.fish >= 50) unlock("fish50");
    }
  }
  // jumpers
  for (auto& j : jumpers) {
    if (j.t < 0) continue;
    j.t += dt; float u = j.t / j.dur; float x = float(j.x - S.distance) + (u - 0.5f) * 1.6f * j.dir;
    float base = waveH(j.x, j.z, S.t, settings.waveAmp); j.pos = V3(x, base + std::sin(u * PI) * j.h - 0.2f, j.z); j.ry = j.dir > 0 ? 0 : PI; j.rz = (0.5f - u) * 2.4f;
    if (u >= 1) { j.t = -1; splashAt(x, j.z, 18); audio.splash(); }
  }
  if (started && rnd() < dt * 0.12f) launchJumper(8 + rnd() * 30, -20 - rnd() * 25);
  // dust
  if (!S.airborne && S.speed >= 3.5f) { dustAcc += dt * S.speed * 1.4f; Col dc = hexLin(0xb9ab92);
    while (dustAcc > 1) { dustAcc -= 1; dust.emit(V3(-0.46f - 0.05f - rnd() * 0.1f, 0.02f, S.lane + (rnd() - 0.5f) * 0.08f), V3(-0.3f - rnd() * 0.6f, 0.15f + rnd() * 0.35f, (rnd() - 0.5f) * 0.4f), dc, 0.03f + rnd() * 0.04f, 0.5f + rnd() * 0.4f, 1.2f, -0.05f, true); } }
  float scroll = S.speed * dt;
  dust.update(dt, scroll); sparkles.update(dt, 0); splash.update(dt, scroll);
  // confetti
  if (confActive) { int alive = 0;
    for (size_t i = 0; i < confetti.size(); i++) { auto& p = confetti[i];
      if (p.life > 0) { p.life -= dt; alive++; p.vel.y -= 5.5f * dt; p.vel = p.vel * std::exp(-1.6f * dt); p.vel.x += std::sin(p.life * 5 + i) * 0.6f * dt; p.pos = p.pos + p.vel * dt; p.pos.x -= scroll * 0.35f;
        if (p.pos.y < 0.01f) { p.pos.y = 0.01f; p.vel = V3(); } else p.rot = p.rot + p.spin * dt; } }
    if (!alive) confActive = false; confDirty = true; }
  // scarf (Verlet), root at knot back side, in world space
  if (settings.scarf && dt > 0) {
    V3 kp = rider.knotPos(); ax::Vec3 a(kp.x - 0.07f, kp.y + 0.025f, 0.01f), b(kp.x - 0.07f, kp.y - 0.04f, -0.01f); riderM.transformPoint(&a); riderM.transformPoint(&b);
    V3 A(a.x, a.y, a.z), B(b.x, b.y, b.z); V3 bodyW; { ax::Vec3 bw(rider.bodyPos().x, rider.bodyPos().y, rider.bodyPos().z); riderM.transformPoint(&bw); bodyW = V3(bw.x, bw.y, bw.z); }
    V3 wind(-S.speed + std::sin(S.t * 0.7f) * 0.8f, S.airborne ? -S.vy * 0.6f : 0.3f, -S.laneV * 0.8f + std::sin(S.t * 0.4f) * 0.6f);
    updateScarf(dt, A, B, wind, bodyW);
  }
  // speed lines (camera space -> world)
  { float k = clampf((S.speed - 9) / 5, 0, 1) * ((camMode == CAM_POV || camMode == CAM_CHASE) ? 1 : 0.6f); lineOpacity = k * 0.35f;
    if (lineOpacity > 0.0035f) { float len = 0.4f + S.speed * 0.08f; ax::Mat4 cw = view; cw.inverse();
      for (int i = 0; i < 70; i++) { V3& s = lineSeeds[i]; s.z += S.speed * 2.2f * dt; if (s.z > 0.5f) s.z = -15;
        ax::Vec3 p0(s.x, s.y, s.z), p1(s.x, s.y, s.z - len); cw.transformPoint(&p0); cw.transformPoint(&p1);
        lineV[i * 2] = V3(p0.x, p0.y, p0.z); lineV[i * 2 + 1] = V3(p1.x, p1.y, p1.z); }
      lineDirty = true; } }
  // achievements by state
  float kmh = S.speed * 3.6f; if (kmh >= 30) unlock("speed30"); if (kmh >= 45) unlock("speed45"); if (S.distance >= 1000) unlock("km1"); if (S.distance >= 5000) unlock("km5");
  if (night > 0.8f && S.time > 5) unlock("night");
}

void Game::updateScarf(float dt, V3 a, V3 b, V3 wind, V3 col) {
  const int n = 16; const float segLen = 0.045f, width = 0.075f;
  auto bad = [&]() { V3 q = scarfP[31]; return !std::isfinite(q.x + q.y + q.z); };
  if (scarfInit && bad()) scarfInit = false;
  if (!scarfInit) { for (int i = 0; i < n; i++) for (int k = 0; k < 2; k++) { V3 s = k ? b : a; scarfP[i * 2 + k] = V3(s.x - i * segLen, s.y, s.z); } scarfQ = scarfP; scarfInit = true; }
  auto constrain = [&](int i, int j, float rest, bool pin) { V3 d = scarfP[j] - scarfP[i]; float l = d.len(); if (l < 1e-6f) l = 1e-6f; float diff = (l - rest) / l;
    if (pin) scarfP[j] = scarfP[j] - d * diff; else { scarfP[i] = scarfP[i] + d * (diff * 0.5f); scarfP[j] = scarfP[j] - d * (diff * 0.5f); } };
  const int steps = 2; float h = std::min(dt, 1.f / 30) / steps; float t = S.t;
  for (int s = 0; s < steps; s++) {
    for (int i = 1; i < n; i++) for (int k = 0; k < 2; k++) { int j = i * 2 + k; V3 v = scarfP[j] - scarfQ[j]; scarfQ[j] = scarfP[j];
      float f = float(i) / n; float flutter = std::sin(t * 17 + i * 0.9f + k * 0.4f) * (0.6f + f * 2.2f);
      float ax_ = (wind.x - v.x / h) * 2.2f, ay = -9.8f * 0.55f + (wind.y - v.y / h) * 1.2f + flutter * 1.4f * std::min(1.f, std::fabs(wind.x) / 6), az = (wind.z - v.z / h) * 2.2f + std::cos(t * 13 + i * 0.7f) * flutter * 0.8f;
      scarfP[j] = scarfP[j] + v * 0.985f + V3(ax_, ay, az) * (h * h); }
    scarfP[0] = a; scarfP[1] = b;
    float dg = std::hypot(segLen, width);
    for (int it = 0; it < 4; it++) {
      for (int i = 0; i < n - 1; i++) { for (int k = 0; k < 2; k++) constrain(i * 2 + k, (i + 1) * 2 + k, segLen, i == 0);
        constrain((i + 1) * 2, (i + 1) * 2 + 1, width, false); constrain(i * 2, (i + 1) * 2 + 1, dg, i == 0); constrain(i * 2 + 1, (i + 1) * 2, dg, i == 0); }
      for (int i = 1; i < n; i++) for (int k = 0; k < 2; k++) { V3& p = scarfP[i * 2 + k]; V3 c = p - col; float d = c.len(); if (d < 0.25f) p = col + c * (0.25f / (d > 0 ? d : 1)); }
    }
  }
  // vertex normals (computeVertexNormals on the strip)
  static thread_local std::vector<V3> nn; nn.assign(32, V3());
  for (int i = 0; i < n - 1; i++) { int a0 = i * 2; int tri[2][3] = {{a0, a0 + 1, a0 + 2}, {a0 + 1, a0 + 3, a0 + 2}};
    for (auto& tr : tri) { V3 fn = (scarfP[tr[1]] - scarfP[tr[0]]).cross(scarfP[tr[2]] - scarfP[tr[0]]); for (int q : tr) nn[q] = nn[q] + fn; } }
  for (int i = 0; i < 32; i++) { PVertex& v = scarfV[i]; v.p[0] = scarfP[i].x; v.p[1] = scarfP[i].y; v.p[2] = scarfP[i].z; V3 m = nn[i].len() > 1e-8f ? nn[i].norm() : V3(0, 1, 0);
    v.n[0] = int8_t(std::lround(m.x * 127)); v.n[1] = int8_t(std::lround(m.y * 127)); v.n[2] = int8_t(std::lround(m.z * 127)); }
  scarfDirty = true;
}

static void put34m(float* d, V3 p, float rx, float ry, float rz, float s) {
  ax::Mat4 T, X, Y, Z; ax::Mat4::createTranslation(p.x, p.y, p.z, &T); ax::Mat4::createRotationX(rx, &X); ax::Mat4::createRotationY(ry, &Y); ax::Mat4::createRotationZ(rz, &Z);
  ax::Mat4 m = T * X * Y * Z; for (int r = 0; r < 3; r++) { d[r * 4] = m.m[r] * s; d[r * 4 + 1] = m.m[4 + r] * s; d[r * 4 + 2] = m.m[8 + r] * s; d[r * 4 + 3] = m.m[12 + r]; }
}

void Game::drawFx(bool shadow) {
  // fish instances (pickups + jumpers): 2 instanced draws
  float nf[17 * 12], gf[12 * 12]; int cn = 0, cg = 0;
  for (auto& f : fishes) if (f.active) put34m(f.golden ? &gf[cg++ * 12] : &nf[cn++ * 12], f.pos, f.rx, f.ry, 0, f.scale);
  if (!shadow) for (auto& j : jumpers) if (j.t >= 0) put34m(&nf[cn++ * 12], j.pos, 0, j.ry, j.rz, 1.8f);
  const Shader& bs = shadow ? shBird : litBird;
  InstanceBuf& fi = shadow ? fishIS : fishI; InstanceBuf& gi = shadow ? goldIS : goldI;
  if (cn) { fi.upload(nf, cn); gfx::draw(bs, fishMesh, &fi); }
  if (cg) { gi.upload(gf, cg); gfx::draw(bs, goldMesh, &gi); }
  const Shader& s = shadow ? shStatic : litStatic; ax::Mat4 I; s.setMat("uModel", I.m);
  if (settings.scarf && scarfInit) { if (scarfDirty) { scarfMesh.updatePacked(scarfV.data(), 32); scarfDirty = false; } gfx::draw(s, scarfMesh); }
  if (confActive) {
    if (confDirty) {
      for (size_t i = 0; i < confetti.size(); i++) { auto& p = confetti[i]; float sc = p.life > 0 ? std::min(1.f, p.life) : 0;
        ax::Mat4 R, X, Y, Z; ax::Mat4::createRotationX(p.rot.x, &X); ax::Mat4::createRotationY(p.rot.y, &Y); ax::Mat4::createRotationZ(p.rot.z, &Z); R = X * Y * Z;
        for (int k = 0; k < 4; k++) { ax::Vec3 q((k & 1 ? 0.025f : -0.025f) * sc, (k & 2 ? -0.014f : 0.014f) * sc, 0), nrm(0, 0, 1); R.transformVector(&q); R.transformVector(&nrm);
          PVertex& v = confV[i * 4 + k]; v.p[0] = p.pos.x + q.x; v.p[1] = p.pos.y + q.y; v.p[2] = p.pos.z + q.z; v.n[0] = int8_t(nrm.x * 127); v.n[1] = int8_t(nrm.y * 127); v.n[2] = int8_t(nrm.z * 127); } }
      confMesh.updatePacked(confV.data(), confV.size()); confDirty = false; }
    gfx::draw(s, confMesh);
  }
}

void Game::drawFxTransparent() {
  // particles: normal-blended (dust, splash) then additive (sparkles, fish halos)
  std::vector<float>& buf = partBuf; buf.clear();
  auto push = [&](const PSys& ps) { for (auto& q : ps.p) { if (q.life <= 0) continue; float f = q.life / q.maxLife; float al = clampf(f * 1.6f, 0, 1) * clampf((1 - f) * 8, 0, 1) * ps.opacity;
      buf.insert(buf.end(), {q.p.x, q.p.y, q.p.z, q.r, q.g, q.b, al, q.size}); } };
  push(dust); push(splash); int nNormal = int(buf.size() / 8);
  push(sparkles); int nAdd = int(buf.size() / 8) - nNormal;
  Col cy = hexLin(0x7fe3ff), gd = hexLin(0xffd257); int nHalo = 0;
  for (auto& f : fishes) if (f.active && f.caught < 0) { Col c = f.golden ? gd : cy; buf.insert(buf.end(), {f.pos.x, f.pos.y, f.pos.z, c.r, c.g, c.b, f.haloA, 0.9f}); nHalo++; }
  gfx::State st = gfx::state(); st.depthWrite = false;
  if (!buf.empty()) {
    size_t nf = std::min<size_t>(buf.size(), 1300 * 8);
    partMesh.updateRaw(buf.data(), nf * 4);
    if (nNormal) { st.blend = gfx::Blend::Alpha; gfx::setState(st); partS.set4("uHalo", 0); gfx::drawArrays(partS, partMesh, bk::PrimitiveType::POINT, 0, nNormal); }
    st.blend = gfx::Blend::Add; gfx::setState(st);
    if (nAdd) { partS.set4("uHalo", 0); gfx::drawArrays(partS, partMesh, bk::PrimitiveType::POINT, nNormal, nAdd); }
    if (nHalo) { partS.set4("uHalo", 1); gfx::drawArrays(partS, partMesh, bk::PrimitiveType::POINT, nNormal + nAdd, nHalo); }
  }
  if (lineOpacity > 0.0035f) {
    if (lineDirty) { static std::vector<PVertex> v(140, PVertex{}); for (int i = 0; i < 140; i++) { v[i].p[0] = lineV[i].x; v[i].p[1] = lineV[i].y; v[i].p[2] = lineV[i].z; } lineMesh.updatePacked(v.data(), 140); lineDirty = false; }
    st.blend = gfx::Blend::Add; gfx::setState(st); lineS.set4("uOpacity", lineOpacity); gfx::draw(lineS, lineMesh);
  }
  st.blend = gfx::Blend::Off; gfx::setState(st);
}
}  // namespace pb
