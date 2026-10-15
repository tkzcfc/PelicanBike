#version 310 es
#include "pb_vs_head.glsl"
layout(location = 0) in vec3 a_position;
layout(std140, binding = 0) uniform vs_ub { vec4 vF[38]; };
#include "pb_frame.glsl"
void main() { gl_Position = uViewProj * vec4(a_position, 1.0); }
