#version 310 es
#include "pb_fs_head.glsl"
// FXAA 3.11 (Timothy Lottes), PC quality path, preset 29 search steps. Replaces MSAA, which ax::backend does not
// expose. Runs after tonemap + sRGB encode on an RGBA8 target; the final pass writes the luma into alpha.
layout(binding = 0) uniform sampler2D uSrc;
layout(std140) uniform fs_ub { vec4 uFx; };  // xy: 1/size, z: subpix, w: edge threshold
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 oCol;
float L(vec2 p) { return textureLod(uSrc, p, 0.0).a; }
const float EDGE_MIN = 0.0312;
const float QP[12] = float[12](1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0);
void main() {
  vec2 rcp = uFx.xy; vec2 posM = vUv;
  vec4 rgbyM = textureLod(uSrc, posM, 0.0);
  float lumaM = rgbyM.a;
  float lumaS = L(posM + vec2(0.0, 1.0) * rcp), lumaE = L(posM + vec2(1.0, 0.0) * rcp);
  float lumaN = L(posM + vec2(0.0, -1.0) * rcp), lumaW = L(posM + vec2(-1.0, 0.0) * rcp);
  float rangeMax = max(max(lumaN, lumaW), max(lumaE, max(lumaS, lumaM)));
  float rangeMin = min(min(lumaN, lumaW), min(lumaE, min(lumaS, lumaM)));
  float range = rangeMax - rangeMin;
  if (range < max(EDGE_MIN, rangeMax * uFx.w)) { oCol = vec4(rgbyM.rgb, 1.0); return; }
  float lumaNW = L(posM + vec2(-1.0, -1.0) * rcp), lumaSE = L(posM + vec2(1.0, 1.0) * rcp);
  float lumaNE = L(posM + vec2(1.0, -1.0) * rcp), lumaSW = L(posM + vec2(-1.0, 1.0) * rcp);
  float lumaNS = lumaN + lumaS, lumaWE = lumaW + lumaE;
  float subpixRcpRange = 1.0 / range;
  float subpixNSWE = lumaNS + lumaWE;
  float edgeHorz1 = -2.0 * lumaM + lumaNS, edgeVert1 = -2.0 * lumaM + lumaWE;
  float lumaNESE = lumaNE + lumaSE, lumaNWNE = lumaNW + lumaNE;
  float edgeHorz2 = -2.0 * lumaE + lumaNESE, edgeVert2 = -2.0 * lumaN + lumaNWNE;
  float lumaNWSW = lumaNW + lumaSW, lumaSWSE = lumaSW + lumaSE;
  float edgeHorz4 = abs(edgeHorz1) * 2.0 + abs(edgeHorz2), edgeVert4 = abs(edgeVert1) * 2.0 + abs(edgeVert2);
  float edgeHorz3 = -2.0 * lumaW + lumaNWSW, edgeVert3 = -2.0 * lumaS + lumaSWSE;
  float edgeHorz = abs(edgeHorz3) + edgeHorz4, edgeVert = abs(edgeVert3) + edgeVert4;
  float subpixNWSWNESE = lumaNWSW + lumaNESE;
  float lengthSign = rcp.x;
  bool horzSpan = edgeHorz >= edgeVert;
  float subpixA = subpixNSWE * 2.0 + subpixNWSWNESE;
  if (!horzSpan) { lumaN = lumaW; lumaS = lumaE; }
  if (horzSpan) lengthSign = rcp.y;
  float subpixB = subpixA * (1.0 / 12.0) - lumaM;
  float gradientN = lumaN - lumaM, gradientS = lumaS - lumaM;
  float lumaNN = lumaN + lumaM, lumaSS = lumaS + lumaM;
  bool pairN = abs(gradientN) >= abs(gradientS);
  float gradient = max(abs(gradientN), abs(gradientS));
  if (pairN) lengthSign = -lengthSign;
  float subpixC = clamp(abs(subpixB) * subpixRcpRange, 0.0, 1.0);
  vec2 posB = posM;
  vec2 offNP = horzSpan ? vec2(rcp.x, 0.0) : vec2(0.0, rcp.y);
  if (!horzSpan) posB.x += lengthSign * 0.5; else posB.y += lengthSign * 0.5;
  vec2 posN = posB - offNP * QP[0], posP = posB + offNP * QP[0];
  float subpixD = -2.0 * subpixC + 3.0;
  float subpixE = subpixC * subpixC;
  if (!pairN) lumaNN = lumaSS;
  float gradientScaled = gradient * 0.25;
  float lumaMM = lumaM - lumaNN * 0.5;
  float subpixF = subpixD * subpixE;
  bool lumaMLTZero = lumaMM < 0.0;
  float lumaEndN = L(posN) - lumaNN * 0.5, lumaEndP = L(posP) - lumaNN * 0.5;
  bool doneN = abs(lumaEndN) >= gradientScaled, doneP = abs(lumaEndP) >= gradientScaled;
  if (!doneN) posN -= offNP * QP[1];
  if (!doneP) posP += offNP * QP[1];
  for (int i = 2; i < 12; i++) {
    if (doneN && doneP) break;
    if (!doneN) lumaEndN = L(posN) - lumaNN * 0.5;
    if (!doneP) lumaEndP = L(posP) - lumaNN * 0.5;
    doneN = abs(lumaEndN) >= gradientScaled; doneP = abs(lumaEndP) >= gradientScaled;
    if (!doneN) posN -= offNP * QP[i];
    if (!doneP) posP += offNP * QP[i];
  }
  float dstN = horzSpan ? posM.x - posN.x : posM.y - posN.y;
  float dstP = horzSpan ? posP.x - posM.x : posP.y - posM.y;
  bool goodSpanN = (lumaEndN < 0.0) != lumaMLTZero, goodSpanP = (lumaEndP < 0.0) != lumaMLTZero;
  float spanLengthRcp = 1.0 / (dstP + dstN);
  bool directionN = dstN < dstP;
  float dst = min(dstN, dstP);
  bool goodSpan = directionN ? goodSpanN : goodSpanP;
  float subpixG = subpixF * subpixF;
  float pixelOffset = dst * -spanLengthRcp + 0.5;
  float subpixH = subpixG * uFx.z;
  float pixelOffsetSubpix = max(goodSpan ? pixelOffset : 0.0, subpixH);
  if (!horzSpan) posM.x += pixelOffsetSubpix * lengthSign; else posM.y += pixelOffsetSubpix * lengthSign;
  oCol = vec4(textureLod(uSrc, posM, 0.0).rgb, 1.0);
}
