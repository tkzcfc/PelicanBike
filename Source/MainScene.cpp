#include "MainScene.h"
#include "Hud.h"
#include "Intro.h"
#include "Platform.h"
#include <cstdlib>
#include <cstdio>
USING_NS_AX;

static const char* env(const char* k) { const char* v = std::getenv(k); return v && *v ? v : nullptr; }

bool MainScene::init() {
  if (!Scene::init()) return false;
  auto vs = Director::getInstance()->getRenderView()->getFrameSize();
  if (auto v = env("PB2_HOUR")) { _game.settings.hour = std::atof(v); }
  { auto t = plat::urlParam("time"); if (!t.empty()) _game.settings.hour = std::atof(t.c_str()); }
  { auto q = plat::urlParam("q"); static const char* QN[4] = {"low", "medium", "high", "ultra"}; for (int i = 0; i < 4; i++) if (q == QN[i]) _game.applyQuality(i); }
  if (env("PB2_FREEZE")) _game.settings.timeFlow = false;
  if (auto v = env("PB2_SCALE")) _game.settings.renderScale = std::atof(v);
  if (auto v = env("PB2_MSAA")) _game.settings.msaa = std::atoi(v);
  { auto cu = plat::urlParam("cam"); if (!cu.empty() && !env("PB2_CAM")) plat::setEnv("PB2_CAM", cu); }
  if (auto v = env("PB2_CAM")) { std::string c(v); _game.camMode = c == "side" ? 2 : c == "orbit" ? 0 : c == "cine" ? 3 : c == "pov" ? 4 : 1; }
  if (auto v = env("PB2_HOLD")) { _holdAt = std::atof(v); }
  if (auto v = env("PB2_DT")) _fixedDt = std::atof(v);
  if (auto v = env("PB2_SPEED")) { _game.settings.cruise = std::atof(v); _game.S.speed = _game.settings.cruise * 0.6f; }
  if (auto v = env("PB2_DIST")) _game.S.distance = std::atof(v);
  if (auto v = env("PB2_SHOT")) { _shotPath = v; _shotFrame = env("PB2_SHOT_FRAME") ? std::atoi(env("PB2_SHOT_FRAME")) : 120; _quitAfterShot = true; if (_holdAt > 0) _shotFrame = 1 << 30; }
  if (auto w = env("PB2_AUDIO_TEST")) { pb::Synth::selfTest(w); AXLOGI("[pb2] audio self-test -> {}", w); }
  _adaptive = !_quitAfterShot && !env("PB2_SCALE") && !env("PB2_NOADAPT") && _fixedDt <= 0;
  _game.init(int(vs.width), int(vs.height));

  _bars = DrawNode::create(); addChild(_bars, -1);  // body.cinematic.bars letterbox (9vh, 0.8s cubic-bezier(.6,0,.2,1))
  _hud = Hud::create(&_game); addChild(_hud);
  if (env("PB2_SETTINGS")) _hud->toggleSettings();
  if (env("PB2_HELP")) _hud->toggleHelp();
  if (env("PB2_TOAST")) _game.unlock("fish1");
  { bool camParam = env("PB2_CAM") != nullptr; bool autostart = !env("PB2_INTRO") && (_quitAfterShot || camParam || env("PB2_ACT") || plat::urlParam("autostart") == "1" || env("PB2_SETTINGS") || env("PB2_HELP"));
    if (autostart) _startToast = 0.9f;
    else { _game.started = false; _hud->setVisible(false); _intro = Intro::create(); addChild(_intro, 100);
      _intro->onUrl = [this](const std::string& u) { plat::openUrl(u); };
      _intro->onStart = [this, camParam](bool snd) { _game.started = true; if (!snd) _game.settings.music = false, _game.settings.volume = 0; else plat::resumeAudio();
        if (!camParam) _game.setCamMode(0); _startToast = 0.9f; _hudFade = 0.001f; _hud->setCascadeOpacityEnabled(true); _hud->setOpacity(0); _hud->setVisible(true); }; } }
  { bool coarse = env("PB2_TOUCH") != nullptr || plat::coarsePointer(); _hud->setTouchMode(coarse);
    auto tl = EventListenerTouchAllAtOnce::create();
    tl->onTouchesBegan = [this](const std::vector<Touch*>& ts, Event*) { for (auto* t : ts) { if (_intro) _intro->mouseDown(t->getLocation()); else _hud->touchDown(t->getID(), t->getLocation()); } };
    tl->onTouchesMoved = [this](const std::vector<Touch*>& ts, Event*) { for (auto* t : ts) _hud->touchMove(t->getID(), t->getLocation()); };
    tl->onTouchesEnded = [this](const std::vector<Touch*>& ts, Event*) { for (auto* t : ts) _hud->touchUp(t->getID()); };
    tl->onTouchesCancelled = tl->onTouchesEnded; _eventDispatcher->addEventListenerWithSceneGraphPriority(tl, this); }
  auto kl = EventListenerKeyboard::create();
  auto setKey = [this](EventKeyboard::KeyCode k, bool d) {
    using K = EventKeyboard::KeyCode; auto& ks = _game.keys;
    switch (k) {
      case K::KEY_W: case K::KEY_UP_ARROW: ks.up = d; break;
      case K::KEY_S: case K::KEY_DOWN_ARROW: ks.down = d; break;
      case K::KEY_A: case K::KEY_LEFT_ARROW: ks.left = d; break;
      case K::KEY_D: case K::KEY_RIGHT_ARROW: ks.right = d; break;
      case K::KEY_SPACE: if (d) _game.jump(); break;
      case K::KEY_T: if (d) _game.doTrick(); break;
      case K::KEY_C: if (d) _game.cycleCam(); break;
      case K::KEY_F3: if (d) _game.settings.showFps = !_game.settings.showFps; break;
      case K::KEY_H: if (d) _game.doHonk(); break;
      case K::KEY_B: if (d) _game.ringBell(); break;
      case K::KEY_P: if (d) { _game.paused = !_game.paused; _hud->toast(_game.paused ? "已暂停" : "继续骑行", "按 P 切换", _game.paused ? "pause" : "play"); } break;
      case K::KEY_U: if (d) _hud->toggleUi(); break;
      case K::KEY_N: if (d) _game.settings.hour = std::fmod(_game.settings.hour + 3, 24.f); break;
      case K::KEY_Q: if (d) { _game.S.gear = std::max(0, _game.S.gear - 1); _game.S.lastGearManual = _game.S.time; } break;
      case K::KEY_E: if (d) { _game.S.gear = std::min(4, _game.S.gear + 1); _game.S.lastGearManual = _game.S.time; } break;
      case K::KEY_K: if (d) _hud->wantsScreenshot = true; break;
      case K::KEY_M: if (d) _game.settings.music = !_game.settings.music; break;
      case K::KEY_F: if (d) _hud->wantsFullscreen = true; break;
      case K::KEY_1: case K::KEY_2: case K::KEY_3: case K::KEY_4: case K::KEY_5: if (d) _game.setCamMode(int(k) - int(K::KEY_1)); break;
      default: break;
    }
  };
  kl->onKeyPressed = [setKey](EventKeyboard::KeyCode k, Event*) { setKey(k, true); };
  kl->onKeyReleased = [setKey](EventKeyboard::KeyCode k, Event*) { setKey(k, false); };
  _eventDispatcher->addEventListenerWithSceneGraphPriority(kl, this);
  auto ml = EventListenerMouse::create();
  ml->onMouseDown = [this](EventMouse* e) { _mouseSeen = true; Vec2 p = e->getLocation(); AXLOGI("[pb2] mdown {} {} loc {} {}", p.x, p.y, e->getLocation().x, e->getLocation().y); if (_game.started) plat::resumeAudio(); if (_intro && _intro->mouseDown(p)) return true; if (_hud->touchDown(-1, p)) { _padDown = true; return true; } if (_hud->mouseDown(p)) { _hudDrag = true; return true; } _down = true; _dragged = false; _downP = _lastP = p;  return true; };
  ml->onMouseMove = [this](EventMouse* e) { Vec2 p = e->getLocation(); if (_hudDrag) { _hud->mouseMove(p); return true; } if (!_down) return true;
    if (!_dragged && p.distance(_downP) > 10) { _dragged = true; if (_game.camMode != 0) _game.setCamMode(0); }
    if (_dragged) _game.orbitDrag(p.x - _lastP.x, -(p.y - _lastP.y)); _lastP = p;  return true; };
  ml->onMouseUp = [this](EventMouse* e) { if (_padDown) { _padDown = false; _hud->touchUp(-1); return true; } if (_hudDrag) { _hud->mouseUp(); _hudDrag = false; return true; }
    if (_down && !_dragged) { auto sz = Director::getInstance()->getVisibleSize(); Vec2 p = e->getLocation(); _game.clickAt(p.x / sz.width * 2 - 1, p.y / sz.height * 2 - 1); } _down = false;  return true; };
  ml->onMouseScroll = [this](EventMouse* e) { if (_game.camMode != 0) _game.setCamMode(0); _game.orbitZoom(-e->getScrollY());  return true; };
  _eventDispatcher->addEventListenerWithSceneGraphPriority(ml, this);
  scheduleUpdate();
  return true;
}

void MainScene::update(float dt) {
  float sdt = _fixedDt > 0 ? _fixedDt : dt;
  { // PB2_FX="jump@3.4,confetti@3.7": scripted actions at sim time (matches js_shot_t.mjs FX)
    static std::vector<std::pair<std::string, float>> fx; static bool init = false;
    if (!init) { init = true; if (auto v = env("PB2_FX")) { std::string a = v; size_t p = 0; while (p < a.size()) { size_t e = a.find(',', p); if (e == std::string::npos) e = a.size();
          std::string it = a.substr(p, e - p); size_t at = it.find('@'); if (at != std::string::npos) fx.push_back({it.substr(0, at), float(std::atof(it.c_str() + at + 1))}); p = e + 1; } } }
    for (auto it = fx.begin(); it != fx.end();) if (_game.S.t >= it->second) { _game.fxTrigger(it->first); it = fx.erase(it); } else ++it; }
  if (_holdAt > 0 && _game.S.t >= _holdAt) { if (_shotFrame > 0 && !_held) { _shotFrame = _frameCount + 10; AXLOGI("[pb2] pose {}", _game.poseDebug()); } _held = true; _game.update(0); } else _game.update(sdt);
  _acc += dt; _frames++; _sumUpd += _game.cpuUpdateMs; _sumRen += _game.cpuRenderMs; _sumGpu += _game.gpuMs;
  if (_acc >= 0.5f) {
    _fps = _frames / _acc;
    if (_adaptive) _game.adaptQuality(_fps);
    auto& st = _game.lastStats;
    char buf[256];
    snprintf(buf, sizeof(buf), "%.0f FPS  %.2f ms  scale %.2f\nupdate %.2f ms  render(cpu) %.2f ms  gpu %.2f ms\n3D draws %d  passes %d  tris %.0fk",
             _fps, 1000.f * _acc / _frames, _game.settings.renderScale, _sumUpd / _frames, _sumRen / _frames, _sumGpu / _frames, st.draws, st.passes, st.tris / 1000.f);
    _perf = buf;
    AXLOGI("[pb2] {}", std::string(buf));
    _acc = 0; _frames = 0; _sumUpd = _sumRen = _sumGpu = 0;
  }
  _hud->tick(sdt, _fps, _perf);
  _game.blurRects.clear();
  if (_hud->isVisible()) { float hop = _hud->getOpacity() / 255.f; size_t n0 = _game.blurRects.size(); _hud->gatherBlur(_game.blurRects); for (size_t i = n0; i < _game.blurRects.size(); i++) _game.blurRects[i].amount *= hop; }
  if (_intro) { auto vs = Director::getInstance()->getVisibleSize(); _game.blurRects.push_back({0, 0, vs.width, vs.height, 0, 0.3f * _intro->getDisplayedOpacity() / 255.f}); }  // #intro blur(3px)
  { float tgt = _game.camMode == 3 ? 1.f : 0.f; if (_barT != tgt) { _barT = tgt > _barT ? std::min(tgt, _barT + sdt / 0.8f) : std::max(tgt, _barT - sdt / 0.8f);
      // cubic-bezier(.6,0,.2,1) approx via smoothstep-ish easing
      float u = _barT, e = u * u * (3 - 2 * u); e = e * e * (3 - 2 * e) * 0.5f + e * 0.5f; auto vs = Director::getInstance()->getVisibleSize(); float h = vs.height * 0.09f * e;
      _bars->clear(); if (h > 0.5f) { _bars->drawSolidRect(Vec2(0, 0), Vec2(vs.width, h), Color4F::BLACK); _bars->drawSolidRect(Vec2(0, vs.height - h), Vec2(vs.width, vs.height), Color4F::BLACK); } } }
  if (_intro) { _intro->tick(sdt); if (_intro->gone()) { _intro->removeFromParent(); _intro = nullptr; } }
  if (_hudFade > 0) { _hudFade = std::min(1.f, _hudFade + sdt / 0.8f); _hud->setOpacity(uint8_t(255 * _hudFade)); if (_hudFade >= 1) { _hudFade = 0; _hud->setCascadeOpacityEnabled(false); } }
  if (_holdAt > 0) _startToast = 0;
  if (_startToast > 0 && (_startToast -= sdt) <= 0) _hud->toast("出发！", (getenv("PB2_TOUCH") || plat::coarsePointer()) ? "点屏幕按钮加速、变道、跳跃" : "W/S 加减速 · A/D 变道 · 空格跳 · T 特技", "bird");
  if (_hud->wantsFullscreen) { _hud->wantsFullscreen = false; plat::toggleFullscreen(); }
  if (_hud->wantsShare) { _hud->wantsShare = false; char q[96]; snprintf(q, sizeof(q), "?time=%.2f&cam=%s", _game.settings.hour, _game.camName().c_str());
    std::string u = plat::pageUrl() + q; if (plat::copyText(u)) _hud->toast("已复制分享链接", "打开即回到当前时刻与镜头", "link"); else _hud->toast("分享链接", u, "link"); }
  if (!_hud->wantsUrl.empty()) { plat::openUrl(_hud->wantsUrl); _hud->wantsUrl.clear(); }
  if (_hud->wantsScreenshot) { _hud->wantsScreenshot = false; plat::screenshot([this](bool ok, const std::string& f) { if (ok) _hud->toast("截图已保存", "分享你的鹈鹕骑行瞬间", "cam"); AXLOGI("[pb2] screenshot {} {}", ok, f); }); }
  _frameCount++;
  if (auto a = env("PB2_ACT")) { std::string acts(a); size_t pos = 0;  // e.g. "trick@100,jump@130,settings@10"
    while (pos < acts.size()) { size_t c = acts.find(',', pos); std::string it = acts.substr(pos, c == std::string::npos ? std::string::npos : c - pos); pos = c == std::string::npos ? acts.size() : c + 1;
      size_t at = it.find('@'); if (at == std::string::npos || std::atoi(it.c_str() + at + 1) != _frameCount) continue; std::string k = it.substr(0, at);
      if (k == "trick") _game.doTrick(); else if (k == "jump") _game.jump(); else if (k == "bell") _game.ringBell(); else if (k == "honk") _game.doHonk(); else if (k == "pause") _game.paused = true;
      else if (k == "shot") _hud->wantsScreenshot = true; else if (k == "share") _hud->wantsShare = true; else if (k == "help") _hud->toggleHelp(); } }
  if (_shotFrame > 0 && _frameCount == _shotFrame) { AXLOGI("[pb2] READY frame {}", _frameCount); fflush(stdout); }
  if (_shotFrame > 0 && _frameCount == _shotFrame + 40 && _quitAfterShot) Director::getInstance()->end();
}

void MainScene::visit(Renderer* renderer, const Mat4& parentTransform, uint32_t parentFlags) {
  // one callback command renders the whole 3D frame before Axmol's 2D queue
  static bool no3d = env("PB2_NO3D") != nullptr;
  if (!no3d) renderer->addCallbackCommand([this]() {
    auto fs = Director::getInstance()->getRenderView()->getFrameSize();
    _game.render(int(fs.width), int(fs.height));  // records on the Renderer's backend::CommandBuffer
  }, -9000.f);  // after Director clear (-10000), before 2D (0)
  Scene::visit(renderer, parentTransform, parentFlags);
}
