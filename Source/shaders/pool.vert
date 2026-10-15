#version 310 es
#include "pb_vs_head.glsl"
layout(location = 0) in vec3 a_position; layout(location = 2) in vec4 a_color;
layout(std140, binding = 0) uniform vs_ub { vec4 vF[38]; mat4 uModel; };
#include "pb_frame.glsl"
layout(location = 0) out vec2 vUv;
void main() { vUv = a_color.rg * 2.0 - 1.0; gl_Position = uViewProj * (uModel * vec4(a_position, 1.0)); }
