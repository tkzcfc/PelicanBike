#version 310 es
#include "pb_fs_head.glsl"
// sanitize + luminosity high pass (UnrealBloomPass LuminosityHighPassShader, smoothWidth 0.01)
layout(binding = 0) uniform sampler2D uSrc;
layout(std140, binding = 1) uniform fs_ub { vec4 uThreshold; };
float fixf(float x) { uint b = floatBitsToUint(x); if ((b & 0x7f800000u) == 0x7f800000u) return ((b & 0x007fffffu) == 0u && (b & 0x80000000u) == 0u) ? 48.0 : 0.0; return clamp(x, 0.0, 48.0); }
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 oCol;
void main() {
  vec3 c = texture(uSrc, vUv).rgb; c = vec3(fixf(c.r), fixf(c.g), fixf(c.b));
  float v = dot(c, vec3(0.2126, 0.7152, 0.0722));
  float alpha = smoothstep(uThreshold.x, uThreshold.x + 0.01, v);
  oCol = vec4(c * alpha, 1.0);
}
