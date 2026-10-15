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
vec3 pbSanitize(vec3 col) { return any(isnan(col)) || any(isinf(col)) ? vec3(0.0) : col; }  // never write NaN/Inf into HDR (blur/bloom spread it)
float fixf(float x) { uint b = floatBitsToUint(x); if ((b & 0x7f800000u) == 0x7f800000u) return ((b & 0x007fffffu) == 0u && (b & 0x80000000u) == 0u) ? 48.0 : 0.0; return clamp(x, 0.0, 48.0); }
