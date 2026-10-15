#include "Hud.h"
#include "Intro.h"
#include <cmath>
USING_NS_AX;
static const char* FONT = "fonts/NotoSansSC-Regular.ttf";
static const char* FONT_NUM = "fonts/NotoSans-ExtraBold.ttf";  // weight-800 Latin for .gauge .val b / #fishCount (font-weight 800/900)
static void numFont(Label* l, float size) { TTFConfig c(FONT_NUM, size); l->disableEffect(LabelEffect::BOLD); l->setTTFConfig(c); }
static const Color4F GLASS(14 / 255.f, 20 / 255.f, 34 / 255.f, 0.52f), GLASS_S(10 / 255.f, 14 / 255.f, 26 / 255.f, 0.72f);
static const Color4B MUTED(236, 240, 250, 168), ACCENT(255, 181, 71, 255), TEXT(244, 246, 251, 255);
static const char* CAMS[5] = {"自由环绕", "追随", "侧面跟拍", "电影运镜", "鹈鹕视角"};
static const float EMO_PX = 26;  // .toast .emo font-size
static const char* ACH_E[14] = {"fish", "star", "rod", "trophy", "roo", "wing", "circus", "dash", "zap", "road", "map", "moon", "bell", "horn"};
static const char* ACH_T[][3] = {{"fish1", "第一条鱼", "张嘴就是干饭"}, {"golden", "金色传说", "吞下了一条金鱼 (+5)"}, {"fish10", "十鱼到手", "喉囊开始沉甸甸了"},
  {"fish50", "海岸渔王", "累计 50 条鱼"}, {"jump", "起飞！", "第一次跳跃"}, {"trick", "放手特技", "展翅 + 抬前轮"}, {"trick3", "杂技鹈鹕", "完成 3 次特技"},
  {"speed30", "时速 30", "风在耳边呼啸"}, {"speed45", "疾风鹈鹕", "时速突破 45 km/h"}, {"km1", "骑行 1 公里", "海岸线才刚开始"}, {"km5", "骑行 5 公里", "真正的长途鹈鹕"},
  {"night", "夜骑", "星光与路灯作伴"}, {"bell10", "铃铃铃", "按铃 10 次，路人已让开"}, {"honk", "嘎——！", "鹈鹕的呐喊"}};

Label* Hud::label(Node* parent, const std::string& s, float size, Vec2 pos, Vec2 anchor, Color4B c, bool bold) {
  auto l = Label::createWithTTF(s, FONT, size); l->setAnchorPoint(anchor); l->setPosition(pos); l->setTextColor(c);
  if (bold) l->enableBold(); parent->addChild(l, 2); return l;
}
void Hud::panel(DrawNode* d, Rect r, float rad, Color4F fill, bool stroke) {
  std::vector<Vec2> pts; const int seg = 6;
  Vec2 c[4] = {{r.getMaxX() - rad, r.getMaxY() - rad}, {r.getMinX() + rad, r.getMaxY() - rad}, {r.getMinX() + rad, r.getMinY() + rad}, {r.getMaxX() - rad, r.getMinY() + rad}};
  for (int q = 0; q < 4; q++) for (int i = 0; i <= seg; i++) { float a = (q * 90 + i * 90.f / seg) * M_PI / 180; pts.push_back(c[q] + Vec2(std::cos(a), std::sin(a)) * rad); }
  d->drawSolidPoly(pts.data(), pts.size(), fill);
  if (stroke && fill.a < 0.8f) { if (!_glass.count(d)) d->retain(); _glass[d].push_back({r, rad}); }
  if (stroke) d->drawPoly(pts.data(), pts.size(), true, Color4F(1, 1, 1, 0.14f));
}

bool Hud::init() {
  Node::init(); _sz = Director::getInstance()->getVisibleSize();
  _bg = DrawNode::create(); addChild(_bg, 0); _bgDim = DrawNode::create(); addChild(_bgDim, 0); _dyn = DrawNode::create(); addChild(_dyn, 1); _static = Node::create(); addChild(_static, 2); _staticDim = Node::create(); _staticDim->setCascadeOpacityEnabled(true); addChild(_staticDim, 2); _dynDim = DrawNode::create(); addChild(_dynDim, 1);
  float W = _sz.width, H = _sz.height;
  // dash; ≤900px: .gauge 96px, val 28px, .stats 11.5px/13px gap 3px 10px (index.html @media)
  _narrow = W <= 900; const float G = _narrow ? 96 : 124, gc = 16 + 12 + G / 2, sw = _narrow ? 78 : 112, dy = _narrow ? 19.5f : 24, vf = _narrow ? 13 : 15;
  _speedVal = label(this, "0", _narrow ? 28 : 36, {gc, gc + 4 * G / 124}, {0.5f, 0.5f}, TEXT, true); numFont(_speedVal, _narrow ? 28 : 36);
  _dimStatic.push_back(label(this, "KM/H", _narrow ? 10 : 11, {gc, gc - 18 * G / 124}, {0.5f, 0.5f}, MUTED));
  float sx = 16 + 12 + G + 14, sy = gc + 1.5f * dy;
  const char* names[4] = {"踏频", "档位", "里程", "镜头"};
  for (int i = 0; i < 4; i++) _dimStatic.push_back(label(this, names[i], _narrow ? 11.5f : 12.5f, {sx, sy - i * dy}, {0, 0.5f}, MUTED));
  // <b>0 <small muted 400>rpm</small></b>: unit at .833em, value right-aligned before it
  { auto u1 = label(this, "rpm", vf * 0.833f, {sx + sw, sy - 1}, {1, 0.5f}, MUTED); auto u2 = label(this, "km", vf * 0.833f, {sx + sw, sy - 2 * dy - 1}, {1, 0.5f}, MUTED);
    _dimStatic.push_back(u1); _dimStatic.push_back(u2); _cadX = sx + sw - u1->getContentSize().width - vf * 0.3f; _distX = sx + sw - u2->getContentSize().width - vf * 0.3f; }
  _cad = label(this, "0", vf, {_cadX, sy}, {1, 0.5f}, TEXT, true);
  _gear = label(this, "3/5", vf, {sx + sw, sy - dy}, {1, 0.5f}, TEXT, true);
  _dist = label(this, "0.00", vf, {_distX, sy - 2 * dy}, {1, 0.5f}, TEXT, true);
  _camL = label(this, "", _narrow ? 12 : 13, {sx + sw, sy - 3 * dy}, {1, 0.5f}, TEXT, true);
  _fishL = label(this, "0", 34, {W - 16 - 16 - 186, 16 + 12 + 66}, {0, 0.5f}, ACCENT, true); numFont(_fishL, 34);
  _achL = label(this, "0/14", 14, {W - 32, 16 + 12 + 60}, {1, 0.5f}, TEXT, true);
  _clock = label(this, "17:18", 14, {W - 32 - 38, 16 + 12 + 16}, {1, 0.5f}, TEXT, true);
  _fps = label(this, "", 11, {W - 26, H - 76}, {1, 1}, MUTED); _fps->setVisible(false);
  _pause = label(this, "已暂停 · 按 P 继续", 22, {W / 2, H / 2}, {0.5f, 0.5f}, TEXT, true); _pause->setVisible(false);
  _pop = label(this, "", 22, {0, 0}, {0.5f, 0.5f}, ACCENT, true); _pop->setVisible(false); _pop->enableOutline(Color4B(0, 0, 0, 120), 2);
  for (int i = 0; i < 5; i++) _camBtns.push_back(label(this, CAMS[i], 13, {0, 0}, {0.5f, 0.5f}, MUTED));
  _touchD = DrawNode::create(); addChild(_touchD, 5);
  rebuildStatic(); buildSettings(); buildHelp();
  return true;
}

// index.html .tools SVG icons (feather-style 18px strokes), drawn as vectors
static void drawToolIcon(DrawNode* d, int i, Vec2 c) {
  const Color4F k(0.957f, 0.965f, 0.984f, 1); const float w = 0.85f;
  auto L = [&](float a, float b, float e, float f) { d->drawSegment(c + Vec2(a, -b), c + Vec2(e, -f), w, k); };
  auto R = [&](float x0, float y0, float x1, float y1) { L(x0, y0, x1, y0); L(x1, y0, x1, y1); L(x1, y1, x0, y1); L(x0, y1, x0, y0); };
  switch (i) {
    case 0: d->drawSolidRect(c + Vec2(-9, -7), c + Vec2(9, 4), Color4F(0.45f, 0.5f, 0.55f, 1)); d->drawSolidRect(c + Vec2(-4, 4), c + Vec2(4, 7), Color4F(0.45f, 0.5f, 0.55f, 1)); d->drawSolidCircle(c + Vec2(0, -1.5f), 4, 0, 16, Color4F(0.2f, 0.65f, 0.9f, 1)); d->drawSolidCircle(c + Vec2(6, 7), 2.6f, 0, 10, Color4F(1, 0.75f, 0.2f, 1)); break; case 100: R(-9, -4, 9, 7); L(-4, -4, -2, -7); L(-2, -7, 2, -7); L(2, -7, 4, -4); d->drawCircle(c + Vec2(0, -1.5f), 3.6f, 0, 20, false, k, 0.9f); break;           // camera
    case 1: L(-3, 6, -3, -7); L(-3, -7, 7, -9); L(7, -9, 7, 4); d->drawSolidCircle(c + Vec2(-5.5f, -6), 2.8f, 0, 12, k); d->drawSolidCircle(c + Vec2(4.5f, -4), 2.8f, 0, 12, k); break;  // note
    case 2: { Color4F t(0.55f, 0.82f, 0.9f, 1); d->drawCircle(c + Vec2(-3.5f, -3.5f), 4.2f, 0, 20, false, t, 2.2f); d->drawCircle(c + Vec2(3.5f, 3.5f), 4.2f, 0, 20, false, t, 2.2f); } break;  // link
    case 3: L(-8, -3, -8, -8); L(-8, -8, -3, -8); L(3, -8, 8, -8); L(8, -8, 8, -3); L(8, 3, 8, 8); L(8, 8, 3, 8); L(-3, 8, -8, 8); L(-8, 8, -8, 3); break;  // fullscreen
    case 4: { Color4F t(0.5f, 0.75f, 0.9f, 1); d->drawSolidCircle(c, 6.5f, 0, 24, t); d->drawSolidCircle(c, 2.6f, 0, 12, Color4F(0.25f, 0.3f, 0.42f, 1)); for (int j = 0; j < 8; j++) { float a = j * M_PI / 4; d->drawSegment(c + Vec2(std::cos(a), std::sin(a)) * 5.5f, c + Vec2(std::cos(a), std::sin(a)) * 8.5f, 1.8f, t); } } break;
    case 40: for (int j = 0; j < 8; j++) { float a = j * M_PI / 4; d->drawSegment(c + Vec2(std::cos(a), std::sin(a)) * 5.5f, c + Vec2(std::cos(a), std::sin(a)) * 8.5f, 1.4f, k); }
      d->drawCircle(c, 6.2f, 0, 24, false, k, 0.9f); break;  // gear
    case 5: break; case 50:  { Vec2 p; for (int j = 0; j <= 10; j++) { float a = M_PI * (1.1f - j * 0.15f); Vec2 q = c + Vec2(std::cos(a) * 2.8f, 2 + std::sin(a) * 2.8f); if (j) d->drawSegment(p, q, w, k); p = q; } }
      L(0.6f, 1.2f, 0, 1.2f); L(0, -1, 0, 1.2f); d->drawSolidCircle(c + Vec2(0, -4.5f), 1.1f, 0, 8, k); break;  // help
    case 6: { d->drawSolidCircle(c, 9, 0, 28, k); d->drawSolidCircle(c + Vec2(0, 0.5f), 4.6f, 0, 20, Color4F(0.11f, 0.13f, 0.2f, 1)); d->drawSolidCircle(c + Vec2(0, -6.5f), 2.4f, 0, 10, Color4F(0.11f, 0.13f, 0.2f, 1));
      Vec2 e1[3] = {c + Vec2(-4.6f, 1.5f), c + Vec2(-4.2f, 6.5f), c + Vec2(-1.5f, 4)}; Vec2 e2[3] = {c + Vec2(4.6f, 1.5f), c + Vec2(4.2f, 6.5f), c + Vec2(1.5f, 4)}; d->drawSolidPoly(e1, 3, Color4F(0.11f, 0.13f, 0.2f, 1)); d->drawSolidPoly(e2, 3, Color4F(0.11f, 0.13f, 0.2f, 1)); } break;  // github-ish
  }
}

void Hud::rebuildStatic() {
  _static->removeAllChildren(); _staticDim->removeAllChildren(); _btns.clear();  // keep an active slider drag across rebuilds (drag holds a copy, not a pointer)
  _bg->clear(); for (auto* n : {_bg, _bgDim}) { auto g = _glass.find(n); if (g != _glass.end()) g->second.clear(); } float W = _sz.width, H = _sz.height;
  // brand
  const bool narrow = W <= 900;  // index.html @media (max-width: 900px)
  panel(_bg, Rect(16, H - 14 - 50, narrow ? 146 : 200, 50), 16, GLASS);
  { // 🐦 logo (Noto-style blue bird facing left)
    const Color4F blue(0.30f, 0.62f, 0.95f, 1), dark(0.16f, 0.42f, 0.78f, 1), org(1, 0.62f, 0.15f, 1); Vec2 o(40, H - 39);
    Vec2 tail[3] = {o + Vec2(6, -2), o + Vec2(14, -8), o + Vec2(13, 1)}; _bg->drawSolidPoly(tail, 3, dark);
    _bg->drawSolidCircle(o + Vec2(1, -2), 8.5f, 0, 24, blue); _bg->drawSolidCircle(o + Vec2(-4, 5), 6.5f, 0, 20, blue);
    Vec2 wing[3] = {o + Vec2(-2, -1), o + Vec2(9, 1), o + Vec2(5, -7)}; _bg->drawSolidPoly(wing, 3, dark);
    Vec2 beak[3] = {o + Vec2(-9, 7), o + Vec2(-15, 4), o + Vec2(-9, 3)}; _bg->drawSolidPoly(beak, 3, org);
    _bg->drawSolidCircle(o + Vec2(-5, 7), 1.5f, 0, 8, Color4F(0.05f, 0.05f, 0.1f, 1));
    _bg->drawSegment(o + Vec2(-1, -10), o + Vec2(-1, -12), 1, org); _bg->drawSegment(o + Vec2(3, -10), o + Vec2(3, -12), 1, org); }
  label(_static, "鹈鹕骑单车", 15, {68, narrow ? H - 39 : H - 31}, {0, 0.5f}, TEXT, true);
  if (!narrow) label(_static, "Pelican on a Bike · 实时 3D", 11, {68, H - 50}, {0, 0.5f}, MUTED);  // .brand small hidden ≤900
  // cam chips (center top)
  std::vector<float> ws; float tot = 10; const float cfs = narrow ? 12 : 13; for (auto* c : CAMS) { float w = Label::createWithTTF(c, FONT, cfs)->getContentSize().width + (narrow ? 18 : 24); ws.push_back(w); tot += w + 4; } tot -= 4;
  float x = W / 2 - tot / 2, cy = H - 14 - 22; if (narrow) { x = 16; cy -= 58 - 2; } panel(_bg, narrow ? Rect(16, cy - 20, W - 32, 40) : Rect(x, cy - 22, tot, 44), 16, GLASS); x += 5;
  for (int i = 0; i < 5; i++) { Rect r(x, cy - (narrow ? 15 : 16), ws[i], narrow ? 30 : 32); _camBtns[i]->setScale(cfs / 13); if (_g->camMode == i) panel(_bg, r, 10, Color4F(1, 0.75f, 0.42f, 1), false);
    _camBtns[i]->setPosition(r.getMidX(), r.getMidY()); _camBtns[i]->setTextColor(_g->camMode == i ? Color4B(26, 18, 6, 255) : MUTED);
    if (_g->camMode == i) _camBtns[i]->enableBold(); else _camBtns[i]->disableEffect(LabelEffect::BOLD);
    _btns.push_back({r, [this, i]() { _g->setCamMode(i); }}); x += ws[i] + 4; }
  // tools (top right): 截图 音乐 分享 全屏 参数 帮助 GitHub
  const int NT = 7; float tw = NT * 44 + 8; Rect tp(W - 16 - tw, H - 14 - 50, tw, 50); panel(_bg, tp, 16, GLASS);
  for (int i = 0; i < NT; i++) { Rect r(tp.getMinX() + 6 + i * 44, tp.getMinY() + 6, 38, 38);
    if ((i == 4 && _showSettings) || (i == 5 && _showHelp)) panel(_bg, r, 11, Color4F(63 / 255.f, 208 / 255.f, 201 / 255.f, 0.22f), false);
    drawToolIcon(_bg, i, Vec2(r.getMidX(), r.getMidY())); if (i == 5) label(_static, "?", 16, {r.getMidX(), r.getMidY()}, {0.5f, 0.5f}, TEXT, true);
    if (i == 1 && !_g->settings.music) _bg->drawSegment(Vec2(r.getMidX() - 9, r.getMidY() - 9), Vec2(r.getMidX() + 9, r.getMidY() + 9), 1, Color4F(1, 90 / 255.f, 110 / 255.f, 1));
    _btns.push_back({r, [this, i]() { switch (i) { case 0: wantsScreenshot = true; break; case 1: _g->settings.music = !_g->settings.music; _dirty = true; break; case 2: wantsShare = true; break;
      case 3: wantsFullscreen = true; break; case 4: toggleSettings(); break; case 5: toggleHelp(); break; default: wantsUrl = "https://github.com/tkzcfc/PelicanBike"; } }}); }
  if (_g->settings.showFps) panel(_bg, Rect(W - 18 - 250, H - 72 - 62, 250, 62), 10, GLASS);
  // dash (dimmed to .35 in cine like body.cinematic)
  _bgDim->clear(); { auto g = _glass.find(_bgDim); if (g != _glass.end()) g->second.clear(); } panel(_bgDim, _narrow ? Rect(16, 16, 12 + 96 + 14 + 78 + 16, 120) : Rect(16, 16, 12 + 124 + 14 + 112 + 16, 148), 16, GLASS);
  // hints
  { std::vector<std::string> T = {"\x01W", "/", "\x01S", " 加速刹车   ", "\x01" "A", "/", "\x01" "D", " 变道   ", "\x01空格", " 跳   ", "\x01T", " 特技   ", "\x01" "B", " 铃   ", "\x01" "C", " 镜头   ", "\x01U", " 隐藏界面"};
    _hints = nullptr;
    if (W > 1180 && !_touchOn) { float hw = layoutKbdLine(_staticDim, _bgDim, T, 0, 0, 12, MUTED, true) + 28; panel(_bgDim, Rect(W / 2 - hw / 2, 16, hw, 36), 16, GLASS);
      layoutKbdLine(_staticDim, _bgDim, T, W / 2, 16 + 18, 12, MUTED); } }
  // score
  Rect sp = narrow ? Rect(W - 16 - 126, 16, 126, 60) : Rect(W - 16 - 281, 16, 281, 104); panel(_bgDim, sp, 16, GLASS);  // .score: padding 12/16; timebar = icon + 129px range + 44px clock + 30px btn, gaps 8
  const float fy = narrow ? 16 + 30 : 16 + 12 + 66, px = narrow ? 12 : 16;  // .score padding 10/12 when narrow, .timebar hidden
  emoSprite(_staticDim, "fish", 28, Vec2(sp.getMinX() + px + 13, fy));  // .fish .ico 26px
  _fishL->setPosition(sp.getMinX() + px + 34, fy);
  label(_staticDim, "成就", 12, {sp.getMaxX() - px, fy + 12}, {1, 0.5f}, MUTED); _achL->setPosition(sp.getMaxX() - px, fy - 6); _clock->setVisible(!narrow);
  if (!narrow) {
  _timeTrack = Rect(sp.getMinX() + 44, 16 + 12 + 7, 129, 16);
  { // #sunIcon: 🌙 night>0.5, 🌅 elev<8, else ☀️ (main.js updateHud)
    _sunIco = _g->nightV() > 0.5f ? 2 : _g->elevV() < 8 ? 1 : 0; Vec2 c(sp.getMinX() + 27, 16 + 12 + 15); Color4F Y(1, 0.8f, 0.2f, 1), O(1, 0.55f, 0.15f, 1);
    emoSprite(_staticDim, _sunIco == 0 ? "sun" : _sunIco == 1 ? "sunset" : "moon", 17, c); (void)Y; (void)O; }
  Rect fb(sp.getMaxX() - 16 - 30, 16 + 12 + 1, 30, 30); if (_g->settings.timeFlow) panel(_bgDim, fb, 9, Color4F(63 / 255.f, 208 / 255.f, 201 / 255.f, 0.22f), false);
  emoSprite(_staticDim, "flow", 15, Vec2(fb.getMidX(), fb.getMidY()));  // ⏯ at 13px
  _btns.push_back({fb, [this]() { _g->settings.timeFlow = !_g->settings.timeFlow; _dirty = true; }});
  _sliders.erase(std::remove_if(_sliders.begin(), _sliders.end(), [this](const Slider& s) { return !s.set && s.v == &_g->settings.hour; }), _sliders.end());
  _sliders.push_back({Rect(_timeTrack.getMinX() - 6, _timeTrack.getMinY() - 6, _timeTrack.size.width + 12, _timeTrack.size.height + 12), 0, 24, 0.01f, &_g->settings.hour, false, _timeTrack.getMinX() + 4, _timeTrack.getMaxX() - 4}); } else _timeTrack = Rect();
  if (_showSettings && _settings) _settings->setVisible(true); else if (_settings) _settings->setVisible(false);
  if (_help) _help->setVisible(_showHelp);
  _dirty = false; _lastCam = _g->camMode;
}

struct Row { const char* name; float* v; float lo, hi, step; bool* b; int* e; int ne; };
void Hud::buildSettings() {
  auto& s = _g->settings; float W = _sz.width, H = _sz.height;
  std::vector<Row> rows = {{"— 骑行", nullptr}, {"巡航速度 m/s", &s.cruise, 0, 14, 0.5f}, {"自动驾驶（追鱼）", nullptr, 0, 0, 0, &s.autopilot}, {"自动变速", nullptr, 0, 0, 0, &s.autoGear},
    {"— 时间与天气", nullptr}, {"时刻", &s.hour, 0, 24, 0.01f}, {"时间流动", nullptr, 0, 0, 0, &s.timeFlow}, {"流速 (时/秒)", &s.dayRate, 0, 0.5f, 0.005f}, {"云量", &s.cloud, 0, 0.9f, 0.01f},
    {"浪高", &s.waveAmp, 0, 2.2f, 0.05f}, {"雾浓度", &s.fog, 0.6f, 3, 0.05f}, {"— 画面", nullptr}, {"画质", nullptr, 0, 0, 0, nullptr, &s.quality, 4}, {"泛光", nullptr, 0, 0, 0, &s.bloom}, {"泛光强度", &s.bloomBoost, 0, 3, 0.05f},
    {"曝光", &s.exposure, 0.4f, 2, 0.01f}, {"显示帧率", nullptr, 0, 0, 0, &s.showFps}, {"— 鹈鹕", nullptr}, {"头盔", nullptr, 0, 0, 0, &s.helmet}, {"墨镜", nullptr, 0, 0, 0, nullptr, &s.glasses, 3},
    {"围巾", nullptr, 0, 0, 0, &s.scarf}, {"— 声音", nullptr}, {"主音量", &s.volume, 0, 1, 0.01f}, {"生成式音乐", nullptr, 0, 0, 0, &s.music}, {"音乐音量", &s.musicVolume, 0, 1, 0.01f}};
  // lil-gui folders: 骑行 open, others closed (main.js: f.close() for f2..f5); headers toggle
  bool wasVisible = _settings && _settings->isVisible();
  if (_settings) { _settings->removeFromParent(); _settings = nullptr; }
  _settingsBtns.clear(); _sliders.erase(std::remove_if(_sliders.begin(), _sliders.end(), [](const Slider& q) { return q.set; }), _sliders.end());
  { std::vector<Row> vis; int fi = -1; for (auto& r : rows) { bool hdr = !r.v && !r.b && !r.e; if (hdr) fi++; if (hdr || (_openMask >> fi & 1)) { vis.push_back(r); if (hdr) vis.back().ne = fi; } } rows = vis; }
  float rw = 300, rh = 24, ph = rows.size() * rh + 44, x0 = W - 16 - rw, y0 = H - 76 - ph;
  _settings = Node::create(); addChild(_settings, 5); auto d = DrawNode::create(); _settings->addChild(d); panel(d, Rect(x0, y0, rw, ph), 12, GLASS_S);
  label(_settings, "参数面板", 14, {x0 + 12, y0 + ph - 18}, {0, 0.5f}, TEXT, true);
  for (size_t i = 0; i < rows.size(); i++) { auto& r = rows[i]; float y = y0 + ph - 44 - i * rh;
    if (!r.v && !r.b && !r.e) { int fi = r.ne; bool open = _openMask >> fi & 1; label(_settings, std::string(open ? "- " : "+ ") + (r.name + 4), 12, {x0 + 12, y}, {0, 0.5f}, ACCENT, true);
      _settingsBtns.push_back({Rect(x0, y - 11, rw, 22), [this, fi]() { _openMask ^= 1u << fi; _settingsRebuild = true; }}); continue; }
    label(_settings, r.name, 12, {x0 + 12, y}, {0, 0.5f}, MUTED);
    Rect hit(x0 + 128, y - 10, rw - 128 - 12, 20);
    if (r.v) { float tx0 = x0 + 128, tw = rw - 128 - 12 - 52;  // drawn track width (value label sits in the last 52px)
      _sliders.push_back({Rect(tx0 - 6, y - 11, tw + 12, 22), r.lo, r.hi, r.step, r.v, true, tx0, tx0 + tw}); }
    else if (r.b) { bool* b = r.b; _settingsBtns.push_back({hit, [this, b]() { *b = !*b; _dirty = true; }}); }
    else { int* e = r.e; int ne = r.ne; _settingsBtns.push_back({hit, [this, e, ne]() { *e = (*e + 1) % ne; if (ne == 4) _g->applyQuality(*e); }}); }
  }
  _settingsRect = Rect(x0, y0, rw, ph);
  _settingsDyn = DrawNode::create(); _settings->addChild(_settingsDyn, 1);
  _settingsVal = Node::create(); _settings->addChild(_settingsVal, 2); _rows = rows.size(); _rowsY0 = y0 + ph - 44; _rowH = rh;
  _settingsRows.clear(); for (auto& r : rows) _settingsRows.push_back({r.v, r.b, r.e, r.ne});
  _settings->setVisible(wasVisible);
}
void Hud::buildHelp() {
  float W = _sz.width, H = _sz.height; _help = Node::create(); addChild(_help, 6); auto d = DrawNode::create(); _help->addChild(d);
  d->drawSolidRect(Vec2(0, 0), Vec2(W, H), Color4F(0, 0, 0, 0.35f)); Rect p(W / 2 - 280, H / 2 - 170, 560, 340); panel(d, p, 16, GLASS_S);
  label(_help, "操作说明", 18, {p.getMinX() + 24, p.getMaxY() - 30}, {0, 0.5f}, TEXT, true);
  const char* items[] = {"W/↑ 加速", "S/↓ 刹车（滑行有棘轮声）", "A/D 左右变道抓鱼", "空格 跳跃", "T 放手展翅 + 抬前轮", "B 按车铃", "H 鹈鹕大叫", "C 切换镜头", "Q/E 手动换挡", "N 时间快进 3 小时",
                         "K 截图 · F 全屏", "U 隐藏界面", "P 暂停", "拖动鼠标环绕 · 滚轮缩放"};
  for (int i = 0; i < 14; i++) label(_help, items[i], 13, {p.getMinX() + 24 + (i % 2) * 270, p.getMaxY() - 64 - (i / 2) * 26}, {0, 0.5f}, MUTED);
  label(_help, "M 音乐开关 · 1-5 直接切换镜头", 12.5f, {p.getMinX() + 24, p.getMinY() + 50}, {0, 0.5f}, MUTED);
  label(_help, "点击鹈鹕 / 车铃 / 海面都有惊喜 · 点击任意处关闭", 12.5f, {p.getMinX() + 24, p.getMinY() + 26}, {0, 0.5f}, MUTED);
  _help->setVisible(false);
}

void Hud::emoSprite(Node* parent, const std::string& name, float px, Vec2 pos) {
  if (auto sp = Sprite::create("emoji/" + name + ".png")) { sp->setScale(px / sp->getContentSize().width); sp->setPosition(pos); parent->addChild(sp); }
}

void Hud::toast(const std::string& title, const std::string& sub, const std::string& emo) {
  auto n = Node::create(); addChild(n, 8); auto d = DrawNode::create(); n->addChild(d);
  float tx = 16; if (!emo.empty()) if (auto sp = Sprite::create("emoji/" + emo + ".png")) { sp->setScale(EMO_PX / sp->getContentSize().width); sp->setPosition(12 + EMO_PX / 2, 10); n->addChild(sp); tx = 12 + EMO_PX + 10; }
  auto t = label(n, title, 14, {tx, 12}, {0, 0}, TEXT, true); auto s = label(n, sub, 12, {tx, -8}, {0, 0}, MUTED);
  float w = std::max(t->getContentSize().width, s->getContentSize().width) + tx + 16;
  panel(d, Rect(0, -16, w, 52), 14, GLASS_S, false); d->drawPoly(std::vector<Vec2>{{0, -16}, {w, -16}, {w, 36}, {0, 36}}.data(), 4, true, Color4F(1, 210 / 255.f, 122 / 255.f, 0.35f));
  n->setContentSize(Size(w, 52)); n->setCascadeOpacityEnabled(true); for (auto c : n->getChildren()) c->setCascadeOpacityEnabled(true);
  _toasts.push_back({n, 0});
}

bool Hud::mouseDown(Vec2 p) {
  if (_uiHidden) return false;
  if (_showHelp) { _showHelp = false; _dirty = true; return true; }
  for (auto& s : _sliders) { if (s.set && !_showSettings) continue;
    if (s.r.containsPoint(p)) { _drag = s; _dragging = true; mouseMove(p); if (s.v == &_g->settings.hour) _g->settings.timeFlow = false, _dirty = true; return true; } }
  for (auto& b : _btns) if (b.r.containsPoint(p)) { b.fn(); return true; }
  if (_showSettings) { for (auto& b : _settingsBtns) if (b.r.containsPoint(p)) { b.fn(); return true; } if (_settingsRect.containsPoint(p)) return true; }
  return false;
}
void Hud::mouseMove(Vec2 p) {
  if (!_dragging) return; float u = std::clamp((p.x - _drag.tx0) / std::max(1.f, _drag.tx1 - _drag.tx0), 0.f, 1.f);
  float v = _drag.lo + u * (_drag.hi - _drag.lo); *_drag.v = std::clamp(std::round(v / _drag.step) * _drag.step, _drag.lo, _drag.hi);
}
void Hud::mouseUp() { _dragging = false; _dragTouch = -2; }

void Hud::tick(float dt, float fps, const std::string& perf) {
  auto& S = _g->S; float W = _sz.width, H = _sz.height;
  if (_settingsRebuild) { _settingsRebuild = false; _dragging = false; buildSettings(); }
  if (_dirty || _lastCam != _g->camMode || _sunIco != (_g->nightV() > 0.5f ? 2 : _g->elevV() < 8 ? 1 : 0)) rebuildStatic();
  drawTouch();
  // gauge arc (270°, from 225° clockwise), gradient #3fd0c9 -> #ffc27a -> #ff5a6e
  _dyn->clear(); _dynDim->clear(); const float G = _narrow ? 96 : 124; Vec2 c(16 + 12 + G / 2, 16 + 12 + G / 2); float r = 52 * G / 124, kmh = S.speed * 3.6f, k = std::clamp(kmh / 50.f, 0.f, 1.f);
  auto arc = [&](float from, float to, bool grad, float alpha) { int n = std::max(2, int((to - from) * 60)); Vec2 prev, pd; Color4F endCol(1, 1, 1, alpha);
    for (int i = 0; i <= n; i++) { float u = from + (to - from) * i / n; float a = (225 - u * 270) * M_PI / 180; Vec2 p = c + Vec2(std::cos(a), std::sin(a)) * r;
      Color4F col(1, 1, 1, alpha); if (grad) { Color4F a0(63 / 255.f, 208 / 255.f, 201 / 255.f, 1), a1(1, 194 / 255.f, 122 / 255.f, 1), a2(1, 90 / 255.f, 110 / 255.f, 1);
        float t = std::clamp((c.x + r - p.x) / (2 * r), 0.f, 1.f); col = t < 0.6f ? Color4F(a0.r + (a1.r - a0.r) * t / 0.6f, a0.g + (a1.g - a0.g) * t / 0.6f, a0.b + (a1.b - a0.b) * t / 0.6f, 1)
                                  : Color4F(a1.r + (a2.r - a1.r) * (t - 0.6f) / 0.4f, a1.g + (a2.g - a1.g) * (t - 0.6f) / 0.4f, a1.b + (a2.b - a1.b) * (t - 0.6f) / 0.4f, 1); }
      // non-overlapping quad strip (stroke-width 9, no alpha beading at joints) + round caps
      Vec2 dir = (p - c).getNormalized(); float hw = 4.5f * G / 124;
      if (i) { Vec2 q[4] = {prev - pd * hw, prev + pd * hw, p + dir * hw, p - dir * hw}; _dynDim->drawSolidPoly(q, 4, col); } prev = p; pd = dir; if (i == n) endCol = col; }
    auto cap = [&](float u, Color4F cc) { float a = (225 - u * 270) * M_PI / 180; Vec2 e = c + Vec2(std::cos(a), std::sin(a)) * r; Vec2 t(std::sin(a), -std::cos(a));
      float hw = 4.5f * G / 124; std::vector<Vec2> h; for (int j = 0; j <= 8; j++) { float b = j * M_PI / 8; h.push_back(e + Vec2(std::cos(a), std::sin(a)) * (hw * std::cos(b)) + t * (hw * std::sin(b)) * (u > from ? 1.f : -1.f)); }
      _dynDim->drawSolidPoly(h.data(), h.size(), cc); };
    cap(from, grad ? Color4F(63 / 255.f, 208 / 255.f, 201 / 255.f, 1) : Color4F(1, 1, 1, alpha)); cap(to, endCol); };
  arc(0, 1, false, 0.12f); if (k > 0.005f) arc(0, k, true, 1);
  char b[64]; snprintf(b, 64, "%.0f", kmh); _speedVal->setString(b);
  snprintf(b, 64, "%.0f", S.cadence); _cad->setString(b); snprintf(b, 64, "%d/5", S.gear + 1); _gear->setString(b);
  snprintf(b, 64, "%.2f", S.distance / 1000.0); _dist->setString(b); _camL->setString(CAMS[std::clamp(_g->camMode, 0, 4)]);
  snprintf(b, 64, "%d", S.fish); _fishL->setString(b);
  if (S.fish != _lastFish) { _bump = 0.45f; _lastFish = S.fish; }
  if (_bump > 0) { _bump -= dt; float u = 1 - _bump / 0.45f; float sc = 1 + 0.35f * std::sin(std::min(1.f, u / 0.6f) * M_PI); _fishL->setScale(sc); } else _fishL->setScale(1);
  snprintf(b, 64, "%d/14", int(_g->achieved.size())); _achL->setString(b);
  float hr = _g->settings.hour; int hh = int(hr) % 24, mm = int((hr - int(hr)) * 60); snprintf(b, 64, "%02d:%02d", hh, mm); _clock->setString(b);
  // time slider
  // Chrome <input type=range> with accent-color: 8px rounded track (filled accent / grey with light rim), 16px thumb
  if (_timeTrack.size.width > 0) {
  Rect t = _timeTrack; float x0 = t.getMinX() + 4, x1 = t.getMaxX() - 4, sy = t.getMidY(), tu = hr / 24, tx = x0 + (x1 - x0) * tu;
  _dynDim->drawSegment(Vec2(x0, sy), Vec2(x1, sy), 4, Color4F(0.62f, 0.62f, 0.64f, 1)); _dynDim->drawSegment(Vec2(x0, sy), Vec2(x1, sy), 3, Color4F(0.2f, 0.2f, 0.22f, 1));
  _dynDim->drawSegment(Vec2(x0, sy), Vec2(tx, sy), 4, Color4F(1, 181 / 255.f, 71 / 255.f, 1));
  _dynDim->drawSolidCircle(Vec2(tx, sy), 8, 0, 20, Color4F(1, 181 / 255.f, 71 / 255.f, 1)); }
  // fps
  _fps->setVisible(_g->settings.showFps); if (_g->settings.showFps) _fps->setString(perf);
  _pause->setVisible(_g->paused);
  // settings values
  if (_showSettings) { _settingsDyn->clear(); _settingsVal->removeAllChildren();
    for (int i = 0; i < _rows; i++) { auto& r = _settingsRows[i]; float y = _rowsY0 - i * _rowH; float x0 = _settingsRect.getMinX() + 128, w = _settingsRect.size.width - 128 - 12;
      if (r.v) { float lo = 0, hi = 1; for (auto& s : _sliders) if (s.v == r.v && s.tx0 == x0) { lo = s.lo; hi = s.hi; }
        float u = std::clamp((*r.v - lo) / (hi - lo), 0.f, 1.f), tw = w - 52;
        _settingsDyn->drawSolidRect(Vec2(x0, y - 7), Vec2(x0 + tw, y + 7), Color4F(1, 1, 1, 0.1f)); _settingsDyn->drawSolidRect(Vec2(x0, y - 7), Vec2(x0 + tw * u, y + 7), Color4F(63 / 255.f, 208 / 255.f, 201 / 255.f, 0.8f));
        char vb[32]; snprintf(vb, 32, hi - lo > 5 ? "%.1f" : "%.2f", *r.v); label(_settingsVal, vb, 11, {x0 + w, y}, {1, 0.5f}, TEXT); }
      else if (r.b) { bool on = *r.b; _settingsDyn->drawSolidRect(Vec2(x0, y - 7), Vec2(x0 + 14, y + 7), on ? Color4F(63 / 255.f, 208 / 255.f, 201 / 255.f, 1) : Color4F(1, 1, 1, 0.15f)); if (on) { _settingsDyn->drawSegment(Vec2(x0 + 3, y), Vec2(x0 + 6, y - 3.5f), 1.2f, Color4F(0.04f, 0.05f, 0.1f, 1)); _settingsDyn->drawSegment(Vec2(x0 + 6, y - 3.5f), Vec2(x0 + 11.5f, y + 4), 1.2f, Color4F(0.04f, 0.05f, 0.1f, 1)); } }
      else if (r.e) { static const char* G[3] = {"自动", "戴上", "摘下"}, *Q[4] = {"流畅", "均衡", "精美", "极致"}; label(_settingsVal, std::string("[ ") + (r.ne == 4 ? Q[*r.e] : G[*r.e]) + " ]", 12, {x0, y}, {0, 0.5f}, TEXT); }
    } }
  { bool cine = _g->camMode == 3; uint8_t op = cine ? 89 : 255; _bgDim->setOpacity(op); _dynDim->setOpacity(op); _staticDim->setOpacity(op);
    for (auto* l : {_speedVal, _cad, _gear, _dist, _camL, _fishL, _achL, _clock}) l->setOpacity(op); if (_hints) _hints->setOpacity(op);
    for (auto* l : _dimStatic) l->setOpacity(op); }
  // cut flash
  if (_g->cutFlash > 0) _dyn->drawSolidRect(Vec2(0, 0), Vec2(W, H), Color4F(0, 0, 0, std::min(1.f, _g->cutFlash)));
  // events -> toasts/pops
  for (auto& id : _g->fxEv.unlocks) for (int k = 0; k < 14; k++) if (id == ACH_T[k][0]) toast(ACH_T[k][1], ACH_T[k][2], ACH_E[k]);
  _g->fxEv.unlocks.clear();
  if (!_g->fxEv.pops.empty()) { auto& pp = _g->fxEv.pops.back(); _pop->setString(pp.first); _pop->setTextColor(pp.second ? Color4B(255, 210, 87, 255) : Color4B(255, 255, 255, 255)); _popT = 0; _g->fxEv.pops.clear(); }
  if (_popT >= 0) { _popT += dt; float sx, sy; if (_popT > 1.1f || !_g->project(_g->fxEv.popWorld, sx, sy)) { _popT = -1; _pop->setVisible(false); }
    else { _pop->setVisible(true); _pop->setPosition((sx * 0.5f + 0.5f) * W, (sy * 0.5f + 0.5f) * H + 20 + _popT * 60); _pop->setOpacity(uint8_t(255 * std::clamp((1.1f - _popT) * 3, 0.f, 1.f))); } }
  float ty = H - 118;
  for (size_t i = 0; i < _toasts.size(); i++) { auto& to = _toasts[i]; to.t += dt; float a = to.t < 0.45f ? to.t / 0.45f : to.t > 3.2f ? std::max(0.f, 1 - (to.t - 3.2f) / 0.5f) : 1;
    to.n->setPosition(W / 2 - to.n->getContentSize().width / 2, ty - 36 + (1 - std::min(1.f, to.t / 0.45f)) * 14); to.n->setOpacity(uint8_t(a * 255)); ty -= 60; }
  while (!_toasts.empty() && _toasts.front().t > 3.7f) { _toasts.front().n->removeFromParent(); _toasts.erase(_toasts.begin()); }
}

// ---- touch pads ----
void Hud::layoutTouch() {
  _tbtns.clear(); float W = _sz.width; const float B = 36, S = 30, G = 10, by = 150;  // bottom: 150px, big 72 / small 60
  _tbtns.push_back({{16 + B, by + B}, B, 0}); _tbtns.push_back({{16 + 3 * B + G, by + B}, B, 1});
  _tbtns.push_back({{W - 16 - 3 * B - G, by + B}, B, 2}); _tbtns.push_back({{W - 16 - B, by + B}, B, 3});
  float ry = by + 2 * B + G + S; _tbtns.push_back({{W - 16 - 5 * S - 2 * G, ry}, S, 4}); _tbtns.push_back({{W - 16 - 3 * S - G, ry}, S, 5}); _tbtns.push_back({{W - 16 - S, ry}, S, 6});
}
void Hud::drawTouch() {
  _touchD->clear(); for (auto* e : _tEmo) if (e) e->setVisible(false); if (!_touchOn || _uiHidden) return; if (_tbtns.empty()) layoutTouch();
  bool held[8] = {}; for (auto& kv : _tptr) if (kv.second >= 0) held[kv.second] = true;
  auto& k = _g->keys; held[0] |= k.left; held[1] |= k.right; held[2] |= k.down; held[3] |= k.up;
  const Color4F ink(1, 1, 1, 0.92f);
  for (auto& b : _tbtns) {
    _touchD->drawSolidCircle(b.c, b.r, 0, 40, held[b.kind] ? Color4F(1, 181 / 255.f, 71 / 255.f, 0.4f) : Color4F(14 / 255.f, 20 / 255.f, 34 / 255.f, 0.52f));
    _touchD->drawCircle(b.c, b.r, 0, 40, false, Color4F(1, 1, 1, 0.14f));
    Vec2 c = b.c; float s = 9;
    switch (b.kind) {
      case 0: { Vec2 t[3] = {c + Vec2(-s, 0), c + Vec2(s * .8f, s), c + Vec2(s * .8f, -s)}; _touchD->drawSolidPoly(t, 3, ink); } break;
      case 1: { Vec2 t[3] = {c + Vec2(s, 0), c + Vec2(-s * .8f, s), c + Vec2(-s * .8f, -s)}; _touchD->drawSolidPoly(t, 3, ink); } break;
      case 2: { Vec2 t[3] = {c + Vec2(0, -s), c + Vec2(s, s * .8f), c + Vec2(-s, s * .8f)}; _touchD->drawSolidPoly(t, 3, ink); } break;
      case 3: { Vec2 t[3] = {c + Vec2(0, s), c + Vec2(s, -s * .8f), c + Vec2(-s, -s * .8f)}; _touchD->drawSolidPoly(t, 3, ink); } break;
      case 4: case 5: { int j = b.kind - 4; if (!_tEmo[j]) { _tEmo[j] = Sprite::create(j ? "emoji/wing.png" : "emoji/bell.png"); addChild(_tEmo[j], 6); }
        _tEmo[j]->setScale(24 / _tEmo[j]->getContentSize().width); _tEmo[j]->setPosition(c); _tEmo[j]->setVisible(true); } break;  // .tbtn font-size 22px emoji
      default: { _touchD->drawSegment(c + Vec2(-8, 9), c + Vec2(8, 9), 1.5f, ink); _touchD->drawSegment(c + Vec2(0, 6), c + Vec2(0, -9), 1.5f, ink);
        _touchD->drawSegment(c + Vec2(0, 6), c + Vec2(-6, 0), 1.5f, ink); _touchD->drawSegment(c + Vec2(0, 6), c + Vec2(6, 0), 1.5f, ink); }
    }
  }
}
void Hud::applyTouchKeys() {
  bool h[4] = {}; for (auto& kv : _tptr) if (kv.second >= 0 && kv.second < 4) h[kv.second] = true;
  auto& k = _g->keys; k.left = h[0]; k.right = h[1]; k.down = h[2]; k.up = h[3];
}
bool Hud::touchDown(int id, Vec2 p) {
  if (id >= 0 && !_uiHidden && isVisible() && !_dragging) for (auto& s : _sliders) { if (s.set && !_showSettings) continue;
    if (s.r.containsPoint(p)) { mouseDown(p); _dragTouch = id; return true; } }
  if (!_touchOn || !isVisible()) return false; if (_tbtns.empty()) layoutTouch();
  for (auto& b : _tbtns) if (p.distance(b.c) <= b.r + 4) {
    if (b.kind >= 4) { double now = utils::gettime(); if (now - _lastAct[b.kind] < 0.25) return true; _lastAct[b.kind] = now;
      if (b.kind == 4) _g->ringBell(); else if (b.kind == 5) _g->doTrick(); else _g->jump(); _tptr[id] = -1 - b.kind; }
    else { _tptr[id] = b.kind; applyTouchKeys(); }
    return true; }
  return false;
}
void Hud::touchMove(int id, Vec2 p) {  // slide between ◀ / ▶ like pointer capture would not; keep simple: re-evaluate held key
  if (_dragging && id == _dragTouch) { mouseMove(p); return; }
  auto it = _tptr.find(id); if (it == _tptr.end() || it->second < 0) return;
  for (auto& b : _tbtns) if (b.kind < 4 && p.distance(b.c) <= b.r + 4) { it->second = b.kind; applyTouchKeys(); return; }
}
void Hud::touchUp(int id) { if (_dragging && id == _dragTouch) mouseUp(); if (_tptr.erase(id)) applyTouchKeys(); }

void Hud::gatherBlur(std::vector<pb::Game::BlurRect>& out) {
  for (auto it = _glass.begin(); it != _glass.end();) {
    DrawNode* d = it->first;
    if (d->getReferenceCount() == 1) { d->release(); it = _glass.erase(it); continue; }  // node was dropped from the HUD
    bool vis = d->getParent() != nullptr; for (Node* n = d; n && vis; n = n->getParent()) vis = n->isVisible();
    float op = d->getDisplayedOpacity() / 255.f;
    if (vis && op > 0.02f) for (auto& g : it->second) {
      Vec2 a = d->convertToWorldSpace(g.r.origin), b = d->convertToWorldSpace(Vec2(g.r.getMaxX(), g.r.getMaxY()));
      float sc = (b.x - a.x) / std::max(1.f, g.r.size.width);
      out.push_back({a.x, a.y, b.x, b.y, g.rad * sc, op});
    }
    ++it;
  }
}
