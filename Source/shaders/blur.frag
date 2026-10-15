#version 310 es
#include "pb_fs_head.glsl"
#include "pb_post.glsl"
layout(binding = 0) uniform sampler2D uSrc;
layout(std140, binding = 1) uniform fs_ub {
  vec4 uDirP;    // xy: texel step, z: pair count, w: centre weight
  vec4 uOW[11];  // x: paired-tap offset, y: weight
  vec4 uPostP;   // x: manual bilinear
};
float fixf(float x) { uint b = floatBitsToUint(x); if ((b & 0x7f800000u) == 0x7f800000u) return ((b & 0x007fffffu) == 0u && (b & 0x80000000u) == 0u) ? 48.0 : 0.0; return clamp(x, 0.0, 48.0); }
vec3 tap(vec2 uv) { vec3 c = pbTex(uSrc, uv, uPostP.x > 0.5); return vec3(fixf(c.r), fixf(c.g), fixf(c.b)); }  // raw HDR may hold NaN/Inf
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 oCol;
void main() {
  vec2 dir = uDirP.xy;
#ifdef METAL
  dir.y = -dir.y;
#endif
  int pairs = int(uDirP.z + 0.5);
  vec3 s = tap(vUv) * uDirP.w;
  for (int i = 0; i < 11; i++) { if (i >= pairs) break; vec2 o = dir * uOW[i].x;
    s += (tap(vUv + o) + tap(vUv - o)) * uOW[i].y; }
  oCol = vec4(s, 1.0);
}
