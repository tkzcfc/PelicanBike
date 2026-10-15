// Shared math / noise / palette helpers (ports of util.js, ImprovedNoise, sky.js palette)
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace pb {
constexpr float PI = 3.14159265358979f, TAU = 6.28318530717959f;
inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstepf(float e0, float e1, float x) { float t = clampf((x - e0) / (e1 - e0), 0.f, 1.f); return t * t * (3 - 2 * t); }
inline float damp(float a, float b, float lambda, float dt) { return lerpf(a, b, 1 - std::exp(-lambda * dt)); }

struct V3 { float x = 0, y = 0, z = 0; V3() = default; V3(float a, float b, float c) : x(a), y(b), z(c) {}
  V3 operator+(const V3& o) const { return {x + o.x, y + o.y, z + o.z}; }
  V3 operator-(const V3& o) const { return {x - o.x, y - o.y, z - o.z}; }
  V3 operator*(float s) const { return {x * s, y * s, z * s}; }
  float dot(const V3& o) const { return x * o.x + y * o.y + z * o.z; }
  V3 cross(const V3& o) const { return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x}; }
  float len() const { return std::sqrt(x * x + y * y + z * z); }
  V3 norm() const { float l = len(); return l > 1e-9f ? *this * (1.f / l) : V3(0, 1, 0); }
};
inline V3 lerp3(const V3& a, const V3& b, float t) { return a + (b - a) * t; }

// mulberry32 (util.js) — deterministic per-chunk placement identical to JS
struct Mulberry { uint32_t a; explicit Mulberry(uint32_t s) : a(s) {}
  float operator()() { a += 0x6D2B79F5u; uint32_t t = a; t = (t ^ (t >> 15)) * (t | 1u); t ^= t + (t ^ (t >> 7)) * (t | 61u); return float(double(t ^ (t >> 14)) / 4294967296.0); }
};

// three.js ImprovedNoise
struct Perlin {
  int p[512];
  Perlin() { static const int b[256] = { 151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23, 190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244, 102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196, 135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42, 223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9, 129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228, 251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107, 49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254, 138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180 }; for (int i = 0; i < 256; i++) p[i] = p[256 + i] = b[i]; }
  static double fade(double t) { return t * t * t * (t * (t * 6 - 15) + 10); }
  static double grad(int h, double x, double y, double z) { h &= 15; double u = h < 8 ? x : y, v = h < 4 ? y : (h == 12 || h == 14) ? x : z; return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v); }
  static double L(double a, double b, double t) { return a + (b - a) * t; }
  double noise(double x, double y, double z) const {
    double fx = std::floor(x), fy = std::floor(y), fz = std::floor(z);
    int X = int(fx) & 255, Y = int(fy) & 255, Z = int(fz) & 255; x -= fx; y -= fy; z -= fz;
    double u = fade(x), v = fade(y), w = fade(z);
    int A = p[X] + Y, AA = p[A] + Z, AB = p[A + 1] + Z, B = p[X + 1] + Y, BA = p[B] + Z, BB = p[B + 1] + Z;
    return L(L(L(grad(p[AA], x, y, z), grad(p[BA], x - 1, y, z), u), L(grad(p[AB], x, y - 1, z), grad(p[BB], x - 1, y - 1, z), u), v),
             L(L(grad(p[AA + 1], x, y, z - 1), grad(p[BA + 1], x - 1, y, z - 1), u), L(grad(p[AB + 1], x, y - 1, z - 1), grad(p[BB + 1], x - 1, y - 1, z - 1), u), v), w);
  }
};
const Perlin& perlin();
float fbm2(double x, double z, int oct, double seed);
float terrainHeight(double x, float z);

struct Col { float r = 0, g = 0, b = 0; };
Col hexLin(uint32_t hex);  // sRGB hex -> linear (THREE.Color)
inline Col lerpc(const Col& a, const Col& b, float t) { return {lerpf(a.r, b.r, t), lerpf(a.g, b.g, t), lerpf(a.b, b.b, t)}; }

struct Palette { Col zen, hor, gnd, sun, hs, hg, fog, ct, cs; float li, hi, exp, bloom; };
void samplePalette(float elevDeg, Palette& out);
V3 sunDirection(float hour);
V3 moonDirection(float hour);
inline float nightFactor(float e) { return smoothstepf(-1.5f, -11.f, e); }

constexpr float SEA_LEVEL = -1.6f, SHORE_Z = -14.6f, CHUNK = 72.f, WHEEL_R = 0.3f;
}  // namespace pb
