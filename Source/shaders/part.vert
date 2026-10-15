#version 310 es
#include "pb_vs_head.glsl"
// particle stream (32 B): pos f32x3 | rgba f32x4 | size f32
layout(location = 0) in vec3 a_position; layout(location = 2) in vec4 a_color; layout(location = 3) in float a_size;
layout(std140, binding = 0) uniform vs_ub { vec4 vF[38]; };
#include "pb_frame.glsl"
layout(location = 0) out vec4 vC;
void main() { gl_Position = uViewProj * vec4(a_position, 1.0); float z = max(dot(a_position - uCamPos.xyz, uCamFwd.xyz), 0.1);
  gl_PointSize = a_size * uCamFwd.w / z; vC = a_color; }
