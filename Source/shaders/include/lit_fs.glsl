#include "pb_fs_head.glsl"
layout(binding = 0) uniform sampler2D uTex;
layout(binding = 1) uniform sampler2D uShadow;  // shadow map: window depth packed into RGB8 (see shadow.frag), nearest
layout(std140) uniform fs_ub {
  vec4 fF[38];
  vec4 uTint;  // rgb tint (linear), w: emissive scale
  vec4 uOpt;   // x: debug view, y: DFG multiscatter, z: bit0 decode uTex sRGB in shader, bit1 aniso emulation, w: shadow map size
};
#include "pb_frame.glsl"
#include "pb_sky.glsl"
layout(location = 0) in vec3 vW; layout(location = 1) in vec3 vN; layout(location = 2) in vec4 vCol; layout(location = 3) in vec4 vMat;
layout(location = 4) in vec4 vSh; layout(location = 5) in vec2 vUV; layout(location = 6) in vec3 vLP;
#if MODE == 1
layout(location = 7) in vec3 vI2;
#endif
layout(location = 0) out vec4 oCol;
vec3 s2l(vec3 c) { return mix(c * 0.0773993808, pow(c * 0.9478672986 + 0.0521327014, vec3(2.4)), step(0.04045, c)); }
// uOpt.z bit 0: sRGB decode in the shader (ax::backend has no sRGB format); bit 1: anisotropic filtering emulation
// (ax::backend has no anisotropy API): up to 8 trilinear taps along the footprint's major axis, like max aniso 8.
vec4 texA(vec2 uv) {
  int fl = int(uOpt.z + 0.5); bool srgb = (fl & 1) != 0;
  vec2 dx = dFdx(uv), dy = dFdy(uv);
  if ((fl & 2) == 0) { vec4 t = texture(uTex, uv); if (srgb) t.rgb = s2l(t.rgb); return t; }
  vec2 sz = vec2(textureSize(uTex, 0)); float lx = length(dx * sz), ly = length(dy * sz);
  bool xMajor = lx > ly; float n = clamp(ceil(max(lx, ly) / max(min(lx, ly), 1e-6)), 1.0, 8.0);
  vec2 major = xMajor ? dx : dy; vec2 gx = xMajor ? dx / n : dx, gy = xMajor ? dy : dy / n;
  int N = int(n); vec4 t = vec4(0.0);
  for (int i = 0; i < 8; i++) { if (i >= N) break;
    vec4 s = textureGrad(uTex, uv + major * ((float(i) + 0.5) / n - 0.5), gx, gy); if (srgb) s.rgb = s2l(s.rgb); t += s; }
  return t / n;
}
vec3 brdf(vec3 N, vec3 V, vec3 L, vec3 diffC, vec3 F0, float a) {
  float ndl = max(dot(N, L), 0.0); if (ndl <= 0.0) return vec3(0.0);
  vec3 H = (L + V) * inversesqrt(max(dot(L + V, L + V), 1e-12)); float ndh = max(dot(N, H), 0.0); float ndv = max(dot(N, V), 1e-3);
  float a2 = a * a; float d = ndh * ndh * (a2 - 1.0) + 1.0; float D = a2 / (3.14159 * d * d);
  float G = 0.5 / max(ndl * sqrt(ndv * ndv * (1.0 - a2) + a2) + ndv * sqrt(ndl * ndl * (1.0 - a2) + a2), 1e-6);  // three V_GGX_SmithCorrelated
  vec3 F = F0 + (1.0 - F0) * pow(1.0 - max(dot(H, V), 0.0), 5.0);
  return (diffC / 3.14159 + F * D * G) * ndl;
}
float distAtt(float d, float cutoff, float decay) { float f = 1.0 / max(pow(d, decay), 0.01); float r = clamp(1.0 - pow(d / cutoff, 4.0), 0.0, 1.0); return f * r * r; }
// sampler2DShadow + GL_LINEAR emulation: 2x2 depth compares (LEQUAL) blended with the bilinear weights
float shDepth(ivec2 c, int sz) { c = clamp(c, ivec2(0), ivec2(sz - 1)); vec3 t = texelFetch(uShadow, c, 0).rgb; return dot(floor(t * 255.0 + 0.5), vec3(65536.0, 256.0, 1.0)) / 16777215.0; }
float shTap(vec2 uv, float z) {
#ifdef METAL
  uv.y = 1.0 - uv.y;  // render-target textures are stored top-down on Metal
#endif
  float S = uOpt.w; int sz = int(S + 0.5);
  vec2 st = uv * S - 0.5; vec2 i0 = floor(st); vec2 f = st - i0; ivec2 c = ivec2(i0);
  float r = clamp(z, 0.0, 1.0);
  float s00 = step(r, shDepth(c, sz)), s10 = step(r, shDepth(c + ivec2(1, 0), sz));
  float s01 = step(r, shDepth(c + ivec2(0, 1), sz)), s11 = step(r, shDepth(c + ivec2(1, 1), sz));
  return mix(mix(s00, s10, f.x), mix(s01, s11, f.x), f.y);
}
float shadowPCF(vec4 sc) {
  vec3 p = sc.xyz / sc.w;
  if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0 || p.z > 1.0) return 1.0;
  float t = uCloudTint.w; float s = 0.0;
  s += shTap(p.xy + vec2(-t, -t), p.z);
  s += shTap(p.xy + vec2( t, -t), p.z);
  s += shTap(p.xy + vec2(-t,  t), p.z);
  s += shTap(p.xy + vec2( t,  t), p.z);
  s += shTap(p.xy, p.z) * 2.0;
  return s / 6.0;
}
void main() {
  int uDbg = int(uOpt.x + 0.5); float uDfg = uOpt.y;
  vec3 albedo = s2l(vCol.rgb) * uTint.rgb;
#if MODE == 1
  albedo *= vI2;
#endif
#if MODE == 3
  albedo *= texA(vUV).r;
#elif MODE == 4
  albedo = texA(vUV).rgb;  // sRGB texture (hardware- or shader-decoded)
#elif MODE == 5
  vec4 tx = texA(vUV); if (tx.a < 0.4) discard; albedo = tx.rgb;
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
  {  // three bumpMap (bumpScale 1.2) via perturbNormalArb; height ~ asphalt grain luminance
    vec2 dx = dFdx(vUV), dy = dFdy(vUV);
    float h0 = sqrt(texA(vUV).g) * 1.8, hx = sqrt(texA(vUV + dx).g) * 1.8, hy = sqrt(texA(vUV + dy).g) * 1.8;
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
  float G = 0.5 / max(ndl * sqrt(ndv * ndv * (1.0 - a2) + a2) + ndv * sqrt(ndl * ndl * (1.0 - a2) + a2), 1e-6);
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
  // AMB_TINT: per-channel calibration of our hemi + ambient-cube against three's PMREM IBL (up-facing vs side-facing fit)
  vec3 AMB_TINT = mix(vec3(uAmb(3).w, uAmb(4).w, uAmb(5).w), vec3(uAmb(0).w, uAmb(1).w, uAmb(2).w), clamp(N.y, 0.0, 1.0));
  vec3 irr = (hemi / 3.14159 + envD * envI) * AMB_TINT;
  col += diffC * irr;
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
  if ((flags & 64) != 0) {  // MeshPhysicalMaterial clearcoat 0.6 / clearcoatRoughness 0.25 (beak)
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
  oCol = vec4(pbSanitize(col), 1.0);
}
