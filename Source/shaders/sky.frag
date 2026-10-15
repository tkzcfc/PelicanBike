#version 310 es
#include "pb_fs_head.glsl"
layout(std140) uniform fs_ub { vec4 fF[38]; };
#include "pb_frame.glsl"
#include "pb_sky.glsl"
layout(location = 0) in vec3 vDir;
layout(location = 0) out vec4 oCol;
void main() {
  vec3 dir = normalize(vDir);
  vec3 col = skyBase(dir);
  float uNight = uSunDir.w; float uTime = uCamPos.w;
  if (uNight > 0.01 && dir.y > -0.05) {
    vec3 p = dir * 320.0; vec3 cell = floor(p); float h = hash13(cell);
    if (h > 0.9955) {
      vec3 c = cell + 0.5 + (vec3(hash13(cell + 1.3), hash13(cell + 7.1), hash13(cell + 3.7)) - 0.5) * 0.7;
      float d = length(p - c); float star = smoothstep(0.42, 0.0, d);
      float tw = 0.55 + 0.45 * sin(uTime * (1.5 + h * 7.0) + h * 91.0);
      vec3 sc = mix(vec3(1.0, 0.82, 0.7), vec3(0.75, 0.85, 1.0), hash13(cell + 5.0));
      col += sc * star * tw * uNight * 3.0 * smoothstep(-0.02, 0.2, dir.y);
    }
    vec3 axis = normalize(vec3(0.3, 0.55, 0.78));
    float band = exp(-pow(dot(dir, axis) * 5.0, 2.0));
    float mw = fbm(dir.xz * 9.0 + dir.y * 4.0);
    col += vec3(0.28, 0.3, 0.45) * band * mw * mw * 0.35 * uNight * smoothstep(0.0, 0.3, dir.y);
  }
  float sd = dot(dir, uSunDir.xyz);
  col += uSunColor.rgb * smoothstep(0.99955, 0.99975, sd) * 18.0 * (1.0 - uNight);
  float md = dot(dir, uMoonDir.xyz);
  float moon = smoothstep(0.99925, 0.99945, md);
  if (moon > 0.0) {
    vec3 t1 = normalize(cross(uMoonDir.xyz, vec3(0.0, 1.0, 0.0))); vec3 t2 = cross(t1, uMoonDir.xyz);
    vec2 mp = vec2(dot(dir, t1), dot(dir, t2)) / 0.033;
    float lit = smoothstep(-0.25, 0.35, mp.x + 0.35);
    float craters = 0.82 + 0.18 * vnoise(mp * 6.0);
    col = mix(col, vec3(1.0, 0.97, 0.9) * 2.4 * craters * mix(0.12, 1.0, lit), moon * smoothstep(0.0, 0.5, uNight));
  }
  if (dir.y > 0.0) {
    vec2 uv = dir.xz / (dir.y + 0.1) * 0.55 + uCloudOff.xy;
    float n = fbm(uv * 0.9); float n2 = fbm(uv * 2.3 + 4.0);
    float d = n * 0.75 + n2 * 0.35;
    float cover = smoothstep(1.0 - uZenith.w, 1.0 - uZenith.w + 0.32, d);
    float lightN = fbm(uv * 0.9 - uSunDir.xz * 0.12);
    float lit = clamp(0.55 + (n - lightN) * 3.0, 0.0, 1.0);
    float sunSide = pow(max(dot(dir, uSunDir.xyz), 0.0), 3.0);
    vec3 cc = mix(uCloudShade.rgb, uCloudTint.rgb, lit) + uSunColor.rgb * sunSide * 0.5 * (1.0 - uNight);
    float horizonFade = smoothstep(0.0, 0.14, dir.y);
    col = mix(col, cc, cover * horizonFade * 0.92);
  }
  oCol = vec4(pbSanitize(col), 1.0);
}
