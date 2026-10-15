// 2D HUD matching the JS index.html overlay (glass panels, cam chips, gauge, score/time bar,
// toasts, settings panel, help, pause). Built from Axmol DrawNode + Label; redrawn only on change.
#pragma once
#include "axmol.h"
#include "gfx/AADraw.h"
#include "game/Game.h"
#include <functional>
#include <map>
class Hud : public ax::Node {
 public:
  static Hud* create(pb::Game* g) { auto h = new Hud(); h->_g = g; h->init(); h->autorelease(); return h; }
  bool init() override;
  void tick(float dt, float fps, const std::string& perf);
  bool mouseDown(ax::Vec2 p);  // true when consumed by the HUD
  void mouseMove(ax::Vec2 p);
  void mouseUp();
  void emoSprite(ax::Node* parent, const std::string& name, float px, ax::Vec2 pos);
  void toast(const std::string& title, const std::string& sub, const std::string& emo = "");  // emo = Content/emoji/<emo>.png (Noto Color Emoji)
  void toggleUi() { _uiHidden = !_uiHidden; setVisible(!_uiHidden); }
  void toggleSettings() { _showSettings = !_showSettings; _dirty = true; }
  void toggleHelp() { _showHelp = !_showHelp; _dirty = true; }
  // touch pads (index.html .touch): shown for coarse pointers / PB2_TOUCH
  void setTouchMode(bool on) { if (_touchOn != on) { _touchOn = on; _dirty = true; } } bool touchMode() const { return _touchOn; }
  bool touchDown(int id, ax::Vec2 p); void touchMove(int id, ax::Vec2 p); void touchUp(int id);
  // backdrop-filter: glass panels register (node-local rect, radius); gathered every frame in world space for Game::blurRects
  void gatherBlur(std::vector<pb::Game::BlurRect>& out);
  bool wantsScreenshot = false, wantsFullscreen = false, wantsShare = false; std::string wantsUrl;
 private:
  struct Btn { ax::Rect r; std::function<void()> fn; };
  struct Slider { ax::Rect r; float lo, hi, step; float* v; bool set = false; float tx0 = 0, tx1 = 0; };  // r: hit area, [tx0,tx1]: value 0..1 across the drawn track
  struct Toast { ax::Node* n; float t; };
  pb::Game* _g = nullptr; ax::Size _sz;
  pb::AADrawNode *_bgDim = nullptr, *_dynDim = nullptr; ax::Node* _staticDim = nullptr; ax::Node* _hints = nullptr; std::vector<ax::Label*> _dimStatic; pb::AADrawNode *_bg = nullptr, *_dyn = nullptr; ax::Node* _static = nullptr;
  ax::Label *_speedVal, *_cad, *_gear, *_dist, *_camL, *_fishL, *_achL, *_clock, *_fps, *_pause, *_pop;
  std::vector<ax::Label*> _camBtns; std::vector<Btn> _btns; std::vector<Slider> _sliders; std::vector<Toast> _toasts;
  ax::Node* _settings = nullptr; ax::Node* _help = nullptr; Slider _drag{}; bool _dragging = false; int _dragTouch = -2; ax::Rect _timeTrack;
  bool _dirty = true, _uiHidden = false, _showSettings = false, _showHelp = false; int _lastCam = -1, _sunIco = -1; ax::Sprite* _tEmo[2] = {nullptr, nullptr}; bool _narrow = false; float _cadX = 0, _distX = 0; float _popT = -1, _bump = 0; int _lastFish = 0;
  ax::Label* label(ax::Node* parent, const std::string& s, float size, ax::Vec2 pos, ax::Vec2 anchor, ax::Color4B c = ax::Color4B(244, 246, 251, 255), bool bold = false);
  void panel(pb::AADrawNode* d, ax::Rect r, float rad, ax::Color4F fill, bool stroke = true);
  struct SRow { float* v; bool* b; int* e; int ne; };
  std::vector<Btn> _settingsBtns; ax::Rect _settingsRect; pb::AADrawNode* _settingsDyn = nullptr; ax::Node* _settingsVal = nullptr;
  uint32_t _openMask = 1; bool _settingsRebuild = false; int _rows = 0; float _rowsY0 = 0, _rowH = 24; std::vector<SRow> _settingsRows;
  struct TBtn { ax::Vec2 c; float r; int kind; };  // 0 left 1 right 2 down 3 up 4 bell 5 trick 6 jump
  std::vector<TBtn> _tbtns; std::map<int, int> _tptr; bool _touchOn = false; pb::AADrawNode* _touchD = nullptr; double _lastAct[8] = {};
  void layoutTouch(); void drawTouch(); void applyTouchKeys();
  struct GlassRec { ax::Rect r; float rad; }; std::map<pb::AADrawNode*, std::vector<GlassRec>> _glass;
  void buildSettings(); void buildHelp(); void rebuildStatic();
};
