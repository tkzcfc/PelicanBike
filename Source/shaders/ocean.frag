#version 310 es
#include "pb_fs_head.glsl"
layout(std140) uniform fs_ub { vec4 fF[38]; };
#include "pb_frame.glsl"
#include "pb_sky.glsl"
layout(location = 0) in vec3 vWorld; layout(location = 1) in vec3 vNormal; layout(location = 2) in float vHeight; layout(location = 3) in vec2 vWP;
layout(location = 0) out vec4 oCol;
const float SHORE = -14.60;
const vec3 uDeep = vec3(0.0036, 0.0423, 0.1022);
const vec3 uShallow = vec3(0.0137, 0.3654, 0.3916);
const vec3 uLampColor = vec3(1.0, 0.4564, 0.1441);
void main() {
  float uTime = uCamPos.w; float uNight = uSunDir.w; float uDayLight = uSunColor.w;
  vec3 cameraPosition = uCamPos.xyz;
  vec3 N = normalize(vNormal);
  float dist = length(vWorld - cameraPosition);
  vec2 q = vWP * 0.55; float e = 0.35;
  vec2 f1 = vec2(0.35, 0.6) * uTime; vec2 f2 = vec2(-0.5, 0.25) * uTime;
  float h0 = vnoise(q + f1) + 0.5 * vnoise(q * 2.7 + f2);
  float hx = vnoise(q + vec2(e, 0.0) + f1) + 0.5 * vnoise((q + vec2(e, 0.0)) * 2.7 + f2);
  float hz = vnoise(q + vec2(0.0, e) + f1) + 0.5 * vnoise((q + vec2(0.0, e)) * 2.7 + f2);
  float detail = 0.55 * (1.0 - smoothstep(20.0, 160.0, dist));
  N = normalize(N + vec3(h0 - hx, 0.0, h0 - hz) * detail);
  vec3 V = normalize(cameraPosition - vWorld);
  float ndv = max(dot(N, V), 0.0);
  float fres = 0.02 + 0.98 * pow(clamp(1.0 - ndv, 0.0, 1.0), 5.0);
  vec3 R = reflect(-V, N); R.y = abs(R.y) + 0.02; R = normalize(R);
  vec3 refl = skyBase(R);
  float depthF = smoothstep(SHORE - 2.0, SHORE - 45.0, vWorld.z);
  vec3 water = mix(uShallow, uDeep, depthF) * (0.12 + 0.88 * uDayLight);
  float sss = pow(max(dot(V, -uSunDir.xyz), 0.0), 3.0) * clamp(vHeight + 0.3, 0.0, 1.5);
  water += uShallow * uSunColor.rgb * sss * 0.35 * (1.0 - uNight);
  vec3 col = mix(water, refl, fres);
  float rs = max(dot(R, uSunDir.xyz), 0.0);
  col += uSunColor.rgb * (pow(rs, 420.0) * 30.0 + pow(rs, 60.0) * 0.7) * (1.0 - uNight) * step(-0.05, uSunDir.y);
  float rm = max(dot(R, uMoonDir.xyz), 0.0);
  col += vec3(0.75, 0.82, 1.0) * (pow(rm, 250.0) * 2.2 + pow(rm, 30.0) * 0.06) * uNight;
  float fn = vnoise(vWP * 1.3 + uTime * 0.4) * 0.6 + vnoise(vWP * 4.0 - uTime * 0.3) * 0.4;
  float crest = smoothstep(0.62, 1.05, vHeight + fn * 0.35) * smoothstep(-8.0, -40.0, vWorld.z);
  float shoreLine = SHORE + 0.9 * sin(vWP.x * 0.045) + 0.5 * sin(vWP.x * 0.13 + 1.7);
  float surge = sin(uTime * 0.7 + vWP.x * 0.021) * 1.3;
  float sd = vWorld.z - (shoreLine + surge * 0.5);
  float shoreFoam = smoothstep(-5.5, 0.5, sd) * (0.55 + 0.45 * fn);
  shoreFoam *= smoothstep(0.35, 0.55, fn + smoothstep(-2.5, 0.8, sd) * 0.5);
  float foam = clamp(max(crest * 0.75, shoreFoam), 0.0, 1.0);
  vec3 foamCol = mix(vec3(0.06, 0.08, 0.12), vec3(0.95, 0.97, 1.0), uDayLight) * (0.6 + 0.4 * uMoonDir.w);
  col = mix(col, foamCol, foam * 0.85);
  float lampRow = pow(max(0.0, 1.0 - abs(fract((vWP.x - 12.0) / 36.0 + 0.5) - 0.5) * 7.0), 2.0);
  col += uLampColor * lampRow * smoothstep(-40.0, -14.0, vWorld.z) * uNight * 0.25 * (0.5 + 0.5 * fn);
  float fogF = 1.0 - exp(-pow(dist * uGround.w, 1.35));
  vec3 fd = vWorld - cameraPosition;
  vec3 fogCol = skyBase(normalize(vec3(fd.x, max(fd.y * 0.02, 0.0) + 0.004, fd.z)));
  col = mix(col, fogCol, clamp(fogF, 0.0, 1.0));
  oCol = vec4(pbSanitize(col), 1.0);
}
