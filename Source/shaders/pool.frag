#version 310 es
precision highp float;
layout(std140) uniform fs_ub { vec4 uOpacity; };
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 oCol;
void main() { float r = length(vUv); if (r > 1.0) discard; float a = r < 0.25 ? mix(1.0, 0.8, r / 0.25) : mix(0.8, 0.0, (r - 0.25) / 0.75);
  oCol = vec4(vec3(1.0, 0.456, 0.136), a * uOpacity.x); }
