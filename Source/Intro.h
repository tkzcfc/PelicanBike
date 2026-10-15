// Intro card (index.html #intro): animated SVG-art port, gradient title, chips, 开始骑行 / 静音进入, key hints, links.
#pragma once
#include "axmol.h"
#include "gfx/AADraw.h"
#include <functional>
class Intro : public ax::Node {
 public:
  static Intro* create() { auto n = new Intro(); n->init(); n->autorelease(); return n; }
  bool init() override;
  void tick(float dt);
  bool mouseDown(ax::Vec2 p);  // consumes every click while visible
  std::function<void(bool withSound)> onStart; std::function<void(const std::string&)> onUrl;
  bool active() const { return _fade < 1; } bool gone() const { return _fade >= 1; }
  void start(bool snd) { if (_starting) return; _starting = true; if (onStart) onStart(snd); }
 private:
  pb::AADrawNode *_bg = nullptr, *_art = nullptr; ax::Node* _ui = nullptr; float _t = 0, _fade = 0; bool _starting = false;
  ax::Rect _bStart, _bMute, _lGh; ax::Size _sz; bool _compact = false; float _off = 0;
};
// kbd-token line helper shared with the HUD hints: tokens starting with '\x01' are drawn as <kbd> keys
float layoutKbdLine(ax::Node* parent, pb::AADrawNode* d, const std::vector<std::string>& toks, float cx, float y, float fontSize, ax::Color4B textCol, bool measureOnly = false);
