#version 310 es
precision highp float;
layout(std140, binding = 1) uniform fs_ub { vec4 uNight; };
layout(location = 0) in float vA;
layout(location = 0) out vec4 oCol;
void main() { float d = length(gl_PointCoord - 0.5); float a = smoothstep(0.5, 0.0, d); oCol = vec4(vec3(0.75, 1.0, 0.35) * 2.5, a * vA * uNight.x); }
