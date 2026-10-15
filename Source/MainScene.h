#pragma once
#include "axmol.h"
#include "game/Game.h"
class Hud; class Intro;

class MainScene : public ax::Scene {
 public:
  bool init() override;
  void update(float dt) override;
  void visit(ax::Renderer* renderer, const ax::Mat4& parentTransform, uint32_t parentFlags) override;
 private:
  pb::Game _game;
  ax::Label* _stats = nullptr; ax::Label* _speed = nullptr; ax::Label* _title = nullptr;
  float _acc = 0; int _frames = 0; float _fps = 0; double _sumUpd = 0, _sumRen = 0, _sumGpu = 0;
  int _shotFrame = -1; std::string _shotPath; int _frameCount = 0; float _fixedDt = 0; bool _showStats = true;
  bool _quitAfterShot = false; Hud* _hud = nullptr; std::string _perf; bool _down = false, _dragged = false, _hudDrag = false; ax::Vec2 _downP, _lastP; float _startToast = 0; Intro* _intro = nullptr; float _hudFade = 0; ax::DrawNode* _bars = nullptr; float _barT = 0; float _holdAt = 0; bool _held = false; bool _adaptive = true, _padDown = false, _mouseSeen = false;
};
