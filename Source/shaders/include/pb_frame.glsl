// PelicanBike per-frame constants. Each stage declares `vec4 vF[PB_FRAME_VEC4]` (vs_ub) or
// `vec4 fF[PB_FRAME_VEC4]` (fs_ub) and defines FR to it before including this file; the CPU writes the
// same 38 x vec4 (FrameUBO, std140) into both. Axmol's GL backend maps uniform names without their block
// prefix, so the two stages must use different member names.
#define PB_FRAME_VEC4 38
#define uViewProj mat4(FR[0], FR[1], FR[2], FR[3])
#define uShadowMat mat4(FR[4], FR[5], FR[6], FR[7])
#define uCamPos FR[8]       // w: time
#define uSunDir FR[9]       // w: night
#define uMoonDir FR[10]     // w: sunGlow
#define uZenith FR[11]      // w: cloudCover
#define uHorizon FR[12]     // w: fog density (scene)
#define uGround FR[13]      // w: ocean fog density
#define uSunColor FR[14]    // w: dayLight (ocean)
#define uLightDir FR[15]    // dir to light, w: intensity
#define uLightColor FR[16]  // w: env intensity
#define uHemiSky FR[17]     // w: hemi intensity
#define uHemiGround FR[18]  // w: wave amp
#define uFogColor FR[19]    // w: ocean uOffset (distance mod 36000)
#define uCloudTint FR[20]   // w: shadow texel size
#define uCloudShade FR[21]
#define uCloudOff FR[22]    // xy offset, z: world shift x, w: distance mod 200
#define uPhaseA FR[23]
#define uPhaseB FR[24]      // x: phase4, y: lamp night, z: bulb emissive
#define uAmb(i) FR[25 + (i)]
#define uLamp0 FR[31]
#define uLamp1 FR[32]
#define uHeadPos FR[33]
#define uHeadDir FR[34]
#define uFillPos FR[35]
#define uNightE FR[36]
#define uCamFwd FR[37]      // xyz camera forward, w: point scale

vec3 ambientCube(vec3 n) { vec3 n2 = n * n; return n2.x * (n.x > 0.0 ? uAmb(0).rgb : uAmb(1).rgb) + n2.y * (n.y > 0.0 ? uAmb(2).rgb : uAmb(3).rgb) + n2.z * (n.z > 0.0 ? uAmb(4).rgb : uAmb(5).rgb); }

// packed vertex normal: u8x4 normalized holding (snorm8 + 128); returns GL snorm decode (xyz normal, w = flags/127)
vec4 pbNormal(vec4 u) { return max((floor(u * 255.0 + 0.5) - 128.0) / 127.0, vec4(-1.0)); }
