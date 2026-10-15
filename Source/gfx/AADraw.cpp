#include "AADraw.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
USING_NS_AX;

namespace pb {
static const Color4F CLEAR(0, 0, 0, 0);  // premultiplied-consistent fade target (rgb*cov, a*cov)

static bool aaOn() { static const bool on = !std::getenv("PB2_NOUIAA"); return on; }  // PB2_NOUIAA=1: stock DrawNode (A/B perf)
float AADrawNode::px() {
  auto* v = Director::getInstance()->getRenderView();
  float s = v ? std::max(v->getScaleX(), v->getScaleY()) : 1.f;
  return s > 0 ? 1.f / s : 1.f;
}
unsigned AADrawNode::circleSegs(float r) {
  float rp = std::max(0.5f, r / px());  // radius in device px; keep sagitta under ~0.2 px
  float a = std::acos(std::max(-1.f, 1.f - 0.2f / rp));
  return unsigned(std::clamp(int(std::ceil(2 * M_PI / std::max(a, 1e-3f))), 12, 160));
}
void AADrawNode::tri(const Vec2& a, const Color4F& ca, const Vec2& b, const Color4F& cb, const Vec2& c, const Color4F& cc) {
  size_t o = _triangles.size(); _triangles.expand(3); auto* t = _triangles.data() + o;
  t[0] = {a, Color4B(ca), Vec2::ZERO}; t[1] = {b, Color4B(cb), Vec2::ZERO}; t[2] = {c, Color4B(cc), Vec2::ZERO};
  _trianglesDirty = true;
}
static std::vector<Vec2> clean(const Vec2* p, unsigned n, bool closed) {
  std::vector<Vec2> v; v.reserve(n);
  for (unsigned i = 0; i < n; i++) if (v.empty() || v.back().distanceSquared(p[i]) > 1e-8f) v.push_back(p[i]);
  if (closed) while (v.size() > 2 && v.back().distanceSquared(v.front()) <= 1e-8f) v.pop_back();
  return v;
}
// per-vertex unit offset direction (miter of the two adjacent edge normals, right-hand side), length = 1/cos(half angle)
static std::vector<Vec2> miters(const std::vector<Vec2>& p, bool closed) {
  size_t n = p.size(); std::vector<Vec2> m(n);
  auto nrm = [](Vec2 a, Vec2 b) { Vec2 d = b - a; float l = d.length(); return l > 0 ? Vec2(d.y / l, -d.x / l) : Vec2::ZERO; };
  for (size_t i = 0; i < n; i++) {
    bool hasP = closed || i > 0, hasN = closed || i + 1 < n;
    Vec2 n0 = hasP ? nrm(p[(i + n - 1) % n], p[i]) : Vec2::ZERO, n1 = hasN ? nrm(p[i], p[(i + 1) % n]) : Vec2::ZERO;
    if (!hasP) n0 = n1; if (!hasN) n1 = n0;
    Vec2 s = n0 + n1; float l = s.length();
    if (l < 1e-4f) { m[i] = n0; continue; }
    s = s / l; float d = std::max(0.25f, s.dot(n0)); m[i] = s / d;  // clamp miter length to 4x
  }
  return m;
}
void AADrawNode::fill(const std::vector<Vec2>& p, const Color4F& c, bool convex) {
  size_t n = p.size(); if (n < 3 || c.a <= 0) return;
  float area = 0; for (size_t i = 0; i < n; i++) area += p[i].cross(p[(i + 1) % n]);
  float sgn = area < 0 ? 1.f : -1.f;  // right-hand normals point outward for clockwise polygons
  auto m = miters(p, true); float f = px();
  if (convex) {
    std::vector<Vec2> in(n), out(n);
    for (size_t i = 0; i < n; i++) { in[i] = p[i] - m[i] * (sgn * 0.5f * f); out[i] = p[i] + m[i] * (sgn * 0.5f * f); }
    for (size_t i = 1; i + 1 < n; i++) tri(in[0], c, in[i], c, in[i + 1], c);
    for (size_t i = 0; i < n; i++) { size_t j = (i + 1) % n; tri(in[i], c, in[j], c, out[j], CLEAR); tri(in[i], c, out[j], CLEAR, out[i], CLEAR); }
  } else {  // arbitrary polygon: exact fill via the stock triangulator, then fade outward over one device px
    DrawNode::drawSolidPoly(p.data(), unsigned(n), c);
    for (size_t i = 0; i < n; i++) { size_t j = (i + 1) % n; Vec2 oi = p[i] + m[i] * (sgn * f), oj = p[j] + m[j] * (sgn * f);
      tri(p[i], c, p[j], c, oj, CLEAR); tri(p[i], c, oj, CLEAR, oi, CLEAR); }
  }
}
void AADrawNode::stroke(const std::vector<Vec2>& p, bool closed, const Color4F& c0, float w) {
  size_t n = p.size(); if (n < 2 || c0.a <= 0 || w <= 0) return;
  float f = px(); Color4F c = c0; if (w < f) { c = Color4F(c.r, c.g, c.b, c.a * w / f); w = f; }  // sub-pixel lines: thinner = fainter
  auto m = miters(p, closed); float core = std::max(0.f, w * 0.5f - f * 0.5f), edge = w * 0.5f + f * 0.5f;
  size_t segs = closed ? n : n - 1;
  for (size_t i = 0; i < segs; i++) {
    size_t j = (i + 1) % n; const Vec2 &a = p[i], &b = p[j]; const Vec2 &ma = m[i], &mb = m[j];
    auto band = [&](float r0, const Color4F& k0, float r1, const Color4F& k1) {
      Vec2 a0 = a + ma * r0, b0 = b + mb * r0, a1 = a + ma * r1, b1 = b + mb * r1;
      tri(a0, k0, b0, k0, b1, k1); tri(a0, k0, b1, k1, a1, k1); };
    if (core > 0) { band(-core, c, core, c); band(core, c, edge, CLEAR); band(-core, c, -edge, CLEAR); }
    else { band(0, c, edge, CLEAR); band(0, c, -edge, CLEAR); }
  }
}
void AADrawNode::drawSolidPoly(const Vec2* p, unsigned n, const Color4F& c, float thickness, const Color4F& border, bool isconvex) {
  if (!aaOn()) { DrawNode::drawSolidPoly(p, n, c, thickness, border, isconvex); return; }
  auto v = clean(p, n, true);
  bool convex = isconvex; if (!convex && v.size() >= 3) {  // cheap convexity test (all cross products same sign)
    int pos = 0, neg = 0; for (size_t i = 0; i < v.size(); i++) { float cr = (v[(i + 1) % v.size()] - v[i]).cross(v[(i + 2) % v.size()] - v[(i + 1) % v.size()]); if (cr > 1e-7f) pos++; else if (cr < -1e-7f) neg++; }
    convex = !(pos && neg); }
  fill(v, c, convex);
  if (thickness > 0 && border.a > 0) stroke(v, true, border, thickness * 2);  // stock DrawNode borders are 2*thickness wide
}
void AADrawNode::drawPolygon(const Vec2* p, int n, const Color4F& fillC, float thickness, const Color4F& border, bool isconvex) {
  drawSolidPoly(p, unsigned(n), fillC, thickness, border, isconvex);
}
// stock DrawNode line widths: thickness 1 = hairline (1 design px here), otherwise the extruded outline is 2*thickness wide
static float lineW(float t) { return t == 1.f ? 1.f : 2.f * t; }
void AADrawNode::drawPoly(const Vec2* p, unsigned n, bool closed, const Color4F& c, float thickness) { if (!aaOn()) { DrawNode::drawPoly(p, n, closed, c, thickness); return; } stroke(clean(p, n, closed), closed, c, lineW(thickness)); }
void AADrawNode::drawSolidRect(const Vec2& a, const Vec2& b, const Color4F& c) {
  // axis-aligned rects stay hard-edged: they tile (gradient bands, bars) and a fringe would draw seams between them
  DrawNode::drawSolidRect(a, b, c);
}
static std::vector<Vec2> ring(const Vec2& c, float r, float ang, unsigned segs) {
  std::vector<Vec2> v(segs); for (unsigned i = 0; i < segs; i++) { float a = ang + float(i) * 2 * float(M_PI) / segs; v[i] = c + Vec2(std::cos(a), std::sin(a)) * r; } return v;
}
void AADrawNode::drawSolidCircle(const Vec2& ctr, float r, float angle, unsigned segs, float sx, float sy, const Color4F& c) {
  if (!aaOn()) { DrawNode::drawSolidCircle(ctr, r, angle, segs, sx, sy, c); return; }
  if (r <= 0) return; auto v = ring(Vec2::ZERO, r, angle, std::max(segs, circleSegs(r * std::max(sx, sy))));
  for (auto& p : v) p = ctr + Vec2(p.x * sx, p.y * sy); fill(v, c, true);
}
void AADrawNode::drawCircle(const Vec2& ctr, float r, float angle, unsigned segs, bool lineToCenter, const Color4F& c, float thickness) {
  if (!aaOn()) { DrawNode::drawCircle(ctr, r, angle, segs, lineToCenter, c, thickness); return; }
  if (r <= 0) return; auto v = ring(ctr, r, angle, std::max(segs, circleSegs(r))); stroke(v, true, c, lineW(thickness));
  if (lineToCenter) stroke({ctr, v[0]}, false, c, lineW(thickness));
}
void AADrawNode::drawSegment(const Vec2& a, const Vec2& b, float thickness, const Color4F& c) {
  if (!aaOn()) { DrawNode::drawSegment(a, b, thickness, c); return; }
  float r = std::max(thickness, px()) * 0.5f; Vec2 d = b - a; float l = d.length();
  if (l < 1e-5f) { drawSolidCircle(a, r, 0, 0, c); return; }
  float base = std::atan2(d.y, d.x); unsigned half = std::max(4u, circleSegs(r) / 2);  // capsule: round caps like DrawNode::EndType::Round
  std::vector<Vec2> v; v.reserve(2 * half + 2);
  for (unsigned i = 0; i <= half; i++) { float t = base - float(M_PI) / 2 + float(M_PI) * i / half; v.push_back(b + Vec2(std::cos(t), std::sin(t)) * r); }
  for (unsigned i = 0; i <= half; i++) { float t = base + float(M_PI) / 2 + float(M_PI) * i / half; v.push_back(a + Vec2(std::cos(t), std::sin(t)) * r); }
  fill(v, c, true);
}
}  // namespace pb
