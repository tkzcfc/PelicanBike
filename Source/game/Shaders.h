// All GLSL (ES 3.0 / GL 3.3 compatible). One std140 "Frame" UBO shared by every program.
#pragma once
namespace pb::glsl {

static const char* FRAME = R"(
layout(std140) uniform Frame {
  mat4 uViewProj;
  mat4 uShadowMat;
  vec4 uCamPos;      // w: time
  vec4 uSunDir;      // w: night
  vec4 uMoonDir;     // w: sunGlow
  vec4 uZenith;      // w: cloudCover
  vec4 uHorizon;     // w: fog density (scene)
  vec4 uGround;      // w: ocean fog density
  vec4 uSunColor;    // w: dayLight (ocean)
  vec4 uLightDir;    // dir to light, w: intensity
  vec4 uLightColor;  // w: env intensity
  vec4 uHemiSky;     // w: hemi intensity
  vec4 uHemiGround;  // w: wave amp
  vec4 uFogColor;    // w: ocean uOffset (distance mod 36000)
  vec4 uCloudTint;   // w: shadow texel size
  vec4 uCloudShade;  // w: unused
  vec4 uCloudOff;    // xy offset, z: world shift x (base*CHUNK - distance), w: distance mod 200
  vec4 uPhaseA;      // wave phases 0..3
  vec4 uPhaseB;      // x: phase4, y: lamp night, z: bulb emissive, w: unused
  vec4 uAmb[6];
  vec4 uLamp0;     // xyz pos, w intensity (PointLight #ffc27a, distance 14, decay 1.6)
  vec4 uLamp1;
  vec4 uHeadPos;   // headlight world pos, w intensity (SpotLight 28 / 0.5 / 0.55 / 1.4)
  vec4 uHeadDir;   // xyz dir, w: fill light intensity (camera-attached #b9c8ff, 10, 1.2)
  vec4 uFillPos;   // xyz world pos of fill light, w: tail blink
  vec4 uNightE;    // emissive intensities: x lens, y tail, z reflector, w night
  vec4 uCamFwd;    // xyz camera forward, w: point scale (bufH / (2 tan(fov/2)))
};
vec3 ambientCube(vec3 n) { vec3 n2 = n * n; return n2.x * (n.x > 0.0 ? uAmb[0].rgb : uAmb[1].rgb) + n2.y * (n.y > 0.0 ? uAmb[2].rgb : uAmb[3].rgb) + n2.z * (n.z > 0.0 ? uAmb[4].rgb : uAmb[5].rgb); }

)";

static const char* SKYFN = R"(
vec3 skyBase(vec3 dir) {
  float y = dir.y;
  float h = pow(1.0 - clamp(y, 0.0, 1.0), 3.5);
  vec3 col = mix(uZenith.rgb, uHorizon.rgb, h);
  if (y < 0.0) col = mix(uHorizon.rgb, uGround.rgb, smoothstep(0.0, -0.3, y));
  float sd = max(dot(dir, uSunDir.xyz), 0.0);
  float glow = pow(sd, 6.0) * 0.28 + pow(sd, 48.0) * 0.55 + pow(sd, 400.0) * 1.2;
  col += uSunColor.rgb * glow * uMoonDir.w * (0.45 + 0.55 * h);
  float md = max(dot(dir, uMoonDir.xyz), 0.0);
  col += vec3(0.35, 0.45, 0.7) * (pow(md, 24.0) * 0.05 + pow(md, 300.0) * 0.2) * uSunDir.w;
  return col;
}
float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
float hash13(vec3 p3) { p3 = fract(p3 * 0.1031); p3 += dot(p3, p3.zyx + 31.32); return fract((p3.x + p3.y) * p3.z); }
float vnoise(vec2 p) { vec2 i = floor(p); vec2 f = fract(p); vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(hash12(i), hash12(i + vec2(1.0, 0.0)), u.x), mix(hash12(i + vec2(0.0, 1.0)), hash12(i + vec2(1.0, 1.0)), u.x), u.y); }
float fbm(vec2 p) { float s = 0.0; float a = 0.5; for (int i = 0; i < 5; i++) { s += a * vnoise(p); p = p * 2.03 + vec2(1.7, 9.2); a *= 0.5; } return s; }
)";

// ---------------- Sky ----------------
static const char* SKY_VS = R"(
in vec3 aPos;
out vec3 vDir;
void main() { vDir = aPos; vec4 p = uViewProj * vec4(aPos + uCamPos.xyz, 1.0); gl_Position = p.xyww; }
)";
static const char* SKY_FS = R"(
in vec3 vDir; out vec4 oCol;
void main() {
  vec3 dir = normalize(vDir);
  vec3 col = skyBase(dir);
  float uNight = uSunDir.w; float uTime = uCamPos.w;
  if (uNight > 0.01 && dir.y > -0.05) {
    vec3 p = dir * 320.0; vec3 cell = floor(p); float h = hash13(cell);
    if (h > 0.9955) {
      vec3 c = cell + 0.5 + (vec3(hash13(cell + 1.3), hash13(cell + 7.1), hash13(cell + 3.7)) - 0.5) * 0.7;
      float d = length(p - c); float star = smoothstep(0.42, 0.0, d);
      float tw = 0.55 + 0.45 * sin(uTime * (1.5 + h * 7.0) + h * 91.0);
      vec3 sc = mix(vec3(1.0, 0.82, 0.7), vec3(0.75, 0.85, 1.0), hash13(cell + 5.0));
      col += sc * star * tw * uNight * 3.0 * smoothstep(-0.02, 0.2, dir.y);
    }
    vec3 axis = normalize(vec3(0.3, 0.55, 0.78));
    float band = exp(-pow(dot(dir, axis) * 5.0, 2.0));
    float mw = fbm(dir.xz * 9.0 + dir.y * 4.0);
    col += vec3(0.28, 0.3, 0.45) * band * mw * mw * 0.35 * uNight * smoothstep(0.0, 0.3, dir.y);
  }
  float sd = dot(dir, uSunDir.xyz);
  col += uSunColor.rgb * smoothstep(0.99955, 0.99975, sd) * 18.0 * (1.0 - uNight);
  float md = dot(dir, uMoonDir.xyz);
  float moon = smoothstep(0.99925, 0.99945, md);
  if (moon > 0.0) {
    vec3 t1 = normalize(cross(uMoonDir.xyz, vec3(0.0, 1.0, 0.0))); vec3 t2 = cross(t1, uMoonDir.xyz);
    vec2 mp = vec2(dot(dir, t1), dot(dir, t2)) / 0.033;
    float lit = smoothstep(-0.25, 0.35, mp.x + 0.35);
    float craters = 0.82 + 0.18 * vnoise(mp * 6.0);
    col = mix(col, vec3(1.0, 0.97, 0.9) * 2.4 * craters * mix(0.12, 1.0, lit), moon * smoothstep(0.0, 0.5, uNight));
  }
  if (dir.y > 0.0) {
    vec2 uv = dir.xz / (dir.y + 0.1) * 0.55 + uCloudOff.xy;
    float n = fbm(uv * 0.9); float n2 = fbm(uv * 2.3 + 4.0);
    float d = n * 0.75 + n2 * 0.35;
    float cover = smoothstep(1.0 - uZenith.w, 1.0 - uZenith.w + 0.32, d);
    float lightN = fbm(uv * 0.9 - uSunDir.xz * 0.12);
    float lit = clamp(0.55 + (n - lightN) * 3.0, 0.0, 1.0);
    float sunSide = pow(max(dot(dir, uSunDir.xyz), 0.0), 3.0);
    vec3 cc = mix(uCloudShade.rgb, uCloudTint.rgb, lit) + uSunColor.rgb * sunSide * 0.5 * (1.0 - uNight);
    float horizonFade = smoothstep(0.0, 0.14, dir.y);
    col = mix(col, cc, cover * horizonFade * 0.92);
  }
  oCol = vec4(any(isnan(col)) || any(isinf(col)) ? vec3(0.0) : col, 1.0);  // never write NaN/Inf into HDR (blur/bloom spread it)
}
)";

// ---------------- Ocean (GPU Gerstner) ----------------
static const char* OCEAN_VS = R"(
in vec3 aPos;
out vec3 vWorld; out vec3 vNormal; out float vHeight; out vec2 vWP;
const float SHORE = -14.60;
vec3 gerstner(vec2 p, vec2 dir, float steep, float wl, float phase, float amp, inout vec3 tangent, inout vec3 binormal) {
  float k = 6.28318 / wl; vec2 d = normalize(dir); float f = k * dot(d, p) + phase;
  float st = steep * amp; float a = st / k; float sf = sin(f); float cf = cos(f);
  tangent += vec3(-d.x * d.x * st * sf, d.x * st * cf, -d.x * d.y * st * sf);
  binormal += vec3(-d.x * d.y * st * sf, d.y * st * cf, -d.y * d.y * st * sf);
  return vec3(d.x * a * cf, a * sf, d.y * a * cf);
}
void main() {
  vec4 wp = vec4(aPos.x, -1.6, aPos.z, 1.0);
  vec2 p = wp.xz;
  float uAmp = uHemiGround.w;
  float off = smoothstep(SHORE + 1.0, SHORE - 30.0, wp.z);
  float amp = uAmp * mix(0.35, 1.0, off);
  vec3 tangent = vec3(1.0, 0.0, 0.0); vec3 binormal = vec3(0.0, 0.0, 1.0); vec3 disp = vec3(0.0);
  disp += gerstner(p, vec2(0.18, 1.0), 0.2, 21.0, uPhaseA.x, amp, tangent, binormal);
  disp += gerstner(p, vec2(-0.45, 1.0), 0.17, 12.5, uPhaseA.y, amp, tangent, binormal);
  disp += gerstner(p, vec2(0.75, 0.62), 0.13, 7.0, uPhaseA.z, amp, tangent, binormal);
  disp += gerstner(p, vec2(-0.9, 0.45), 0.1, 4.2, uPhaseA.w, amp, tangent, binormal);
  disp += gerstner(p, vec2(0.3, 1.0), 0.07, 2.6, uPhaseB.x, amp, tangent, binormal);
  wp.xyz += disp;
  vNormal = normalize(cross(binormal, tangent));
  vWorld = wp.xyz;
  vHeight = disp.y / max(uAmp, 0.05);
  vWP = vec2(wp.x + uFogColor.w, wp.z);
  gl_Position = uViewProj * wp;
}
)";
static const char* OCEAN_FS = R"(
in vec3 vWorld; in vec3 vNormal; in float vHeight; in vec2 vWP; out vec4 oCol;
const float SHORE = -14.60;
const vec3 uDeep = vec3(0.0036, 0.0423, 0.1022);
const vec3 uShallow = vec3(0.0137, 0.3654, 0.3916);
const vec3 uLampColor = vec3(1.0, 0.4564, 0.1441);
void main() {
  float uTime = uCamPos.w; float uNight = uSunDir.w; float uDayLight = uSunColor.w;
  vec3 cameraPosition = uCamPos.xyz;
  vec3 N = normalize(vNormal);
  float dist = length(vWorld - cameraPosition);
  vec2 q = vWP * 0.55; float e = 0.35;
  vec2 f1 = vec2(0.35, 0.6) * uTime; vec2 f2 = vec2(-0.5, 0.25) * uTime;
  float h0 = vnoise(q + f1) + 0.5 * vnoise(q * 2.7 + f2);
  float hx = vnoise(q + vec2(e, 0.0) + f1) + 0.5 * vnoise((q + vec2(e, 0.0)) * 2.7 + f2);
  float hz = vnoise(q + vec2(0.0, e) + f1) + 0.5 * vnoise((q + vec2(0.0, e)) * 2.7 + f2);
  float detail = 0.55 * (1.0 - smoothstep(20.0, 160.0, dist));
  N = normalize(N + vec3(h0 - hx, 0.0, h0 - hz) * detail);
  vec3 V = normalize(cameraPosition - vWorld);
  float ndv = max(dot(N, V), 0.0);
  float fres = 0.02 + 0.98 * pow(clamp(1.0 - ndv, 0.0, 1.0), 5.0);
  vec3 R = reflect(-V, N); R.y = abs(R.y) + 0.02; R = normalize(R);
  vec3 refl = skyBase(R);
  float depthF = smoothstep(SHORE - 2.0, SHORE - 45.0, vWorld.z);
  vec3 water = mix(uShallow, uDeep, depthF) * (0.12 + 0.88 * uDayLight);
  float sss = pow(max(dot(V, -uSunDir.xyz), 0.0), 3.0) * clamp(vHeight + 0.3, 0.0, 1.5);
  water += uShallow * uSunColor.rgb * sss * 0.35 * (1.0 - uNight);
  vec3 col = mix(water, refl, fres);
  float rs = max(dot(R, uSunDir.xyz), 0.0);
  col += uSunColor.rgb * (pow(rs, 420.0) * 30.0 + pow(rs, 60.0) * 0.7) * (1.0 - uNight) * step(-0.05, uSunDir.y);
  float rm = max(dot(R, uMoonDir.xyz), 0.0);
  col += vec3(0.75, 0.82, 1.0) * (pow(rm, 250.0) * 2.2 + pow(rm, 30.0) * 0.06) * uNight;
  float fn = vnoise(vWP * 1.3 + uTime * 0.4) * 0.6 + vnoise(vWP * 4.0 - uTime * 0.3) * 0.4;
  float crest = smoothstep(0.62, 1.05, vHeight + fn * 0.35) * smoothstep(-8.0, -40.0, vWorld.z);
  float shoreLine = SHORE + 0.9 * sin(vWP.x * 0.045) + 0.5 * sin(vWP.x * 0.13 + 1.7);
  float surge = sin(uTime * 0.7 + vWP.x * 0.021) * 1.3;
  float sd = vWorld.z - (shoreLine + surge * 0.5);
  float shoreFoam = smoothstep(-5.5, 0.5, sd) * (0.55 + 0.45 * fn);
  shoreFoam *= smoothstep(0.35, 0.55, fn + smoothstep(-2.5, 0.8, sd) * 0.5);
  float foam = clamp(max(crest * 0.75, shoreFoam), 0.0, 1.0);
  vec3 foamCol = mix(vec3(0.06, 0.08, 0.12), vec3(0.95, 0.97, 1.0), uDayLight) * (0.6 + 0.4 * uMoonDir.w);
  col = mix(col, foamCol, foam * 0.85);
  float lampRow = pow(max(0.0, 1.0 - abs(fract((vWP.x - 12.0) / 36.0 + 0.5) - 0.5) * 7.0), 2.0);
  col += uLampColor * lampRow * smoothstep(-40.0, -14.0, vWorld.z) * uNight * 0.25 * (0.5 + 0.5 * fn);
  float fogF = 1.0 - exp(-pow(dist * uGround.w, 1.35));
  vec3 fd = vWorld - cameraPosition;
  vec3 fogCol = skyBase(normalize(vec3(fd.x, max(fd.y * 0.02, 0.0) + 0.004, fd.z)));
  col = mix(col, fogCol, clamp(fogF, 0.0, 1.0));
  oCol = vec4(any(isnan(col)) || any(isinf(col)) ? vec3(0.0) : col, 1.0);  // never write NaN/Inf into HDR (blur/bloom spread it)
}
)";

// ---------------- Lit (standard-ish material) ----------------
// MODE: 0 = static mesh with uModel, 1 = instanced props (sway), 2 = rider frame lerp, 3 = terrain, 4 = road
static const char* LIT_VS = R"(
in vec3 aPos; in vec4 aNrm; in vec4 aCol; in vec4 aMat; in vec3 aPos2; in vec4 aNrm2;
in vec4 aI0; in vec4 aI1; in vec4 aI2;
uniform mat4 uModel; uniform float uLerp; uniform vec2 uSway;
#if MODE == 2
uniform vec4 uBones[144];
#endif
out vec3 vW; out vec3 vN; out vec4 vCol; out vec4 vMat; out vec4 vSh; out vec2 vUV; out vec3 vLP;
#if MODE == 1
out vec3 vI2;
#endif
void main() {
  vec3 p = aPos; vec3 n = aNrm.xyz; vLP = aPos;
#if MODE == 2
  { int b = int(aMat.w * 255.0 + 0.5) * 3; vec4 r0 = uBones[b], r1 = uBones[b + 1], r2 = uBones[b + 2]; vec4 q = vec4(p, 1.0);
    p = vec3(dot(r0, q), dot(r1, q), dot(r2, q)); n = vec3(dot(r0.xyz, n), dot(r1.xyz, n), dot(r2.xyz, n)); }
#endif
  vec4 wp;
#if MODE == 1
  // aI0: chunk-relative pos (xyz) + scale; aI1: rotY, yScale, -, -; aI2: tint rgb, local x (sway phase)
  vec3 ip = aI0.xyz + vec3(uCloudOff.z, 0.0, 0.0);
  float sw = pow(max(p.y - uSway.y, 0.0), 1.4);
  vec3 q = vec3(p.x * aI0.w, p.y * aI0.w * aI1.y, p.z * aI0.w);
  float c = cos(aI1.x), s = sin(aI1.x);
  q = vec3(c * q.x + s * q.z, q.y, -s * q.x + c * q.z);
  n = vec3(c * n.x + s * n.z, n.y, -s * n.x + c * n.z);
  // sway in instance-local (pre-rotation in JS 'transformed'): approximate post-rotation, same amplitude
  vec3 swv = vec3(sin(uCamPos.w * 1.3 + aI2.w * 0.31 + aI0.z * 0.17) * sw * uSway.x, 0.0, cos(uCamPos.w * 1.1 + aI2.w * 0.23) * sw * uSway.x * 0.6);
  q += vec3(c * swv.x + s * swv.z, 0.0, -s * swv.x + c * swv.z) * aI0.w;
  wp = vec4(q + ip, 1.0);
  vI2 = aI2.rgb;
#elif MODE == 6
  { vec4 q = vec4(p, 1.0); wp = vec4(dot(aI0, q), dot(aI1, q), dot(aI2, q), 1.0); n = vec3(dot(aI0.xyz, n), dot(aI1.xyz, n), dot(aI2.xyz, n)); }
#else
  wp = uModel * vec4(p, 1.0);
  n = mat3(uModel) * n;
#endif
  vW = wp.xyz; vN = n; vCol = aCol; vMat = aMat;
  vMat.w = aNrm.w * 127.0;  // flags byte stored in normal.w (snorm)
  vSh = uShadowMat * vec4(wp.xyz + normalize(n) * 0.025, 1.0);
#if MODE == 3
  
  vUV = vec2(p.x / 9.0, p.z / 9.0);
#elif MODE == 4
  vUV = vec2(p.x / 24.0, (p.z + 2.82) / 5.64);
#elif MODE == 5
  vUV = vec2((p.z + 0.55) / 1.1, 1.0 - (p.y + 0.55) / 1.1);
#else
  vUV = vec2(0.0);
#endif
  gl_Position = uViewProj * wp;
}
)";
static const char* LIT_FS = R"(
in vec3 vW; in vec3 vN; in vec4 vCol; in vec4 vMat; in vec4 vSh; in vec2 vUV; in vec3 vLP;
#if MODE == 1
in vec3 vI2;
#endif
uniform sampler2DShadow uShadow; uniform sampler2D uTex;
uniform vec4 uTint;  // rgb tint (linear), w: emissive scale
uniform int uDbg; uniform float uDfg;
out vec4 oCol;
vec3 s2l(vec3 c) { return mix(c * 0.0773993808, pow(c * 0.9478672986 + 0.0521327014, vec3(2.4)), step(0.04045, c)); }
vec3 brdf(vec3 N, vec3 V, vec3 L, vec3 diffC, vec3 F0, float a) {
  float ndl = max(dot(N, L), 0.0); if (ndl <= 0.0) return vec3(0.0);
  vec3 H = (L + V) * inversesqrt(max(dot(L + V, L + V), 1e-12)); float ndh = max(dot(N, H), 0.0); float ndv = max(dot(N, V), 1e-3);
  float a2 = a * a; float d = ndh * ndh * (a2 - 1.0) + 1.0; float D = a2 / (3.14159 * d * d);
  float G = 0.5 / max(ndl * sqrt(ndv * ndv * (1.0 - a2) + a2) + ndv * sqrt(ndl * ndl * (1.0 - a2) + a2), 1e-6);  // three V_GGX_SmithCorrelated
  vec3 F = F0 + (1.0 - F0) * pow(1.0 - max(dot(H, V), 0.0), 5.0);
  return (diffC / 3.14159 + F * D * G) * ndl;
}
float distAtt(float d, float cutoff, float decay) { float f = 1.0 / max(pow(d, decay), 0.01); float r = clamp(1.0 - pow(d / cutoff, 4.0), 0.0, 1.0); return f * r * r; }
float shadowPCF(vec4 sc) {
  vec3 p = sc.xyz / sc.w;
  if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0 || p.z > 1.0) return 1.0;
  float t = uCloudTint.w; float s = 0.0;
  s += texture(uShadow, vec3(p.xy + vec2(-t, -t), p.z));
  s += texture(uShadow, vec3(p.xy + vec2( t, -t), p.z));
  s += texture(uShadow, vec3(p.xy + vec2(-t,  t), p.z));
  s += texture(uShadow, vec3(p.xy + vec2( t,  t), p.z));
  s += texture(uShadow, p) * 2.0;
  return s / 6.0;
}
void main() {
  vec3 albedo = s2l(vCol.rgb) * uTint.rgb;
#if MODE == 1
  albedo *= vI2;
#endif
#if MODE == 3
  albedo *= texture(uTex, vUV).r;
#elif MODE == 4
  albedo = texture(uTex, vUV).rgb;  // SRGB8 texture: hardware-decoded
#elif MODE == 5
  vec4 tx = texture(uTex, vUV); if (tx.a < 0.4) discard; albedo = tx.rgb;
#endif
  float rough = max(vMat.r, 0.04); float metal = vMat.g; float emis = vMat.b * 8.0 * abs(uTint.w);
  vec3 emisC = uTint.w < 0.0 ? vec3(1.0, 0.539, 0.195) / max(albedo, vec3(0.05)) : vec3(1.0);  // w<0: lamp bulbs, emissive #ffc27a != albedo #fff4dc
  int flags = int(vMat.w + 0.5);
#if MODE == 2
  if ((flags & 4) != 0) {  // wicker basket (makeWickerTexture, repeat 6 x 1.2 on the basket)
    vec2 wc = vec2(atan(vLP.z, vLP.x - 0.0) * 7.64, vLP.y * 64.0); vec2 ci = floor(wc); vec2 cf = fract(wc);
    bool horiz = mod(ci.x + ci.y, 2.0) < 0.5; float g = horiz ? cf.y : cf.x;
    vec3 c0 = vec3(0.195, 0.078, 0.023), c1 = vec3(0.693, 0.386, 0.135), c2 = vec3(0.156, 0.060, 0.018);
    vec3 wcol = g < 0.5 ? mix(c0, c1, g * 2.0) : mix(c1, c2, g * 2.0 - 1.0);
    bool border = min(min(cf.x, cf.y), min(1.0 - cf.x, 1.0 - cf.y)) < 0.0625;
    albedo = border ? vec3(0.147, 0.057, 0.018) : wcol;
  }
  if ((flags & 8) != 0) emis = vMat.b * uNightE.x;
  if ((flags & 16) != 0) emis = vMat.b * uNightE.y;
  if ((flags & 32) != 0) emis = vMat.b * uNightE.z;
#endif
  vec3 N = normalize(vN);
  if ((flags & 2) != 0) N = normalize(cross(dFdx(vW), dFdy(vW)));
  if (!gl_FrontFacing) N = -N;
#if MODE == 4
  {  // three bumpMap (bumpScale 1.2) via perturbNormalArb; height ~ asphalt grain luminance (JS bump grains share the map's positions)
    vec2 dx = dFdx(vUV), dy = dFdy(vUV);
    float h0 = sqrt(texture(uTex, vUV).g) * 1.8, hx = sqrt(texture(uTex, vUV + dx).g) * 1.8, hy = sqrt(texture(uTex, vUV + dy).g) * 1.8;
    vec2 dH = vec2(hx - h0, hy - h0) * 1.2;
    vec3 sx = normalize(dFdx(vW)), sy = normalize(dFdy(vW)); vec3 R1 = cross(sy, N), R2 = cross(N, sx);
    float det = dot(sx, R1); N = normalize(abs(det) * N - sign(det) * (dH.x * R1 + dH.y * R2));
  }
#endif
  vec3 V = normalize(uCamPos.xyz - vW);
  vec3 L = uLightDir.xyz;
  float ndl = max(dot(N, L), 0.0);
  float sh = ndl > 0.0 ? shadowPCF(vSh) : 0.0;
  vec3 diffC = albedo * (1.0 - metal);
  vec3 F0 = mix(vec3(0.04), albedo, metal);
  vec3 H = (L + V) * inversesqrt(max(dot(L + V, L + V), 1e-12));
  float ndh = max(dot(N, H), 0.0); float ndv = max(dot(N, V), 1e-3);
  float a = rough * rough; float a2 = a * a;
  float d = ndh * ndh * (a2 - 1.0) + 1.0; float D = a2 / (3.14159 * d * d);
  float G = 0.5 / max(ndl * sqrt(ndv * ndv * (1.0 - a2) + a2) + ndv * sqrt(ndl * ndl * (1.0 - a2) + a2), 1e-6);  // three V_GGX_SmithCorrelated
  vec3 F = F0 + (1.0 - F0) * pow(1.0 - max(dot(H, V), 0.0), 5.0);
  vec3 lightC = uLightColor.rgb * uLightDir.w;
  vec3 col = (diffC / 3.14159 + F * D * G) * lightC * ndl * sh;
  if (uNightE.w > 0.01) {  // night: two street lamps, bike headlight (spot), camera fill
    vec3 lampC = vec3(1.0, 0.539, 0.196);
    vec3 d0 = uLamp0.xyz - vW; float l0 = length(d0); l0 = max(l0, 1e-3); col += brdf(N, V, d0 / l0, diffC, F0, a) * lampC * uLamp0.w * distAtt(l0, 14.0, 1.6);
    vec3 d1 = uLamp1.xyz - vW; float l1 = length(d1); l1 = max(l1, 1e-3); col += brdf(N, V, d1 / l1, diffC, F0, a) * lampC * uLamp1.w * distAtt(l1, 14.0, 1.6);
    vec3 dh = uHeadPos.xyz - vW; float lh = length(dh); lh = max(lh, 1e-3); vec3 Lh = dh / lh;
    float sc = smoothstep(cos(0.5), cos(0.5 * (1.0 - 0.55)), dot(-Lh, uHeadDir.xyz));
    col += brdf(N, V, Lh, diffC, F0, a) * vec3(1.0, 0.879, 0.631) * uHeadPos.w * distAtt(lh, 28.0, 1.4) * sc;
    vec3 df = uFillPos.xyz - vW; float lf = length(df); lf = max(lf, 1e-3); col += brdf(N, V, df / lf, diffC, F0, a) * vec3(0.485, 0.578, 1.0) * uHeadDir.w * distAtt(lf, 10.0, 1.2);
  }
  // hemisphere + analytic sky IBL (replaces PMREM env map)
  vec3 hemi = mix(uHemiGround.rgb, uHemiSky.rgb, N.y * 0.5 + 0.5) * uHemiSky.w;
  vec3 envD = ambientCube(N);
  float envI = uLightColor.w;
  // AMB_TINT: per-channel calibration of our hemi + ambient-cube against three's PMREM IBL (tools/js_hdr_split.mjs + amb_fit.py; constant across 8h/12h/17.35h)
  // up-facing (ground) fit vs side-facing (rider, 8h cine fit): PMREM lobe differs by direction
  vec3 AMB_TINT = mix(vec3(uAmb[3].w, uAmb[4].w, uAmb[5].w), vec3(uAmb[0].w, uAmb[1].w, uAmb[2].w), clamp(N.y, 0.0, 1.0));
  vec3 irr = (hemi / 3.14159 + envD * envI) * AMB_TINT;
  col += diffC * irr;  // (multiscatter energy split applied below when uDfg)
  vec3 R = reflect(-V, N);
  vec3 envS = mix(skyBase(R), envD, clamp(rough * 1.2, 0.0, 1.0));
  vec3 Fe = F0 + (max(vec3(1.0 - rough), F0) - F0) * pow(1.0 - ndv, 5.0);
  // three.js DFGApprox (Karis mobile env BRDF) used by RE_IndirectSpecular_Physical
  vec4 dr = rough * vec4(-1.0, -0.0275, -0.572, 0.022) + vec4(1.0, 0.0425, 1.04, -0.04);
  float a004 = min(dr.x * dr.x, exp2(-9.28 * ndv)) * dr.x + dr.y; vec2 fab = vec2(-1.04, 1.04) * a004 + dr.zw;
  vec3 envBrdf = uDfg > 0.5 ? F0 * fab.x + fab.y : Fe * (1.0 - rough * 0.6);
  if (uDfg > 0.5) { vec3 FssEss = envBrdf; float Ess = fab.x + fab.y; float Ems = 1.0 - Ess; vec3 Favg = F0 + (1.0 - F0) * 0.047619;
    vec3 Fms = FssEss * Favg / (1.0 - Ems * Favg); vec3 ms = Fms * Ems; col += irr * ms - diffC * irr * (FssEss + ms); }
  col += envS * envBrdf * envI * AMB_TINT;
  if ((flags & 64) != 0) {  // MeshPhysicalMaterial clearcoat 0.6 / clearcoatRoughness 0.25 (beak): F0 .04 GGX layer, base attenuated by 1 - cc*Fcc
    const float cc = 0.6, cr = 0.25; float ca = cr * cr, ca2 = ca * ca;
    float Fcc = 0.04 + 0.96 * pow(1.0 - ndv, 5.0);
    float dc = ndh * ndh * (ca2 - 1.0) + 1.0; float Dc = ca2 / (3.14159 * dc * dc);
    float Gc = 0.5 / max(ndl * sqrt(ndv * ndv * (1.0 - ca2) + ca2) + ndv * sqrt(ndl * ndl * (1.0 - ca2) + ca2), 1e-6);
    float Fh = 0.04 + 0.96 * pow(1.0 - max(dot(H, V), 0.0), 5.0);
    vec3 Rc = reflect(-V, N); vec3 envC = skyBase(Rc);
    vec4 cdr = cr * vec4(-1.0, -0.0275, -0.572, 0.022) + vec4(1.0, 0.0425, 1.04, -0.04);
    float ca004 = min(cdr.x * cdr.x, exp2(-9.28 * ndv)) * cdr.x + cdr.y; vec2 cfab = vec2(-1.04, 1.04) * ca004 + cdr.zw;
    col = col * (1.0 - cc * Fcc) + cc * (Fh * Dc * Gc * lightC * ndl * sh + envC * (0.04 * cfab.x + cfab.y) * envI * AMB_TINT);
  }
  col += albedo * emisC * emis;
  if (uDbg == 1) col = albedo;
  if (uDbg == 2) col = (diffC / 3.14159 + F * D * G) * lightC * ndl * sh;
  if (uDbg == 7) col = vec3(clamp(N.y, 0.0, 1.0));
  if (uDbg == 6) col = (diffC / 3.14159 + F * D * G) * lightC * ndl;
  if (uDbg == 3) col = diffC * (hemi / 3.14159 + envD * envI) * AMB_TINT;
  if (uDbg == 4) col = envS * envBrdf * envI * AMB_TINT;
  if (uDbg == 5) col = N * 0.5 + 0.5;
  float dist = length(vW - uCamPos.xyz);
  float fd = uHorizon.w * dist;
  float fog = 1.0 - exp(-fd * fd);
  col = mix(col, uFogColor.rgb, fog);
  oCol = vec4(any(isnan(col)) || any(isinf(col)) ? vec3(0.0) : col, 1.0);  // never write NaN/Inf into HDR (blur/bloom spread it)
}
)";

// ---------------- additive FX (three AdditiveBlending: SRC_ALPHA, ONE) ----------------
static const char* BEAM_VS = R"(
in vec3 aPos; in vec4 aNrm; in vec4 aCol; uniform mat4 uModel;
out float vY; out vec3 vN; out vec3 vV;
void main() { vY = aCol.r; vec4 wp = uModel * vec4(aPos, 1.0); vN = normalize(mat3(uModel) * aNrm.xyz); vV = normalize(uCamPos.xyz - wp.xyz); gl_Position = uViewProj * wp; }
)";
static const char* BEAM_FS = R"(
uniform float uNight; in float vY; in vec3 vN; in vec3 vV; out vec4 oCol;
void main() { float edge = pow(abs(dot(normalize(vN + 1e-6), normalize(vV + 1e-6))), 1.5); float a = pow(clamp(vY, 0.0, 1.0), 2.6) * edge * uNight * 0.32; oCol = vec4(vec3(1.0, 0.86, 0.55) * a, a); }
)";
static const char* FF_VS = R"(
in vec3 aPos; in vec4 aCol; uniform float uOffset; out float vA;
void main() { float seed = aCol.r * 100.0; vec3 p = aPos; float t = uCamPos.w; p.x = mod(p.x - uOffset + 60.0, 200.0) - 60.0;
  p.x += sin(t * 0.7 + seed) * 0.6; p.y += sin(t * 0.9 + seed * 1.3) * 0.35; p.z += cos(t * 0.6 + seed * 0.7) * 0.6;
  gl_Position = uViewProj * vec4(p, 1.0); float z = max(dot(p - uCamPos.xyz, uCamFwd.xyz), 0.05);
  vA = pow(0.5 + 0.5 * sin(t * 2.2 + seed * 3.0), 3.0); gl_PointSize = 0.12 * uCamFwd.w / z; }
)";
static const char* FF_FS = R"(
uniform float uNight; in float vA; out vec4 oCol;
void main() { float d = length(gl_PointCoord - 0.5); float a = smoothstep(0.5, 0.0, d); oCol = vec4(vec3(0.75, 1.0, 0.35) * 2.5, a * vA * uNight); }
)";
static const char* POOL_VS = R"(
in vec3 aPos; in vec4 aCol; uniform mat4 uModel; out vec2 vUv;
void main() { vUv = aCol.rg * 2.0 - 1.0; gl_Position = uViewProj * (uModel * vec4(aPos, 1.0)); }
)";
static const char* POOL_FS = R"(
uniform float uOpacity; in vec2 vUv; out vec4 oCol;
void main() { float r = length(vUv); if (r > 1.0) discard; float a = r < 0.25 ? mix(1.0, 0.8, r / 0.25) : mix(0.8, 0.0, (r - 0.25) / 0.75);
  oCol = vec4(vec3(1.0, 0.456, 0.136), a * uOpacity); }
)";

// depth-only shadow pass
static const char* SHADOW_FS = R"(
out vec4 oCol; void main() { oCol = vec4(1.0); }
)";

// ---------------- Post ----------------
static const char* FS_VS = R"(
out vec2 vUv;
void main() { vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2); vUv = p; gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0); }
)";
// sanitize + luminosity high pass (UnrealBloomPass LuminosityHighPassShader, smoothWidth 0.01)
static const char* BRIGHT_FS = R"(
in vec2 vUv; uniform sampler2D uSrc; uniform float uThreshold; out vec4 oCol;
float fixf(float x) { uint b = floatBitsToUint(x); if ((b & 0x7f800000u) == 0x7f800000u) return ((b & 0x007fffffu) == 0u && (b & 0x80000000u) == 0u) ? 48.0 : 0.0; return clamp(x, 0.0, 48.0); }
void main() {
  vec3 c = texture(uSrc, vUv).rgb; c = vec3(fixf(c.r), fixf(c.g), fixf(c.b));
  float v = dot(c, vec3(0.2126, 0.7152, 0.0722));  // three luminance() (Rec.709 working space)
  float alpha = smoothstep(uThreshold, uThreshold + 0.01, v);
  oCol = vec4(c * alpha, 1.0);
}
)";
static const char* BLUR_FS = R"(
in vec2 vUv; uniform sampler2D uSrc; uniform vec2 uDir; uniform int uPairs; uniform float uCW; uniform float uOff[11]; uniform float uW[11]; out vec4 oCol;
float fixf(float x) { uint b = floatBitsToUint(x); if ((b & 0x7f800000u) == 0x7f800000u) return ((b & 0x007fffffu) == 0u && (b & 0x80000000u) == 0u) ? 48.0 : 0.0; return clamp(x, 0.0, 48.0); }
vec3 tap(vec2 uv) { vec3 c = texture(uSrc, uv).rgb; return vec3(fixf(c.r), fixf(c.g), fixf(c.b)); }  // raw HDR may hold NaN/Inf (backdrop pass)
void main() {
  vec3 s = tap(vUv) * uCW;
  for (int i = 0; i < 11; i++) { if (i >= uPairs) break; vec2 o = uDir * uOff[i];
    s += (tap(vUv + o) + tap(vUv - o)) * uW[i]; }
  oCol = vec4(s, 1.0);
}
)";
static const char* FINAL_FS = R"(
in vec2 vUv; uniform sampler2D uHdr; uniform sampler2D uB0; uniform sampler2D uB1; uniform sampler2D uB2; uniform sampler2D uB3; uniform sampler2D uB4;
uniform vec4 uBloomF;  // factors 0..3 (pre-multiplied by strength)
uniform vec4 uGrade;   // x factor4, y vignette, z warm, w grain
uniform vec2 uExpTime; // exposure, time
uniform int uNR; uniform vec4 uRects[24]; uniform vec4 uRectP[24]; uniform sampler2D uBlur;  // backdrop-filter rects (fb px), P: x radius, y amount
out vec4 oCol;
float fixf(float x) { uint b = floatBitsToUint(x); if ((b & 0x7f800000u) == 0x7f800000u) return ((b & 0x007fffffu) == 0u && (b & 0x80000000u) == 0u) ? 48.0 : 0.0; return clamp(x, 0.0, 48.0); }
float h(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
vec3 RRTAndODTFit(vec3 v) { vec3 a = v * (v + 0.0245786) - 0.000090537; vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081; return a / b; }
vec3 aces(vec3 color) {
  const mat3 IM = mat3(vec3(0.59719, 0.07600, 0.02840), vec3(0.35458, 0.90834, 0.13383), vec3(0.04823, 0.01566, 0.83777));
  const mat3 OM = mat3(vec3(1.60475, -0.10208, -0.00327), vec3(-0.53108, 1.10813, -0.07276), vec3(-0.07367, -0.00605, 1.07602));
  color *= uExpTime.x / 0.6; color = IM * color; color = RRTAndODTFit(color); color = OM * color; return clamp(color, 0.0, 1.0);
}
void main() {
  vec3 c = texture(uHdr, vUv).rgb; c = vec3(fixf(c.r), fixf(c.g), fixf(c.b));
  float shade = 0.0;
  if (uNR > 0) {
    float m = 0.0; vec2 p = gl_FragCoord.xy; shade = 0.0;
    for (int i = 0; i < 24; i++) { if (i >= uNR) break; vec4 r = uRects[i]; float rad = uRectP[i].x;
      vec2 hc = (r.zw - r.xy) * 0.5; vec2 q = abs(p - (r.xy + hc)) - hc + rad; float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - rad;
      m = max(m, uRectP[i].y * clamp(0.5 - d, 0.0, 1.0));
      // box-shadow: 0 10px 40px rgba(0,0,0,.35) — outside the border box only
      float k = uRectP[i].z; vec2 q2 = abs(p + vec2(0.0, 10.0 * k) - (r.xy + hc)) - hc + rad; float ds = length(max(q2, 0.0)) + min(max(q2.x, q2.y), 0.0) - rad;
      shade = max(shade, 0.35 * uRectP[i].y * (1.0 - smoothstep(-40.0 * k, 40.0 * k, ds)) * clamp(d + 0.5, 0.0, 1.0)); }
    if (m > 0.0) { vec3 bl = texture(uBlur, vUv).rgb; bl = vec3(fixf(bl.r), fixf(bl.g), fixf(bl.b)); float l = dot(bl, vec3(0.2126, 0.7152, 0.0722)); bl = max(mix(vec3(l), bl, 1.3), 0.0);  // blur(14px) saturate(1.3)
      c = mix(c, bl, min(m, 1.0)); }
  }
  vec3 b = texture(uB0, vUv).rgb * uBloomF.x + texture(uB1, vUv).rgb * uBloomF.y + texture(uB2, vUv).rgb * uBloomF.z
         + texture(uB3, vUv).rgb * uBloomF.w + texture(uB4, vUv).rgb * uGrade.x;
  vec3 col = c + b;
  vec2 cc = vUv - 0.5; float r = length(cc);
  col *= mix(1.0, smoothstep(0.85, 0.2, r), uGrade.y);
  col *= mix(vec3(1.0), vec3(1.06, 1.0, 0.92), uGrade.z);
  col *= 1.0 + (h(vUv * 1000.0 + fract(uExpTime.y)) - 0.5) * uGrade.w * 2.0;
  col = aces(col);
  col = mix(pow(col, vec3(1.0 / 2.4)) * 1.055 - 0.055, col * 12.92, step(col, vec3(0.0031308)));
  col *= 1.0 - shade;
  oCol = vec4(col, 1.0);
}
)";
}  // namespace pb::glsl
