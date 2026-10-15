#version 310 es
#include "pb_vs_head.glsl"
layout(location = 0) in vec3 a_position;
layout(std140, binding = 0) uniform vs_ub { vec4 vF[38]; };
#include "pb_frame.glsl"
layout(location = 0) out vec3 vDir;
void main() { vDir = a_position; vec4 p = uViewProj * vec4(a_position + uCamPos.xyz, 1.0); gl_Position = p.xyww; }
