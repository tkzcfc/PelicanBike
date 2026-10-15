#version 310 es
precision highp float;
layout(std140, binding = 1) uniform fs_ub { vec4 uOpacity; };
layout(location = 0) out vec4 oCol;
void main() { oCol = vec4(1.0, 1.0, 1.0, uOpacity.x); }
