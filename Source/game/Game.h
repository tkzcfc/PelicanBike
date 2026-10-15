// PelicanBike core: simulation (ride/camera/lighting, ported from main.js) + frame graph.
#pragma once
#include "gfx/Gfx.h"
#include "game/Common.h"
#include "game/Rider.h"
#include "game/Fx.h"
#include "audio/Synth.h"
#include "math/Mat4.h"
#include "math/Quaternion.h"
#include <array>
#include <vector>
#include <string>

namespace pb {

struct Keys { bool up = false, down = false, left = false, right = false; };

struct RideState {
  float speed = 7.5f * 0.6f; double distance = 0; float lane = 0.9f, laneV = 0, laneTarget = 0.9f, prevLaneV = 0, latAcc = 0;
  float lean = 0, steer = 0, crank = 0, wheel = 0; int gear = 2; float y = 0, vy = 0; bool airborne = false;
  float trick = 0, trickT = 0, wheelie = 0, accel = 0; bool pedaling = true; float cadence = 0; int fish = 0, golden = 0, jumps = 0, tricks = 0, bells = 0, honks = 0;
  float lastSteer = -99, lastGearManual = -99; float t = 0, time = 0;
};

struct Settings { float volume = 0.8f, musicVolume = 0.55f; bool music = true; int quality = 2; bool bloom = true, scarf = true, helmet = true, showFps = false; int glasses = 0; /*0 auto 1 on 2 off*/ float cruise = 7.5f, hour = 17.35f, dayRate = 0.025f; bool timeFlow = true, autopilot = true, autoGear = true;
  float cloud = 0.42f, fog = 1.f, waveAmp = 1.f, exposure = 1.f, bloomBoost = 1.f; float renderScale = 1.0f; int aa = 1; /* >0: FXAA post pass (replaces MSAA) */ int shadowSize = 2048; };

struct FrameUBO {
  float viewProj[16], shadowMat[16];
  float camPos[4], sunDir[4], moonDir[4], zenith[4], horizon[4], ground[4], sunColor[4], lightDir[4], lightColor[4], hemiSky[4],
      hemiGround[4], fogColor[4], cloudTint[4], cloudShade[4], cloudOff[4], phaseA[4], phaseB[4];
  float amb[6][4];
  float lamp0[4], lamp1[4], headPos[4], headDir[4], fillPos[4], nightE[4], camFwd[4];  // irradiance ambient cube (+X,-X,+Y,-Y,+Z,-Z) from the analytic sky (replaces PMREM)
};

struct PbmPart { std::string name; std::vector<gfx::PVertex> v; std::vector<uint32_t> i; };
bool loadPbm(const std::string& file, std::vector<PbmPart>& out);

class Game {
 public:
  bool init(int winW, int winH);
  void update(float dt);
  void render(int winW, int winH);  // called from the Axmol CallbackCommand
  int targetW() const { return rtW; } int targetH() const { return rtH; }  // HDR scene target size (device px * renderScale)
  Keys keys; RideState S; Settings settings;
  enum { CAM_ORBIT, CAM_CHASE, CAM_SIDE, CAM_CINE, CAM_POV, CAM_N };
  int camMode = CAM_CHASE;
  bool lookOverride = false; V3 lookOverrideP;
  void honk() { honkReq = true; }
  void jump(); void doTrick(); void cycleCam(); void setCamMode(int id, bool quick = false);
  void orbitDrag(float dx, float dy); void orbitZoom(float steps);
  float cutFlash = 0;
  // perf
  float cpuRenderMs = 0, cpuUpdateMs = 0; gfx::Stats lastStats; float gpuMs = -1;
  std::string camName() const;
  float nightV() const { return night; } float elevV() const { return elev; }

 private:
  void buildStatic();
  void rebuildChunk(int slot, int index);
  void uploadInstances();
  void updateLighting(float dt);
  void updateRide(float dt);
  void updateCamera(float dt);
  void updateRiderPose(float dt);
  float groundAt(V3 p) const; void povPose(V3& pos, V3& look, float& f);
  ax::Quaternion camQuat, blendQuat; V3 blendPos; float blendT = 1, blendFov = 42;
  float orbR = 4.3797f, orbTheta = 0.5560f, orbPhi = 1.4679f, orbVT = 0, orbVP = 0, orbZoom = 1, lastOrbitInput = 0;  // main.js: focus+(2.3,0.45,3.7), S.lastOrbitInput ?? 0
  float cineT = 0; int cineShot = 0;
  void drawScene(bool shadowPass);
  void createTargets(int w, int h);
  void refreshAmbientCube();
  // world extras (Extras.cpp): lighthouse, boats, gulls + pelican flock, signs, fireflies, lamp pools
  void buildExtras(); void updateExtras(float dt); void drawExtras(bool shadow); void drawAdditive();
  gfx::Mesh lhStatic, lhLantern, lhBeam, boat[5], gullBody, gullWing, pelBody, pelWing, signPost, signPlate, ffMesh, pools;
  gfx::InstanceBuf birdI[4]; std::vector<float> birdData[4]; bool birdDirty = true;
  gfx::Texture texSign[3];
  gfx::Shader litBird, litSign, beamS, ffS, poolS;
  std::vector<gfx::Shader*> sceneShaders, shadowShaders;  // programs that take the per-frame constants
  void makeProg(gfx::Shader& s, const char* vs, const char* fs, gfx::VL vl = gfx::VL::Packed, bool shadowPass = false);
  void bindTex(const gfx::Shader& s, const gfx::Texture& t);
  float dbgView = 0, dfgOn = 0, blurDirP[4] = {0, 0, 0, 0};
  ax::Mat4 lhM, boatM[5], beamM[2]; float lanternE = 0.4f;
 public:
  // gameplay fx (Fx.cpp)
  void buildFx(); void updateFx(float dt); void drawFx(bool shadow); void drawFxTransparent();
  int nextFish(float range) const; void unlock(const char* id); void confettiBurst(V3 c); void spawnFish(float ahead);
  void launchJumper(float x, float z); void splashAt(float x, float z, int n); void burstDust(int n); void updateScarf(float dt, V3 a, V3 b, V3 wind, V3 col);
  Synth audio; bool audioOk = false;
  FxEvents fxEv; std::vector<std::string> achieved; bool started = true, paused = false;
  bool project(V3 w, float& sx, float& sy) const { ax::Vec4 v(w.x, w.y, w.z, 1); viewProj.transformVector(&v); if (v.w <= 0) return false; sx = v.x / v.w; sy = v.y / v.w; return true; }
  void applyQuality(int q) {  // main.js QUALITY (DPR 1): low/medium/high/ultra
    static const float PR[4] = {0.8f, 1.f, 1.f, 1.f}; static const int SH[4] = {1024, 2048, 2048, 4096}, MS[4] = {0, 2, 4, 4};
    q = std::max(0, std::min(3, q)); settings.quality = q; settings.renderScale = PR[q]; basePr = PR[q]; prCeiling = 1e9f; settings.shadowSize = SH[q]; settings.aa = MS[q]; qualityBloom = q > 0; targetsDirty = true; for (auto& c : chunkIndex) c = -99999; }
  bool qualityBloom = true, targetsDirty = false;
  // CSS backdrop-filter emulation: HUD/intro hand us rounded rects (window px, y up) blurred from a 1/4-res copy of the HDR frame
  struct BlurRect { float x0, y0, x1, y1, rad, amount; }; std::vector<BlurRect> blurRects;
  // main.js adaptQuality(): fps sampled every 0.5 s; drop render scale 0.2 after 4 slow samples, regrow 0.1 after 16 fast ones
  float basePr = 1, prCeiling = 1e9f, refreshCap = 0; int lowStreak = 0, highStreak = 0;
  void adaptQuality(float fps) {
    if (!started || paused) return;
    refreshCap = std::max(refreshCap * 0.995f, fps); float pr = settings.renderScale, lowT = std::min(40.f, std::max(20.f, refreshCap * 0.72f));  // +floor: always shed pixels under 20 fps
    lowStreak = fps < lowT ? lowStreak + 1 : 0; highStreak = fps > refreshCap * 0.93f ? highStreak + 1 : 0;
    if (lowStreak >= 4 && pr > 0.6f) { prCeiling = pr - 0.01f; settings.renderScale = std::max(0.6f, pr - 0.2f); lowStreak = highStreak = 0; }
    else if (highStreak >= 16 && pr < std::min(basePr, prCeiling) - 0.01f) { settings.renderScale = std::min(std::min(basePr, prCeiling), pr + 0.1f); highStreak = 0; }
  }
  void ringBell(); void doHonk(); void clickAt(float ndcX, float ndcY);
  void fxTrigger(const std::string& k);
  std::string poseDebug() const;  // test harness: same actions as tools/js_shot_t.mjs FX=
 private:
  Mulberry rnd{2024}; Fish fishes[12]; Jumper jumpers[5]; PSys dust, sparkles, splash; double nextFishAt = 30; float dustAcc = 0;
  std::vector<ConfettiP> confetti; std::vector<gfx::PVertex> confV; gfx::Mesh confMesh; bool confActive = false, confDirty = false;
  std::vector<V3> scarfP, scarfQ; std::vector<gfx::PVertex> scarfV; gfx::Mesh scarfMesh; bool scarfInit = false, scarfDirty = false;
  std::vector<V3> lineSeeds; V3 lineV[140]; gfx::Mesh lineMesh; float lineOpacity = 0; bool lineDirty = false;
  gfx::Mesh fishMesh, goldMesh, partMesh; gfx::InstanceBuf fishI, goldI, fishIS, goldIS; std::vector<float> partBuf;  // *IS: shadow-pass copies (Metal: one upload per buffer per frame)
  gfx::Shader partS, lineS, shBird;
  float envHour = -99;

  // GPU objects
  FrameUBO F{};
  gfx::Shader skyS, oceanS, litStatic, litInst, litRider, litTerrain, litRoad;
  gfx::Shader shStatic, shInst, shRider, shTerrain;  // shadow variants (depth only)
  gfx::Shader brightS, blurS, finalS, fxaaS;
  gfx::Mesh sky, ocean, road, strip, bulbs;
  gfx::Mesh terrain[7]; int chunkIndex[7];
  std::vector<gfx::PVertex> terrainScratch; std::vector<uint32_t> terrainIdx;
  enum { P_ROUND, P_PINE, P_PALM, P_ROCK, P_UMB, P_N };
  gfx::Mesh prop[P_N]; gfx::InstanceBuf inst[P_N]; std::vector<float> instData[P_N]; int instBase = -1000; bool instDirty = true;
  std::vector<float> chunkInst[7][P_N];
  float trickConfetti = 0, bellPulse = 0; Rider rider; bool honkReq = false, gulpReq = false; float glanceT = 0; int glanceKind = 0;
  gfx::Texture texRoad, texDetail;
  gfx::RenderTarget ldrRT, shadowRT, hdrRT, bloomA[5], bloomB[5], blurA, blurB;
  int rtW = 0, rtH = 0;
  // camera
  ax::Mat4 view, proj, viewProj, shadowVP; V3 camPos{-3.5f, 1.6f, 1.f}, camSmooth; bool camSmoothInit = false; float fov = 46;
  V3 riderPos; ax::Mat4 riderM;
  Palette pal{}; float night = 0, elev = 10; float bloomStrength = 0.4f, bloomThreshold = 1.05f, warm = 0;
  float exposureVal = 1.f;
  int frameNo = 0;
};
}  // namespace pb
