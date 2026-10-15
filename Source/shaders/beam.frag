#version 310 es
precision highp float;
layout(std140, binding = 1) uniform fs_ub { vec4 uNight; };
layout(location = 0) in float vY; layout(location = 1) in vec3 vN; layout(location = 2) in vec3 vV;
layout(location = 0) out vec4 oCol;
void main() { float edge = pow(abs(dot(normalize(vN + 1e-6), normalize(vV + 1e-6))), 1.5); float a = pow(clamp(vY, 0.0, 1.0), 2.6) * edge * uNight.x * 0.32; oCol = vec4(vec3(1.0, 0.86, 0.55) * a, a); }
