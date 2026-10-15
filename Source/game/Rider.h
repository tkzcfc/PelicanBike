// Rider: rigid-joint bone palette (baked 16 crank x 2 trick poses, see tools/bake_rider.mjs)
// + runtime controllers (steer, wheels, head look, jaw) + CPU neck tube (DynamicTube port).
#pragma once
#include "gfx/Gfx.h"
#include "game/Common.h"
#include "math/Mat4.h"
#include <vector>

namespace pb {
struct RiderPoseIn { float crank, trick, steer, wheel, dt, t; bool airborne; V3 look; bool honk, gulp; };

class Rider {
 public:
  bool load(const char* file);
  void update(const RiderPoseIn& in);   // computes bone palette + neck mesh
  void draw(const gfx::Shader& s);      // sets uBones and draws (2 draws)
  V3 headPos() const { return V3(bone[headB].m[12], bone[headB].m[13], bone[headB].m[14]); }
  ax::Mat4 headM() const { return bone[headB]; }
  V3 knotPos() const { return V3(knotP.x, knotP.y, knotP.z); }
  V3 bodyPos() const { return V3(bone[bodyB].m[12], bone[bodyB].m[13], bone[bodyB].m[14]); }
  V3 mouthPos() const;
  float honkK = 0, gulpK = 0; bool showHelmet = true, showGlasses = true;
  ax::Mat4 knotM() const { return bone[knotB]; } ax::Mat4 bodyM() const { return bone[bodyB]; }
 private:
  int nb = 0, F = 16, K = 2;
  struct Ctl { int bone, parent, kind; V3 axis; };
  std::vector<Ctl> ctls; std::vector<int> boneCtl;
  std::vector<float> poses;   // [K*F][nb][12]
  std::vector<ax::Mat4> bone; std::vector<float> palette; // nb*12 (3 rows of vec4)
  int bodyB = 0, headB = 0, knotB = 0, helmetB = 0, glassesB = 0; struct WingJ { int bone, side; bool shoulder; }; std::vector<WingJ> wings; std::vector<int> lids; float nextBlink = 2, blinkT = -1; std::vector<ax::Mat4> ctlScratch; float neckCol[3]; int neckMat = 0; std::vector<uint8_t> matTab;
  gfx::Mesh mesh, neck; std::vector<gfx::PVertex> neckV;
  V3 knotP; bool neckDirty = false;
  // animation state (pelican.js st)
 public:
  float spread = 0, yaw = 0, pitch = 0, jaw = 0, honkT = -1, gulpT = -1;
 private:
};
}  // namespace pb
