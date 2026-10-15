// Gameplay FX ported from main.js/effects.js: fish pickups (+halo), jumping fish, CPU particle
// pools drawn as GPU point sprites (dust/sparkles/splash), confetti, Verlet scarf, speed lines.
#pragma once
#include "gfx/Gfx.h"
#include "game/Common.h"
#include "math/Mat4.h"
#include <vector>
#include <string>
namespace pb {
struct Particle { V3 p, v; float r, g, b, size, life = 0, maxLife = 1, drag, grav; bool ground; };
struct PSys {
  std::vector<Particle> p; int cursor = 0; float opacity = 1;
  void init(int n, float op) { p.resize(n); opacity = op; }
  void emit(V3 pos, V3 vel, Col c, float size, float life, float drag = 0.5f, float grav = 0, bool ground = false);
  void update(float dt, float scroll);
};
struct Fish { bool active = false; double x = 0; float y = 0, z = 0; bool golden = false; float phase = 0, caught = -1; V3 pos; float scale = 1.35f; float rx = 0, ry = 0, haloA = 0.5f; };
struct Jumper { float t = -1; double x = 0; float z = 0, dur = 1.2f, h = 1.5f; int dir = 1; V3 pos; float rz = 0, ry = 0; };
struct ConfettiP { V3 pos{0, -100, 0}, vel, rot, spin; float life = 0; };

struct FxEvents { std::vector<std::string> unlocks; std::vector<std::pair<std::string, bool>> pops; V3 popWorld; };
}  // namespace pb
