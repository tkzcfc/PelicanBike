#version 310 es
#include "pb_vs_head.glsl"
layout(location = 0) in vec3 a_position; layout(location = 1) in vec4 a_normal; layout(location = 2) in vec4 a_color;
layout(std140, binding = 0) uniform vs_ub { vec4 vF[38]; mat4 uModel; };
#include "pb_frame.glsl"
layout(location = 0) out float vY; layout(location = 1) out vec3 vN; layout(location = 2) out vec3 vV;
void main() { vY = a_color.r; vec4 wp = uModel * vec4(a_position, 1.0); vN = normalize(mat3(uModel) * pbNormal(a_normal).xyz); vV = normalize(uCamPos.xyz - wp.xyz); gl_Position = uViewProj * wp; }
