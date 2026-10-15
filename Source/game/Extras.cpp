// World extras ported from world.js: lighthouse island (+ night beam), sail boats riding the
// waves, gull circles + pelican V-formation (instanced 3x4 matrices), road signs, fireflies,
// lamp light pools. All cheap: ~16 draws total.
#include "game/Game.h"
#include "game/Shaders.h"
#include "platform/FileUtils.h"
#include <cmath>
using namespace gfx;
namespace pb {

static const float WAVES5[5][5] = {{0.18f, 1.0f, 0.2f, 21.0f, 1}, {-0.45f, 1.0f, 0.17f, 12.5f, 1}, {0.75f, 0.62f, 0.13f, 7.0f, 1}, {-0.9f, 0.45f, 0.1f, 4.2f, 1}, {0.3f, 1.0f, 0.07f, 2.6f, 1}};
static float waveHeight(double x, double z, double t, float amp) {
  double off = std::min(1.0, std::max(0.0, (SHORE_Z + 1 - z) / 31)); double a0 = amp * (0.35 + 0.65 * off * off * (3 - 2 * off)); double y = 0;
  for (auto& w : WAVES5) { double k = 2 * M_PI / w[3], c = std::sqrt(9.8 / k) * w[4], l = std::hypot(w[0], w[1]); double f = k * (w[0] / l * x + w[1] / l * z - c * t); y += w[2] * a0 / k * std::sin(f); }
  return float(SEA_LEVEL + y);
}

GLuint makeProgX(Shader& s, const char* name, int mode, const char* vs, const char* fs);

void Game::buildExtras() {
  std::vector<PbmPart> parts; loadPbm("data/world2.pbm", parts);
  for (auto& p : parts) {
    Mesh* m = nullptr; const std::string& n = p.name;
    if (n == "lh_static") m = &lhStatic; else if (n == "lh_lantern") m = &lhLantern; else if (n == "lh_beam") m = &lhBeam;
    else if (n.rfind("boat", 0) == 0) m = &boat[n[4] - '0']; else if (n == "gull_body") m = &gullBody; else if (n == "gull_wing") m = &gullWing;
    else if (n == "pel_body") m = &pelBody; else if (n == "pel_wing") m = &pelWing; else if (n == "signpost") m = &signPost;
    if (m) m->createPacked(p.v.data(), p.v.size(), p.i.data(), p.i.size());
  }
  Mesh* bm[4] = {&gullBody, &gullWing, &pelBody, &pelWing}; int cap[4] = {9, 18, 3, 6};
  for (int i = 0; i < 4; i++) { birdI[i].create(cap[i]); birdI[i].attach(bm[i]->vao); birdData[i].resize(cap[i] * 12); }
  // sign plate: PlaneGeometry(1.1,1.1).rotateY(-PI/2), double sided, alphaTest 0.4, roughness 0.5
  { std::vector<PVertex> v(4); uint32_t idx[6] = {0, 1, 2, 2, 1, 3};
    for (int k = 0; k < 4; k++) { PVertex& p = v[k]; p = PVertex{}; p.p[0] = 0; p.p[1] = (k & 2) ? -0.55f : 0.55f; p.p[2] = (k & 1) ? 0.55f : -0.55f;
      p.n[0] = -127; p.flags = 1; for (int c = 0; c < 4; c++) p.c[c] = 255; p.m[0] = 128; p.m[1] = 0; p.m[2] = 0; p.m[3] = 255; }
    signPlate.createPacked(v.data(), 4, idx, 6); }
  const char* sn[3] = {"tex/sign_crossing.png", "tex/sign_speed.png", "tex/sign_fish.png"};
  for (int i = 0; i < 3; i++) texSign[i].loadPNG(sn[i], true, true, false);
  // fireflies (mulberry32(5))
  { Mulberry rr(5); std::vector<PVertex> v(160);
    for (auto& p : v) { p = PVertex{}; p.p[0] = rr() * 200 - 60; p.p[1] = 0.4f + rr() * 2.6f; p.p[2] = 4.5f + rr() * 22; p.c[0] = uint8_t(rr() * 255); }
    ffMesh.createPacked(v.data(), v.size(), nullptr, 0); ffMesh.prim = GL_POINTS; }
  // lamp pools: CircleGeometry(3.4) scaled 1.25 in x at LAMP_X, z 1.3, over 7 chunks
  { std::vector<PVertex> v; std::vector<uint32_t> idx;
    for (int c = 0; c < 7; c++) for (float lx : {12.f, 48.f}) { uint32_t b = (uint32_t)v.size();
      for (int k = 0; k < 4; k++) { PVertex p{}; float u = (k & 1) ? 1.f : 0.f, w = (k & 2) ? 1.f : 0.f; p.p[0] = c * CHUNK + lx + (u * 2 - 1) * 3.4f * 1.25f; p.p[1] = 0.03f; p.p[2] = 1.3f + (w * 2 - 1) * 3.4f;
        p.c[0] = uint8_t(u * 255); p.c[1] = uint8_t(w * 255); v.push_back(p); }
      idx.insert(idx.end(), {b, b + 2, b + 1, b + 1, b + 2, b + 3}); }
    pools.createPacked(v.data(), v.size(), idx.data(), idx.size()); }
  makeProgX(litBird, "lit6", 6, glsl::LIT_VS, glsl::LIT_FS);
  makeProgX(litSign, "lit5", 5, glsl::LIT_VS, glsl::LIT_FS);
  makeProgX(beamS, "beam", 0, glsl::BEAM_VS, glsl::BEAM_FS);
  makeProgX(ffS, "ff", 0, glsl::FF_VS, glsl::FF_FS);
  makeProgX(poolS, "pool", 0, glsl::POOL_VS, glsl::POOL_FS);
}

static void put34(float* d, const ax::Mat4& m) { for (int r = 0; r < 3; r++) { d[r * 4] = m.m[r]; d[r * 4 + 1] = m.m[4 + r]; d[r * 4 + 2] = m.m[8 + r]; d[r * 4 + 3] = m.m[12 + r]; } }
static ax::Mat4 compose(V3 p, ax::Quaternion q, float s) { ax::Mat4 T, R, S; ax::Mat4::createTranslation(p.x, p.y, p.z, &T); ax::Mat4::createRotation(q, &R); ax::Mat4::createScale(s, s, s, &S); return T * R * S; }

void Game::updateExtras(float dt) {
  double dist = S.distance; float t = S.t;
  // lighthouse
  { double lp = 1600; double x = std::fmod(std::fmod(330 - dist + 420, lp) + lp, lp) - 420;
    ax::Mat4::createTranslation(float(x), SEA_LEVEL - 0.5f, -118, &lhM);
    for (int i = 0; i < 2; i++) { ax::Mat4 R; ax::Mat4::createRotationY(t * 0.9f + i * PI, &R); beamM[i] = lhM * R; }
    lanternE = lerpf(0.4f, 8, smoothstepf(0.2f, 0.7f, night)); }
  // boats
  for (int i = 0; i < 5; i++) {
    double ax_ = 60 + i * 170 + t * (0.6 + i * 0.25), period = 900; float z = -42.f - i * 34;
    double x = std::fmod(std::fmod(ax_ - dist + 300, period) + period, period) - 300; double wx = x + dist;
    float h = waveHeight(wx, z, t, settings.waveAmp), hF = waveHeight(wx + 2.5, z, t, settings.waveAmp), hS = waveHeight(wx, z + 1.2, t, settings.waveAmp);
    float rz = std::atan2(hF - h, 2.5f) * 0.8f, rx = -std::atan2(hS - h, 1.2f) * 0.8f + std::sin(t * 0.8f + i * 1.3f) * 0.04f;
    ax::Mat4 T, Rx, Rz, Sc; ax::Mat4::createTranslation(float(x), h + 0.15f, z, &T); ax::Mat4::createRotationX(rx, &Rx); ax::Mat4::createRotationZ(rz, &Rz);
    float s = 0.9f + i * 0.12f; ax::Mat4::createScale(s, s, s, &Sc); boatM[i] = T * Rx * Rz * Sc;  // Euler XYZ
  }
  // birds (setBird)
  auto setBird = [&](int set, int i, V3 pos, float yaw, float bank, float flap, float scale) {
    ax::Quaternion qy(ax::Vec3(0, 1, 0), yaw), qx(ax::Vec3(1, 0, 0), bank); ax::Quaternion q = qy * qx;  // 'YXZ'
    ax::Mat4 bm = compose(pos, q, scale); put34(&birdData[set * 2][i * 12], bm);
    ax::Mat4 wr = bm * compose(V3(0.02f, 0.03f, 0.05f), ax::Quaternion(ax::Vec3(1, 0, 0), -flap), 1);
    ax::Mat4 wl = bm * compose(V3(0.02f, 0.03f, -0.05f), ax::Quaternion(ax::Vec3(1, 0, 0), PI + flap), 1);
    put34(&birdData[set * 2 + 1][(i * 2) * 12], wr); put34(&birdData[set * 2 + 1][(i * 2 + 1) * 12], wl);
  };
  for (int i = 0; i < 9; i++) {
    float cx = (i - 4) * 7.f, cy = 7 + (i % 3) * 2.5f, cz = -12.f - (i % 4) * 7, rx = 9.f + i, rz = 4.f + (i % 3) * 2, w = 0.12f + i * 0.013f, ph = i * 1.7f, fl = 5.5f + (i % 3), s = 0.9f + (i % 3) * 0.15f;
    float a = t * w + ph; V3 gp(cx + std::sin(a) * rx + 6, cy + std::sin(a * 2.3f) * 1.2f, cz + std::cos(a) * rz);
    float vx = std::cos(a) * rx, vz = -std::sin(a) * rz; float yaw = std::atan2(-vz, vx);
    bool glide = std::sin(t * 0.5f + ph) > 0.2f; float flap = glide ? 0.12f + std::sin(t * 1.5f + i) * 0.05f : std::sin(t * fl + ph) * 0.6f;
    setBird(0, i, gp, yaw, -std::sin(a) * 0.35f, flap, s);
  }
  { float fx = std::fmod(t * 3.2f, 560.f) - 260;
    for (int i = 0; i < 3; i++) { float off = i == 0 ? 0 : 2.6f; float side = i == 1 ? -1.f : 1.f;
      V3 gp(fx - off, 5.5f + std::sin(t * 0.6f + i) * 0.35f + (i ? 0.4f : 0), -22 + (i ? side * 2.4f : 0));
      float flap = std::sin(t * 2.4f - i * 0.6f) * 0.38f * (std::sin(t * 0.25f) > -0.3f ? 1 : 0.15f);
      setBird(1, i, gp, 0, std::sin(t * 0.4f + i) * 0.06f, flap, 1.0f); } }
  birdDirty = true;
}

void Game::drawExtras(bool shadow) {
  const Shader& s = shadow ? shStatic : litStatic; s.use(); GLint lm = s.loc("uModel");
  glUniformMatrix4fv(lm, 1, GL_FALSE, lhM.m); lhStatic.draw();
  if (!shadow) { glUniform4f(s.loc("uTint"), 1, 1, 1, lanternE / 8.f); lhLantern.draw(); glUniform4f(s.loc("uTint"), 1, 1, 1, 1); }
  if (!shadow) for (int i = 0; i < 5; i++) { if (std::fabs(boatM[i].m[12]) > 330) continue; glUniformMatrix4fv(lm, 1, GL_FALSE, boatM[i].m); boat[i].draw(); }
  // signs on odd chunks (x=30, z=3.55)
  for (int sl = 0; sl < 7; sl++) {
    int idx = chunkIndex[sl]; if ((idx & 1) == 0) continue;
    float ox = float(double(idx) * CHUNK - S.distance) + 30; if (ox < -40 || ox > 260) continue;
    ax::Mat4 M; ax::Mat4::createTranslation(ox, 0.14f, 3.55f, &M); s.use(); glUniformMatrix4fv(lm, 1, GL_FALSE, M.m); signPost.draw();
    const Shader& ps = shadow ? shStatic : litSign; ps.use();  // plate (alpha tested; shadow uses full quad)
    ax::Mat4 P; ax::Mat4::createTranslation(ox - 0.03f, 2.19f, 3.55f, &P); glUniformMatrix4fv(ps.loc("uModel"), 1, GL_FALSE, P.m);
    if (!shadow) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texSign[std::abs(int(std::floor(idx / 2.0))) % 3].id); }
    signPlate.draw();
  }
  if (shadow) return;
  if (birdDirty) { birdI[0].upload(birdData[0].data(), 9); birdI[1].upload(birdData[1].data(), 18); birdI[2].upload(birdData[2].data(), 3); birdI[3].upload(birdData[3].data(), 6); birdDirty = false; }
  litBird.use(); gullBody.draw(9); gullWing.draw(18); pelBody.draw(3); pelWing.draw(6);
}

void Game::drawAdditive() {
  glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE); glDepthMask(GL_FALSE);
  float beamN = smoothstepf(0.2f, 0.8f, night), ffN = smoothstepf(0.35f, 0.9f, night), poolO = smoothstepf(0.2f, 0.7f, night) * 0.5f;
  if (beamN > 0.001f) { beamS.use(); glUniform1f(beamS.loc("uNight"), beamN); for (int i = 0; i < 2; i++) { glUniformMatrix4fv(beamS.loc("uModel"), 1, GL_FALSE, beamM[i].m); lhBeam.draw(); } }
  if (poolO > 0.001f) { poolS.use(); glUniform1f(poolS.loc("uOpacity"), poolO); ax::Mat4 M; ax::Mat4::createTranslation(F.cloudOff[2], 0, 0, &M); glUniformMatrix4fv(poolS.loc("uModel"), 1, GL_FALSE, M.m); pools.draw(); }
  if (ffN > 0.001f) {
#if AX_TARGET_PLATFORM != AX_PLATFORM_WASM
    glEnable(GL_PROGRAM_POINT_SIZE);
#endif
    ffS.use(); glUniform1f(ffS.loc("uNight"), ffN); glUniform1f(ffS.loc("uOffset"), float(std::fmod(S.distance, 200.0))); ffMesh.draw(); }
  glDisable(GL_BLEND); glDepthMask(GL_TRUE);
}
}  // namespace pb
