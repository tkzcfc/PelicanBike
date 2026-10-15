#version 310 es
#include "pb_fs_head.glsl"
#include "pb_post.glsl"
layout(binding = 0) uniform sampler2D uHdr;
layout(binding = 2) uniform sampler2D uB0; layout(binding = 3) uniform sampler2D uB1; layout(binding = 4) uniform sampler2D uB2;
layout(binding = 5) uniform sampler2D uB3; layout(binding = 6) uniform sampler2D uB4;
layout(binding = 7) uniform sampler2D uBlur;
layout(std140) uniform fs_ub {
  vec4 uBloomF;   // factors 0..3 (pre-multiplied by strength)
  vec4 uGrade;    // x factor4, y vignette, z warm, w grain
  vec4 uExpTime;  // x exposure, y time, z backdrop rect count, w framebuffer height (px)
  vec4 uRects[24];  // backdrop-filter rects (fb px, y up)
  vec4 uRectP[24];  // x radius, y amount, z px scale
  vec4 uPostP;      // x manual bilinear, y write FXAA luma to alpha
};
float fixf(float x) { uint b = floatBitsToUint(x); if ((b & 0x7f800000u) == 0x7f800000u) return ((b & 0x007fffffu) == 0u && (b & 0x80000000u) == 0u) ? 48.0 : 0.0; return clamp(x, 0.0, 48.0); }
float h(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
vec3 RRTAndODTFit(vec3 v) { vec3 a = v * (v + 0.0245786) - 0.000090537; vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081; return a / b; }
vec3 aces(vec3 color) {
  const mat3 IM = mat3(vec3(0.59719, 0.07600, 0.02840), vec3(0.35458, 0.90834, 0.13383), vec3(0.04823, 0.01566, 0.83777));
  const mat3 OM = mat3(vec3(1.60475, -0.10208, -0.00327), vec3(-0.53108, 1.10813, -0.07276), vec3(-0.07367, -0.00605, 1.07602));
  color *= uExpTime.x / 0.6; color = IM * color; color = RRTAndODTFit(color); color = OM * color; return clamp(color, 0.0, 1.0);
}
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 oCol;
void main() {
  // screen-space uv (y up, GL convention) for the grain hash and vignette; vUv itself is the texture coordinate
  vec2 suv = vUv;
#ifdef METAL
  suv.y = 1.0 - suv.y;
#endif
  bool man = uPostP.x > 0.5;
  vec3 c = pbTex(uHdr, vUv, man); c = vec3(fixf(c.r), fixf(c.g), fixf(c.b));
  float shade = 0.0;
  int nr = int(uExpTime.z + 0.5);
  if (nr > 0) {
    float m = 0.0; vec2 p = gl_FragCoord.xy;
#ifdef METAL
    p.y = uExpTime.w - p.y;  // top-left origin on Metal
#endif
    for (int i = 0; i < 24; i++) { if (i >= nr) break; vec4 r = uRects[i]; float rad = uRectP[i].x;
      vec2 hc = (r.zw - r.xy) * 0.5; vec2 q = abs(p - (r.xy + hc)) - hc + rad; float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - rad;
      m = max(m, uRectP[i].y * clamp(0.5 - d, 0.0, 1.0));
      // box-shadow: 0 10px 40px rgba(0,0,0,.35) — outside the border box only
      float k = uRectP[i].z; vec2 q2 = abs(p + vec2(0.0, 10.0 * k) - (r.xy + hc)) - hc + rad; float ds = length(max(q2, 0.0)) + min(max(q2.x, q2.y), 0.0) - rad;
      shade = max(shade, 0.35 * uRectP[i].y * (1.0 - smoothstep(-40.0 * k, 40.0 * k, ds)) * clamp(d + 0.5, 0.0, 1.0)); }
    if (m > 0.0) { vec3 bl = pbTex(uBlur, vUv, man); bl = vec3(fixf(bl.r), fixf(bl.g), fixf(bl.b)); float l = dot(bl, vec3(0.2126, 0.7152, 0.0722)); bl = max(mix(vec3(l), bl, 1.3), 0.0);
      c = mix(c, bl, min(m, 1.0)); }
  }
  vec3 b = pbTex(uB0, vUv, man) * uBloomF.x + pbTex(uB1, vUv, man) * uBloomF.y + pbTex(uB2, vUv, man) * uBloomF.z
         + pbTex(uB3, vUv, man) * uBloomF.w + pbTex(uB4, vUv, man) * uGrade.x;
  vec3 col = c + b;
  vec2 cc = suv - 0.5; float r = length(cc);
  col *= mix(1.0, smoothstep(0.85, 0.2, r), uGrade.y);
  col *= mix(vec3(1.0), vec3(1.06, 1.0, 0.92), uGrade.z);
  col *= 1.0 + (h(suv * 1000.0 + fract(uExpTime.y)) - 0.5) * uGrade.w * 2.0;
  col = aces(col);
  col = mix(pow(col, vec3(1.0 / 2.4)) * 1.055 - 0.055, col * 12.92, step(col, vec3(0.0031308)));
  col *= 1.0 - shade;
  // FXAA reads perceptual luma from alpha (FXAA 3.11 "luma in alpha" convention)
  oCol = vec4(col, uPostP.y > 0.5 ? dot(col, vec3(0.299, 0.587, 0.114)) : 1.0);
}
