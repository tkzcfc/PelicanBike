// Lit vertex shader. MODE: 0 static mesh (uModel), 1 instanced props (sway), 2 rider bone palette,
// 3 terrain, 4 road, 5 sign plate, 6 instanced affine (birds/fish). SHADOW: depth-only variant.
#include "pb_vs_head.glsl"
#define PB_INSTANCED (MODE == 1 || MODE == 6)
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_normal;
layout(location = 2) in vec4 a_color;
layout(location = 3) in vec4 a_mat;
#if PB_INSTANCED && !defined(METAL)
layout(location = 4) in mat4 a_instance;  // axmol instance stream: 64 B / instance, columns = aI0..aI2, (unused)
#endif
layout(std140, binding = 0) uniform vs_ub {
  vec4 vF[38];
  mat4 uModel;
  vec4 uSway;
#if MODE == 2
  vec4 uBones[144];
#endif
};
#if PB_INSTANCED && defined(METAL)
layout(std140, binding = 1) buffer vs_inst { mat4 u_instance[]; };
#endif
#include "pb_frame.glsl"
#ifdef SHADOW
layout(location = 0) out float vDepth;
#else
layout(location = 0) out vec3 vW; layout(location = 1) out vec3 vN; layout(location = 2) out vec4 vCol; layout(location = 3) out vec4 vMat;
layout(location = 4) out vec4 vSh; layout(location = 5) out vec2 vUV; layout(location = 6) out vec3 vLP;
#if MODE == 1
layout(location = 7) out vec3 vI2;
#endif
#endif
void main() {
  vec4 aNrm = pbNormal(a_normal);
  vec3 p = a_position; vec3 n = aNrm.xyz;
#ifndef SHADOW
  vLP = a_position;
#endif
#if PB_INSTANCED
#  ifdef METAL
  mat4 I = u_instance[gl_InstanceIndex];
#  else
  mat4 I = a_instance;
#  endif
  vec4 aI0 = I[0], aI1 = I[1], aI2 = I[2];
#endif
#if MODE == 2
  { int b = int(a_mat.w * 255.0 + 0.5) * 3; vec4 r0 = uBones[b], r1 = uBones[b + 1], r2 = uBones[b + 2]; vec4 q = vec4(p, 1.0);
    p = vec3(dot(r0, q), dot(r1, q), dot(r2, q)); n = vec3(dot(r0.xyz, n), dot(r1.xyz, n), dot(r2.xyz, n)); }
#endif
  vec4 wp;
#if MODE == 1
  // aI0: chunk-relative pos (xyz) + scale; aI1: rotY, yScale, -, -; aI2: tint rgb, local x (sway phase)
  vec3 ip = aI0.xyz + vec3(uCloudOff.z, 0.0, 0.0);
  float sw = pow(max(p.y - uSway.y, 0.0), 1.4);
  vec3 q = vec3(p.x * aI0.w, p.y * aI0.w * aI1.y, p.z * aI0.w);
  float c = cos(aI1.x), s = sin(aI1.x);
  q = vec3(c * q.x + s * q.z, q.y, -s * q.x + c * q.z);
  n = vec3(c * n.x + s * n.z, n.y, -s * n.x + c * n.z);
  vec3 swv = vec3(sin(uCamPos.w * 1.3 + aI2.w * 0.31 + aI0.z * 0.17) * sw * uSway.x, 0.0, cos(uCamPos.w * 1.1 + aI2.w * 0.23) * sw * uSway.x * 0.6);
  q += vec3(c * swv.x + s * swv.z, 0.0, -s * swv.x + c * swv.z) * aI0.w;
  wp = vec4(q + ip, 1.0);
#  ifndef SHADOW
  vI2 = aI2.rgb;
#  endif
#elif MODE == 6
  { vec4 q = vec4(p, 1.0); wp = vec4(dot(aI0, q), dot(aI1, q), dot(aI2, q), 1.0); n = vec3(dot(aI0.xyz, n), dot(aI1.xyz, n), dot(aI2.xyz, n)); }
#else
  wp = uModel * vec4(p, 1.0);
  n = mat3(uModel) * n;
#endif
  gl_Position = uViewProj * wp;
#ifdef SHADOW
  vDepth = gl_Position.z / gl_Position.w * 0.5 + 0.5;  // GL window depth, backend independent (uShadowMat uses the same mapping)
#else
  vW = wp.xyz; vN = n; vCol = a_color; vMat = a_mat;
  vMat.w = aNrm.w * 127.0;  // flags byte stored in normal.w
  vSh = uShadowMat * vec4(wp.xyz + normalize(n) * 0.025, 1.0);
#  if MODE == 3
  vUV = vec2(p.x / 9.0, p.z / 9.0);
#  elif MODE == 4
  vUV = vec2(p.x / 24.0, (p.z + 2.82) / 5.64);
#  elif MODE == 5
  vUV = vec2((p.z + 0.55) / 1.1, 1.0 - (p.y + 0.55) / 1.1);
#  else
  vUV = vec2(0.0);
#  endif
#endif
}
