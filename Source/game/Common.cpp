#include "game/Common.h"
namespace pb {
const Perlin& perlin() { static Perlin P; return P; }
float fbm2(double x, double z, int oct, double seed) {
  double s = 0, a = 0.5, f = 1;
  for (int i = 0; i < oct; i++) { s += a * perlin().noise(x * f, z * f, seed + i * 7.31); a *= 0.5; f *= 2.03; }
  return float(s);
}
float terrainHeight(double x, float z) {
  if (z >= 0) {
    float t = smoothstepf(3.8f, 28.f, z);
    float big = 2.5f + 17.f * smoothstepf(16.f, 115.f, z);
    float n = fbm2(x * 0.013, z * 0.013, 4, 1.3);
    float small = fbm2(x * 0.085, z * 0.085, 2, 5.1);
    return -0.06f + t * std::max(0.f, big * (0.62f + 1.1f * n) + small * 1.3f);
  }
  float s = -z;
  float u = smoothstepf(3.2f, 30.f, s);
  float dunes = fbm2(x * 0.07, z * 0.07, 2, 9.7) * 0.5f * smoothstepf(3.6f, 6.5f, s) * (1 - smoothstepf(9.f, 13.5f, s));
  return -0.08f - u * 3.9f + dunes;
}
static float s2l(float c) { return c < 0.04045f ? c * 0.0773993808f : std::pow(c * 0.9478672986f + 0.0521327014f, 2.4f); }
Col hexLin(uint32_t h) { return {s2l(((h >> 16) & 255) / 255.f), s2l(((h >> 8) & 255) / 255.f), s2l((h & 255) / 255.f)}; }

struct Key { float e; uint32_t zen, hor, gnd, sun; float li; uint32_t hs, hg; float hi; uint32_t fog; float exp; uint32_t ct, cs; float bloom; };
static const Key KEYS[] = {
  {-24, 0x02030a, 0x070a18, 0x030409, 0x000000, 0.0f, 0x1f2c58, 0x07070c, 0.42f, 0x080c1a, 1.5f, 0x161b2c, 0x070912, 0.95f},
  {-9, 0x081028, 0x23264a, 0x0b0c16, 0xff5a3a, 0.0f, 0x2a3564, 0x0c0c14, 0.45f, 0x161a33, 1.4f, 0x2d2a48, 0x0f1024, 0.85f},
  {-2, 0x1c2a5e, 0xd0694a, 0x2b1d20, 0xff5b2a, 0.25f, 0x50507a, 0x2b1c1c, 0.45f, 0x6a4a5a, 1.18f, 0xf07a5a, 0x3a2a4a, 0.7f},
  {3, 0x2f4f9a, 0xff9658, 0x5a3a2c, 0xff8a3c, 1.3f, 0x8a86b0, 0x4a3326, 0.55f, 0xd58a6a, 1.0f, 0xffb487, 0x6a5070, 0.55f},
  {10, 0x3a6fc0, 0xffc896, 0x6e5a48, 0xffc27e, 2.3f, 0xa9c2e6, 0x6b5a40, 0.65f, 0xe8c2a2, 0.95f, 0xfff0dc, 0x9aa2b8, 0.42f},
  {28, 0x2f76d2, 0xb9dbf4, 0x7a7058, 0xfff0d6, 3.0f, 0xb8d8ff, 0x6f6448, 0.8f, 0xbcd6ea, 0.9f, 0xffffff, 0xb4c2d6, 0.32f},
  {75, 0x256bd4, 0xc6e3fb, 0x7c7460, 0xffffff, 3.3f, 0xc0dcff, 0x6f6448, 0.85f, 0xc4dcf0, 0.88f, 0xffffff, 0xbcc8da, 0.3f},
};
void samplePalette(float e, Palette& o) {
  int i = 0, n = sizeof(KEYS) / sizeof(KEYS[0]);
  while (i < n - 2 && e > KEYS[i + 1].e) i++;
  const Key &a = KEYS[i], &b = KEYS[i + 1];
  float t = clampf((e - a.e) / (b.e - a.e), 0, 1);
  auto C = [&](uint32_t x, uint32_t y) { return lerpc(hexLin(x), hexLin(y), t); };
  o.zen = C(a.zen, b.zen); o.hor = C(a.hor, b.hor); o.gnd = C(a.gnd, b.gnd); o.sun = C(a.sun, b.sun);
  o.hs = C(a.hs, b.hs); o.hg = C(a.hg, b.hg); o.fog = C(a.fog, b.fog); o.ct = C(a.ct, b.ct); o.cs = C(a.cs, b.cs);
  o.li = lerpf(a.li, b.li, t); o.hi = lerpf(a.hi, b.hi, t); o.exp = lerpf(a.exp, b.exp, t); o.bloom = lerpf(a.bloom, b.bloom, t);
}
V3 sunDirection(float hour) {
  float dayT = (hour - 6) / 12, nightT = std::fmod(hour - 18 + 24, 24.f) / 12;
  float e = (hour >= 6 && hour <= 18) ? std::sin(dayT * PI) * (66 * PI / 180) : -std::sin(nightT * PI) * 0.9f;
  float az = (120 - dayT * 180) * PI / 180;
  return {std::cos(e) * std::cos(az), std::sin(e), std::cos(e) * std::sin(az)};
}
V3 moonDirection(float hour) {
  float nt = std::fmod(hour - 18 + 24, 24.f) / 12;
  float e = std::sin(clampf(nt, 0, 1) * PI) * 0.95f - 0.12f;
  float az = (-110 + nt * 150) * PI / 180;
  return V3(std::cos(e) * std::cos(az), std::sin(e), std::cos(e) * std::sin(az)).norm();
}
}  // namespace pb
