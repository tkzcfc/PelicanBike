#version 310 es
#include "pb_vs_head.glsl"
layout(location = 0) in vec3 a_position; layout(location = 2) in vec4 a_color;
layout(std140, binding = 0) uniform vs_ub { vec4 vF[38]; vec4 uOffset; };
#include "pb_frame.glsl"
layout(location = 0) out float vA;
void main() { float seed = a_color.r * 100.0; vec3 p = a_position; float t = uCamPos.w; p.x = mod(p.x - uOffset.x + 60.0, 200.0) - 60.0;
  p.x += sin(t * 0.7 + seed) * 0.6; p.y += sin(t * 0.9 + seed * 1.3) * 0.35; p.z += cos(t * 0.6 + seed * 0.7) * 0.6;
  gl_Position = uViewProj * vec4(p, 1.0); float z = max(dot(p - uCamPos.xyz, uCamFwd.xyz), 0.05);
  vA = pow(0.5 + 0.5 * sin(t * 2.2 + seed * 3.0), 3.0); gl_PointSize = 0.12 * uCamFwd.w / z; }
