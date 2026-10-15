#include "game/Rider.h"
#include "platform/FileUtils.h"
#include "base/Logging.h"
#include <cstring>
#include <cmath>
using namespace gfx;
namespace pb {

bool Rider::load(const char* file) {
  auto data = ax::FileUtils::getInstance()->getDataFromFile(file);
  const uint8_t* p = data.getBytes(); size_t o = 4;
  if (data.getSize() < 64 || memcmp(p, "PBR2", 4)) return false;
  auto u32 = [&]() { uint32_t v; memcpy(&v, p + o, 4); o += 4; return v; };
  auto i32 = [&]() { int32_t v; memcpy(&v, p + o, 4); o += 4; return v; };
  auto f32 = [&]() { float v; memcpy(&v, p + o, 4); o += 4; return v; };
  nb = u32(); F = u32(); K = u32(); int nc = u32(); int nm = u32(); int nv = u32(); int ni = u32();
  boneCtl.resize(nb); for (auto& b : boneCtl) b = i32();
  for (int i = 0; i < nc; i++) { Ctl c; c.bone = i32(); c.parent = i32(); c.kind = u32(); c.axis.x = f32(); c.axis.y = f32(); c.axis.z = f32(); ctls.push_back(c); }
  poses.resize(size_t(K) * F * nb * 12); memcpy(poses.data(), p + o, poses.size() * 4); o += poses.size() * 4;
  o += nm * 4;  // material table (re-read at end incl. neck)
  const uint8_t* vsrc = p + o; o += size_t(nv) * 16;
  std::vector<uint32_t> idx(ni); for (int i = 0; i < ni; i++) { uint16_t x; memcpy(&x, p + o + i * 2, 2); idx[i] = x; } o += size_t(ni) * 2;
  neckCol[0] = f32(); neckCol[1] = f32(); neckCol[2] = f32(); neckMat = u32();
  bodyB = u32(); headB = u32(); knotB = u32(); helmetB = u32(); glassesB = u32(); o += 64;
  int nm2 = u32(); matTab.assign(p + o, p + o + nm2 * 4); o += nm2 * 4;
  for (int i = 0; i < nm2; i++) if (matTab[i * 4] == 102 && matTab[i * 4 + 1] == 0 && matTab[i * 4 + 2] == 0) matTab[i * 4 + 3] |= 64;  // beak (roughness .4) → clearcoat flag
  if (o + 4 <= data.getSize()) { int nw = u32(); for (int i = 0; i < nw; i++) { WingJ w; w.bone = u32(); w.side = i32(); w.shoulder = u32() != 0; wings.push_back(w); }
    int nl = u32(); for (int i = 0; i < nl; i++) lids.push_back(u32()); }
  std::vector<PVertex> v(nv);
  for (int i = 0; i < nv; i++) {
    const uint8_t* s = vsrc + i * 16; int16_t q[3]; memcpy(q, s, 6); int b = s[6], m = s[7];
    PVertex& d = v[i];
    for (int c = 0; c < 3; c++) d.p[c] = q[c] / 8192.f;
    for (int c = 0; c < 3; c++) d.n[c] = int8_t(s[8 + c]);
    d.flags = matTab[m * 4 + 3];
    d.c[0] = s[11]; d.c[1] = s[12]; d.c[2] = s[13]; d.c[3] = 255;
    d.m[0] = matTab[m * 4]; d.m[1] = matTab[m * 4 + 1]; d.m[2] = matTab[m * 4 + 2]; d.m[3] = uint8_t(b);
  }
  mesh.createPacked(v.data(), v.size(), idx.data(), idx.size());
  // neck tube topology (28 x 18)
  const int TUB = 28, RAD = 18; neckV.resize((TUB + 1) * (RAD + 1));
  std::vector<uint32_t> ni2;
  for (int i = 0; i < TUB; i++) for (int j = 0; j < RAD; j++) { uint32_t a = i * (RAD + 1) + j, b = a + RAD + 1; ni2.insert(ni2.end(), {a, a + 1, b, b, a + 1, b + 1}); }
  for (auto& d : neckV) { d = PVertex{}; d.flags = matTab[neckMat * 4 + 3]; for (int c = 0; c < 3; c++) d.c[c] = uint8_t(neckCol[c]); d.c[3] = 255;
    d.m[0] = matTab[neckMat * 4]; d.m[1] = matTab[neckMat * 4 + 1]; d.m[2] = matTab[neckMat * 4 + 2]; d.m[3] = 0; }
  neck.createPacked(neckV.data(), neckV.size(), ni2.data(), ni2.size(), true);
  bone.resize(nb); palette.resize(nb * 12);
  return true;
}

static ax::Mat4 fromPose(const float* e) {
  ax::Mat4 m; m.m[0] = e[0]; m.m[1] = e[1]; m.m[2] = e[2]; m.m[3] = 0; m.m[4] = e[3]; m.m[5] = e[4]; m.m[6] = e[5]; m.m[7] = 0;
  m.m[8] = e[6]; m.m[9] = e[7]; m.m[10] = e[8]; m.m[11] = 0; m.m[12] = e[9]; m.m[13] = e[10]; m.m[14] = e[11]; m.m[15] = 1; return m;
}
static V3 xf(const ax::Mat4& m, V3 p) { return V3(m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12], m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13], m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14]); }

// THREE.CatmullRomCurve3 (centripetal) getPoint
static V3 catmull(const V3* P, int l, float t) {
  float pp = (l - 1) * t; int ip = int(std::floor(pp)); float w = pp - ip;
  if (w == 0 && ip == l - 1) { ip = l - 2; w = 1; }
  V3 p0 = ip > 0 ? P[ip - 1] : (P[0] - P[1]) + P[0];
  V3 p1 = P[ip], p2 = P[ip + 1];
  V3 p3 = ip + 2 < l ? P[ip + 2] : (P[l - 1] - P[l - 2]) + P[l - 1];
  auto d2 = [](V3 a, V3 b) { V3 d = a - b; return d.dot(d); };
  float dt0 = std::pow(d2(p0, p1), 0.25f), dt1 = std::pow(d2(p1, p2), 0.25f), dt2 = std::pow(d2(p2, p3), 0.25f);
  if (dt1 < 1e-4f) dt1 = 1; if (dt0 < 1e-4f) dt0 = dt1; if (dt2 < 1e-4f) dt2 = dt1;
  auto cr = [&](float x0, float x1, float x2, float x3) {
    float t1 = ((x1 - x0) / dt0 - (x2 - x0) / (dt0 + dt1) + (x2 - x1) / dt1) * dt1;
    float t2 = ((x2 - x1) / dt1 - (x3 - x1) / (dt1 + dt2) + (x3 - x2) / dt2) * dt1;
    float c2 = -3 * x1 + 3 * x2 - 2 * t1 - t2, c3 = 2 * x1 - 2 * x2 + t1 + t2;
    return x1 + t1 * w + c2 * w * w + c3 * w * w * w;
  };
  return V3(cr(p0.x, p1.x, p2.x, p3.x), cr(p0.y, p1.y, p2.y, p3.y), cr(p0.z, p1.z, p2.z, p3.z));
}

static ax::Mat4 axisRot(V3 a, float ang) { ax::Mat4 m; ax::Mat4::createRotation(ax::Vec3(a.x, a.y, a.z), ang, &m); return m; }

void Rider::update(const RiderPoseIn& in) {
  float dt = in.dt;
  spread = damp(spread, in.trick, 5, dt);
  float k = smoothstepf(0, 1, spread);
  float fp = std::fmod(in.crank / TAU, 1.f); if (fp < 0) fp += 1; fp *= F;
  int f0 = int(fp) % F, f1 = (f0 + 1) % F; float fr = fp - std::floor(fp);
  const size_t PS = size_t(nb) * 12;
  const float* A0 = &poses[f0 * PS]; const float* A1 = &poses[f1 * PS];
  const float* B0 = &poses[(F + f0) * PS]; const float* B1 = &poses[(F + f1) * PS];
  float w00 = (1 - fr) * (1 - k), w01 = fr * (1 - k), w10 = (1 - fr) * k, w11 = fr * k;
  std::vector<ax::Mat4>& baked = bone;
  for (int b = 0; b < nb; b++) { float e[12]; for (int i = 0; i < 12; i++) e[i] = A0[b * 12 + i] * w00 + A1[b * 12 + i] * w01 + B0[b * 12 + i] * w10 + B1[b * 12 + i] * w11; baked[b] = fromPose(e); }
  // honk / gulp (pelican.js)
  honkK = 0; if (in.honk) honkT = 0; if (in.gulp) gulpT = 0;
  if (honkT >= 0) { honkT += dt; honkK = std::sin(std::min(1.f, honkT / 0.9f) * PI); if (honkT > 0.9f) honkT = -1; }
  gulpK = 0; if (gulpT >= 0) { gulpT += dt; gulpK = gulpT < 0.18f ? gulpT / 0.18f : std::max(0.f, 1 - (gulpT - 0.18f) / 0.25f); if (gulpT > 1.2f) gulpT = -1; }
  // head look
  ax::Mat4 Hb = baked[headB]; V3 hp(Hb.m[12], Hb.m[13], Hb.m[14]);
  V3 d = in.look - hp;
  float yawT = std::atan2(-d.z, d.x), pitchT = std::atan2(d.y, std::hypot(d.x, d.z));
  if (d.x < -0.2f) yawT = clampf(yawT, -1.2f, 1.2f);
  yawT = clampf(yawT, -1.05f, 1.05f); pitchT = clampf(pitchT, -0.55f, 0.5f);
  pitchT += honkK * 0.55f + gulpK * 0.35f + in.trick * 0.15f;
  yaw = damp(yaw, yawT, 5, dt); pitch = damp(pitch, pitchT, 6, dt);
  V3 db(6 - hp.x, 1.15f - hp.y, 0); float pb0 = clampf(std::atan2(db.y, std::fabs(db.x)), -0.55f, 0.5f) + in.trick * 0.15f;
  jaw = damp(jaw, std::max(honkK * 0.55f, gulpK * 0.7f), 20, dt);
  // controllers (ordered parents first in bake: steer, rotor0, rotor1(steer), head, jaw(head), upper(head))
  std::vector<ax::Mat4>& C = ctlScratch; C.resize(ctls.size());
  for (size_t c = 0; c < ctls.size(); c++) {
    const Ctl& ct = ctls[c]; ax::Mat4 P; if (ct.parent >= 0) P = C[ct.parent];
    ax::Mat4 W = P * baked[ct.bone]; ax::Mat4 R;
    if (ct.kind == 1) {
      V3 q(W.m[12], W.m[13] + honkK * 0.03f, W.m[14]); q.x -= honkK * 0.02f;
      ax::Mat4 T, Ti, Ry, Rz, Tr; ax::Mat4::createTranslation(q.x, q.y, q.z, &T); ax::Mat4::createTranslation(-W.m[12], -W.m[13], -W.m[14], &Ti);
      ax::Mat4::createRotationY(yaw, &Ry); ax::Mat4::createRotationZ(pitch - pb0, &Rz);
      C[c] = T * Ry * Rz * Ti * P; continue;
    }
    float ang = 0;
    if (c == 0) ang = in.steer; else if (c == 1 || c == 2) ang = -in.wheel; else if (c == 4) ang = -jaw; else if (c == 5) ang = jaw * 0.12f;
    R = axisRot(ct.axis, ang);
    ax::Mat4 Wi = baked[ct.bone]; Wi.inverse();
    C[c] = P * baked[ct.bone] * R * Wi;
  }
  for (int b = 0; b < nb; b++) if (boneCtl[b] >= 0) bone[b] = C[boneCtl[b]] * baked[b];
  // neck (pelican.js)
  ax::Mat4 H = bone[headB]; V3 headP(H.m[12], H.m[13], H.m[14]);
  V3 nbP = xf(bone[bodyB], V3(0.3f, 0.1f, 0));
  V3 pts[5];
  pts[0] = nbP + V3(-0.03f, -0.03f, 0); pts[1] = nbP + V3(0.035f, 0.07f, 0);
  pts[2] = nbP + (headP - nbP) * 0.45f + V3(0.035f, 0, 0);
  { V3 a = xf(H, V3(-0.075f, -0.1f, 0)), b = headP + V3(-0.075f, -0.1f, 0); pts[3] = a + (b - a) * 0.4f; }
  pts[4] = xf(H, V3(-0.03f, -0.03f, 0));
  const int TUB = 28, RAD = 18; V3 Pp[TUB + 1], Tt[TUB + 1];
  for (int i = 0; i <= TUB; i++) Pp[i] = catmull(pts, 5, float(i) / TUB);
  for (int i = 0; i <= TUB; i++) { V3 t = Pp[std::min(TUB, i + 1)] - Pp[std::max(0, i - 1)]; Tt[i] = t * (1.f / std::sqrt(t.dot(t))); }
  V3 N(0, 0, 1); if (std::fabs(Tt[0].dot(N)) > 0.9f) N = V3(1, 0, 0);
  N = N - Tt[0] * Tt[0].dot(N); N = N * (1.f / std::sqrt(N.dot(N)));
  for (int i = 0; i <= TUB; i++) {
    V3 T = Tt[i]; if (i > 0) { N = N - T * T.dot(N); N = N * (1.f / std::sqrt(N.dot(N))); }
    V3 B(T.y * N.z - T.z * N.y, T.z * N.x - T.x * N.z, T.x * N.y - T.y * N.x);
    float t = float(i) / TUB; float r = lerpf(0.085f, 0.05f, smoothstepf(0, 0.45f, t)) + 0.004f * std::sin(t * PI) + (t > 0.85f ? (t - 0.85f) * 0.05f : 0);
    for (int j = 0; j <= RAD; j++) {
      float th = float(j) / RAD * TAU, c = std::cos(th), s = std::sin(th);
      V3 dd = N * c + B * s; PVertex& v = neckV[i * (RAD + 1) + j];
      v.p[0] = Pp[i].x + dd.x * r; v.p[1] = Pp[i].y + dd.y * r; v.p[2] = Pp[i].z + dd.z * r;
      v.n[0] = int8_t(std::lround(dd.x * 127)); v.n[1] = int8_t(std::lround(dd.y * 127)); v.n[2] = int8_t(std::lround(dd.z * 127));
    }
  }
  neckDirty = true;
  // knot at neck point round(28*0.2)=6, z axis along tangent
  { int ki = 6; knotP = Pp[ki]; V3 z(0, 0, 1), t = Tt[ki]; V3 ax_(z.y * t.z - z.z * t.y, z.z * t.x - z.x * t.z, z.x * t.y - z.y * t.x);
    float s = std::sqrt(ax_.dot(ax_)), c = z.dot(t); ax::Mat4 R; if (s > 1e-6f) R = axisRot(ax_ * (1.f / s), std::atan2(s, c));
    ax::Mat4 T; ax::Mat4::createTranslation(knotP.x, knotP.y, knotP.z, &T); bone[knotB] = T * R; }
  // wing flap (pelican.js: rotation about the shoulder x axis, only in the spread pose) + blink
  { float k = in.trick * in.trick * (3 - 2 * in.trick); float delta = std::sin(in.t * 8.5f) * 0.55f * (in.airborne ? 1.f : 0.f) + std::sin(in.t * 2.6f) * 0.32f * k;
    if (k > 1e-3f && std::fabs(delta) > 1e-4f) for (int sd : {-1, 1}) {
      V3 sh; for (auto& w : wings) if (w.side == sd && w.shoulder) sh = V3(bone[w.bone].m[12], bone[w.bone].m[13], bone[w.bone].m[14]);
      ax::Mat4 T, Ti, R; ax::Mat4::createTranslation(sh.x, sh.y, sh.z, &T); ax::Mat4::createTranslation(-sh.x, -sh.y, -sh.z, &Ti); ax::Mat4::createRotationX(-sd * delta * k, &R);
      ax::Mat4 M = T * R * Ti; for (auto& w : wings) if (w.side == sd) bone[w.bone] = M * bone[w.bone]; } }
  nextBlink -= in.dt;
  if (nextBlink <= 0 && blinkT < 0) { blinkT = 0; nextBlink = 2 + float(std::rand()) / RAND_MAX * 4; }
  float blink = 0; if (blinkT >= 0) { blinkT += in.dt; blink = std::sin(std::min(1.f, blinkT / 0.16f) * float(M_PI)); if (blinkT > 0.16f) blinkT = -1; }
  if (blink > 0.05f) { ax::Mat4 S; ax::Mat4::createScale(blink / 0.001f, blink / 0.001f, blink / 0.001f, &S); for (int l : lids) bone[l] = bone[l] * S; }
  if (!showHelmet) bone[helmetB] = ax::Mat4::ZERO;
  if (!showGlasses) bone[glassesB] = ax::Mat4::ZERO;
  for (int b = 0; b < nb; b++) { const float* m = bone[b].m; float* d2 = &palette[b * 12];
    for (int r = 0; r < 3; r++) { d2[r * 4] = m[r]; d2[r * 4 + 1] = m[4 + r]; d2[r * 4 + 2] = m[8 + r]; d2[r * 4 + 3] = m[12 + r]; } }
}

V3 Rider::mouthPos() const { return xf(bone[headB], V3(0.1999f, -0.0956f, 0)); }

void Rider::draw(const Shader& s) {
  s.set("uBones", palette.data(), size_t(nb) * 48);  // nb bones x 3 rows (vec4 uBones[144])
  if (neckDirty) { neck.updatePacked(neckV.data(), neckV.size()); neckDirty = false; }
  gfx::draw(s, mesh); gfx::draw(s, neck);
}
}  // namespace pb
