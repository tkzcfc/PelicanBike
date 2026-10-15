#version 310 es
precision highp float;
layout(std140) uniform fs_ub { vec4 uHalo; };
layout(location = 0) in vec4 vC;
layout(location = 0) out vec4 oCol;
void main() { float d = length(gl_PointCoord - 0.5); float a;
  if (uHalo.x > 0.5) { float r = d * 2.0; a = r < 0.25 ? mix(1.0, 0.8, r / 0.25) : mix(0.8, 0.0, clamp((r - 0.25) / 0.75, 0.0, 1.0)); }
  else a = smoothstep(0.5, 0.05, d);
  a *= vC.a; if (a < 0.003) discard; oCol = vec4(vC.rgb, a); }
