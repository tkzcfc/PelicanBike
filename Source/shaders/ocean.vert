#version 310 es
#include "pb_vs_head.glsl"
layout(location = 0) in vec3 a_position;
layout(std140, binding = 0) uniform vs_ub { vec4 vF[38]; };
#include "pb_frame.glsl"
layout(location = 0) out vec3 vWorld; layout(location = 1) out vec3 vNormal; layout(location = 2) out float vHeight; layout(location = 3) out vec2 vWP;
const float SHORE = -14.60;
vec3 gerstner(vec2 p, vec2 dir, float steep, float wl, float phase, float amp, inout vec3 tangent, inout vec3 binormal) {
  float k = 6.28318 / wl; vec2 d = normalize(dir); float f = k * dot(d, p) + phase;
  float st = steep * amp; float a = st / k; float sf = sin(f); float cf = cos(f);
  tangent += vec3(-d.x * d.x * st * sf, d.x * st * cf, -d.x * d.y * st * sf);
  binormal += vec3(-d.x * d.y * st * sf, d.y * st * cf, -d.y * d.y * st * sf);
  return vec3(d.x * a * cf, a * sf, d.y * a * cf);
}
void main() {
  vec4 wp = vec4(a_position.x, -1.6, a_position.z, 1.0);
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
