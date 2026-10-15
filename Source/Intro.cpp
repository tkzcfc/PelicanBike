#include "Intro.h"
#include <cmath>
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
USING_NS_AX;
static const char* FONT = "fonts/NotoSansSC-Regular.ttf";
static Color4F hexF(uint32_t h, float a = 1) { return Color4F(((h >> 16) & 255) / 255.f, ((h >> 8) & 255) / 255.f, (h & 255) / 255.f, a); }
static void rrect(DrawNode* d, Rect r, float rad, Color4F fill, Color4F stroke, float sw = 1) {
  std::vector<Vec2> pts; const int seg = 8; rad = std::min(rad, std::min(r.size.width, r.size.height) / 2);
  Vec2 c[4] = {{r.getMaxX() - rad, r.getMaxY() - rad}, {r.getMinX() + rad, r.getMaxY() - rad}, {r.getMinX() + rad, r.getMinY() + rad}, {r.getMaxX() - rad, r.getMinY() + rad}};
  for (int q = 0; q < 4; q++) for (int i = 0; i <= seg; i++) { float a = (q * 90 + i * 90.f / seg) * M_PI / 180; pts.push_back(c[q] + Vec2(std::cos(a), std::sin(a)) * rad); }
  d->drawPolygon(pts.data(), pts.size(), fill, stroke.a > 0 ? sw : 0, stroke);
}
float layoutKbdLine(Node* parent, DrawNode* d, const std::vector<std::string>& toks, float cx, float y, float fs, Color4B tc, bool measureOnly) {
  struct It { std::string s; bool key; float w; Label* l; }; std::vector<It> its; float W = 0;
  for (auto& t : toks) { bool key = !t.empty() && t[0] == '\x01'; std::string s = key ? t.substr(1) : t;
    auto l = Label::createWithTTF(s, FONT, key ? fs - 1 : fs); if (key) l->enableBold(); float w = l->getContentSize().width + (key ? 12 : 0); w = key ? std::max(w, 22.f) : w;
    its.push_back({s, key, w, l}); W += w + (key ? 4 : 0); }
  if (measureOnly) return W;
  float x = cx - W / 2;
  for (auto& it : its) { if (it.key) { x += 2; rrect(d, Rect(x, y - 10, it.w, 20), 6, Color4F(1, 1, 1, 0.12f), Color4F(1, 1, 1, 0.2f)); d->drawSegment(Vec2(x + 4, y - 10), Vec2(x + it.w - 4, y - 10), 1, Color4F(1, 1, 1, 0.2f)); }
    it.l->setAnchorPoint({0.5f, 0.5f}); it.l->setPosition(x + it.w / 2, y); it.l->setTextColor(it.key ? Color4B::WHITE : tc); parent->addChild(it.l, 3); x += it.w + (it.key ? 2 : 0); }
  return W;
}

bool Intro::init() {
  Node::init(); _sz = Director::getInstance()->getVisibleSize(); float W = _sz.width, H = _sz.height; _compact = H < 620; float artW = std::min(300.f, W * 0.7f); (void)artW;
  setCascadeOpacityEnabled(true);
  _bg = DrawNode::create(); addChild(_bg, 0); _art = DrawNode::create(); addChild(_art, 1); _ui = Node::create(); _ui->setCascadeOpacityEnabled(true); addChild(_ui, 2);
  auto lab = [&](const std::string& s, float size, float x, float yTop, Color4B c, bool bold = false) { auto l = Label::createWithTTF(s, FONT, size); l->setAnchorPoint({0.5f, 0.5f}); l->setPosition(x, H - yTop); l->setTextColor(c); if (bold) l->enableBold(); _ui->addChild(l, 3); return l; };
  const Color4B MUTED(236, 240, 250, 168);
  float y = _compact ? 30 : 45; float artH = _compact ? 0 : 190; y += artH + 6;
  // title: per-glyph gradient #fff7e6 -> #ffc27a (45%) -> #ff8a5b (CSS linear-gradient 92deg)
  { const char* glyphs[5] = {"鹈", "鹕", "骑", "单", "车"}; float fs = _compact ? 30 : 60, adv = fs * 1.04f, x0 = W / 2 - adv * 2;
    Color4F a = hexF(0xfff7e6), b = hexF(0xffc27a), c = hexF(0xff8a5b);
    for (int i = 0; i < 5; i++) { float u = (i + 0.5f) / 5; Color4F k = u < 0.45f ? Color4F(a.r + (b.r - a.r) * u / 0.45f, a.g + (b.g - a.g) * u / 0.45f, a.b + (b.b - a.b) * u / 0.45f, 1)
                                                                   : Color4F(b.r + (c.r - b.r) * (u - 0.45f) / 0.55f, b.g + (c.g - b.g) * (u - 0.45f) / 0.55f, b.b + (c.b - b.b) * (u - 0.45f) / 0.55f, 1);
      lab(glyphs[i], fs, x0 + i * adv, y + fs * 0.6f, Color4B(k), true); }
    y += fs * 1.25f; }
  float sfs = _compact ? 14 : 17;
  lab("一只戴头盔、围红围巾的白鹈鹕，骑着复古单车沿海岸线追鱼。", sfs, W / 2, y + 14, MUTED); lab("黄昏、星空、海浪与路灯 —— 全部实时生成，无一张外部贴图。", sfs, W / 2, y + 14 + sfs * 1.6f, MUTED);
  y += 14 + sfs * 3.2f + 18;
  if (!_compact) {  // chips (flex-wrap at 640px card width)
    const char* chips[7] = {"程序化建模", "双骨骼 IK 踩踏", "Verlet 布料围巾", "Gerstner 海浪", "昼夜循环", "合成音效 + 生成式音乐", "抓鱼小游戏"};
    std::vector<std::vector<int>> rows(1); float rowW = 0; std::vector<float> ws;
    for (int i = 0; i < 7; i++) { float w = Label::createWithTTF(chips[i], FONT, 12.5f)->getContentSize().width + 24; ws.push_back(w); if (rowW + w > std::min(640.f, W - 48) && !rows.back().empty()) { rows.emplace_back(); rowW = 0; } rows.back().push_back(i); rowW += w + 8; }
    for (auto& r : rows) { float tw = -8; for (int i : r) tw += ws[i] + 8; float x = W / 2 - tw / 2;
      for (int i : r) { rrect(_bg, Rect(x, H - y - 28, ws[i], 26), 13, Color4F(1, 1, 1, 0.08f), Color4F(1, 1, 1, 0.14f)); lab(chips[i], 12.5f, x + ws[i] / 2, y + 15, Color4B(255, 255, 255, 219)); x += ws[i] + 8; }
      y += 34; }
    y += 16;
  }
  // buttons
  { float bh = _compact ? 42 : 52, pw = _compact ? 130 : 152, gw = _compact ? 100 : 118, gap = 12, x = W / 2 - (pw + gap + gw) / 2;
    _bStart = Rect(x, H - y - bh, pw, bh); _bMute = Rect(x + pw + gap, H - y - bh, gw, bh);
    lab("开始骑行 ▶", _compact ? 15 : 17, _bStart.getMidX(), y + bh / 2, Color4B(42, 20, 0, 255), true); lab("静音进入", _compact ? 13 : 15, _bMute.getMidX(), y + bh / 2, Color4B(244, 246, 251, 255));
    y += bh + 20; }
  if (H >= 420) {
    auto keys = DrawNode::create(); _ui->addChild(keys, 2);
    std::vector<std::vector<std::string>> L = {{"\x01W", "\x01S", " 加速 / 刹车　", "\x01" "A", "\x01" "D", " 变道抓鱼　", "\x01空格", " 跳跃　", "\x01T", " 放手特技"},
                                               {"\x01" "B", " 按铃　", "\x01H", " 鹈鹕大叫　", "\x01" "C", " 切换镜头　", "\x01N", " 快进 3 小时　点击鹈鹕 / 车铃 / 海面都有惊喜"}};
    if (W < 760) L = {{"\x01W", "\x01S", " 加速 / 刹车　", "\x01" "A", "\x01" "D", " 变道抓鱼"}, {"\x01空格", " 跳跃　", "\x01T", " 放手特技　", "\x01" "B", " 按铃"},
                      {"\x01H", " 鹈鹕大叫　", "\x01" "C", " 切换镜头　", "\x01N", " 快进 3 小时"}, {"点击鹈鹕 / 车铃 / 海面都有惊喜"}};
    float fsK = _compact ? 11 : 12.5f, lh = _compact ? 24 : 30;
    for (size_t i = 0; i < L.size(); i++) layoutKbdLine(_ui, keys, L[i], W / 2, H - y - 12 - i * lh, fsK, MUTED);
    y += lh * (L.size() - 1) + 30;
  }
  { auto g = lab("GitHub 开源仓库", 12.5f, W / 2, y + 16, MUTED); _lGh = g->getBoundingBox(); y += 30; }
  { float y0 = _compact ? 30 : 45; _off = std::max(-y0 + 8, (H - (y - y0)) / 2 - y0);  // vertically centre the card (CSS grid place-items:center)
    _ui->setPositionY(-_off); _bg->setPositionY(-_off); for (Rect* r : {&_bStart, &_bMute, &_lGh}) r->origin.y -= _off; }
  return true;
}

bool Intro::mouseDown(Vec2 p) {
  if (gone()) return false; if (_starting) return true;
  if (_bStart.containsPoint(p)) start(true); else if (_bMute.containsPoint(p)) start(false);
  else if (_lGh.containsPoint(p) && onUrl) onUrl("https://github.com/tkzcfc/PelicanBike");
  return true;
}

void Intro::tick(float dt) {
  if (gone()) return; _t += dt; float W = _sz.width, H = _sz.height;
  if (_starting) { _fade = std::min(1.f, _fade + dt / 0.9f); setOpacity(uint8_t(255 * (1 - _fade))); if (_fade >= 1) { setVisible(false); return; } }
  float op = 1 - _fade;
  // background: linear-gradient(180deg, rgba(10,14,30,.55), rgba(8,10,22,.82)) + warm radial at (70%,10%)
  _bg->setOpacity(uint8_t(255 * op)); _art->clear();
  static bool bgDone = false; (void)bgDone;
  if (_t == dt || _starting) { /* rebuild bg with fade alpha */ }
  _art->drawSolidRect(Vec2(0, 0), Vec2(W, H), Color4F(0, 0, 0, 0));
  { const int N = 24; for (int i = 0; i < N; i++) { float u0 = float(i) / N, u1 = float(i + 1) / N; float a = (0.62f + (0.86f - 0.62f) * (u0 + u1) / 2) * op;
      Color4F c(lerpf(10, 8, u0) / 255.f, lerpf(14, 10, u0) / 255.f, lerpf(30, 22, u0) / 255.f, a); _art->drawSolidRect(Vec2(0, H * (1 - u1)), Vec2(W, H * (1 - u0)), c); }
    Vec2 rc(W * 0.7f, H * 0.9f); for (int k = 40; k >= 1; k--) { float r = W * 0.6f * k / 40; _art->drawSolidCircle(rc, r, 0, 64, 1, 0.75f, Color4F(1, 150 / 255.f, 90 / 255.f, 0.007f * op)); } }
  if (!_compact) {  // SVG art (viewBox 300x190) -> screen
    float ox = W / 2 - 150, oy = H - 45 - _off; auto P = [&](float x, float y) { return Vec2(ox + x, oy - y); };
    // road (dashed, moving)
    float off = std::fmod(_t / 0.6f * 32, 32.f);
    for (float x = -off; x < 300; x += 32) { float a = std::max(0.f, x), b = std::min(300.f, x + 18); if (b > a) _art->drawSegment(P(a, 178), P(b, 178), 1.5f, Color4F(1, 1, 1, 0.5f * op)); }
    float spin = _t / 1.1f * 2 * M_PI;
    for (float cx : {84.f, 210.f}) { Vec2 c = P(cx, 140); _art->drawCircle(c, 36, 0, 48, false, hexF(0x3fd0c9, op), 5);
      for (int k = 0; k < 4; k++) { float a = spin + k * M_PI / 4; Vec2 d(std::cos(a) * 36, std::sin(a) * 36); _art->drawSegment(c - d, c + d, 0.65f, hexF(0xccffee, op)); } }
    Color4F fr = hexF(0x138a8a, op); auto seg = [&](float a, float b, float c, float d, float w, Color4F col) { _art->drawSegment(P(a, b), P(c, d), w, col); };
    seg(84, 140, 140, 142, 3, fr); seg(140, 142, 186, 98, 3, fr); seg(186, 98, 118, 98, 3, fr); seg(118, 98, 84, 140, 3, fr); seg(140, 142, 112, 84, 3, fr); seg(210, 140, 192, 80, 3, fr); seg(192, 80, 178, 78, 3, fr);
    float bob = 2 * (0.5f - 0.5f * std::cos(std::fmod(_t / 0.55f, 2.f) * M_PI));  // ease-in-out alternate 0..2px
    auto Q = [&](float x, float y) { return P(x, y + bob); };
    { std::vector<Vec2> e; for (int i = 0; i < 40; i++) { float a = i * 2 * M_PI / 40, ex = std::cos(a) * 46, ey = std::sin(a) * 30, r = -18 * M_PI / 180;
        e.push_back(Q(120 + ex * std::cos(r) - ey * std::sin(r), 70 + ex * std::sin(r) + ey * std::cos(r))); } _art->drawSolidPoly(e.data(), e.size(), hexF(0xf6f3ec, op)); }
    { Vec2 prev; for (int i = 0; i <= 16; i++) { float u = i / 16.f, v = 1 - u; float x = v * v * v * 150 + 3 * v * v * u * 170 + 3 * v * u * u * 150 + u * u * u * 166, y = v * v * v * 58 + 3 * v * v * u * 40 + 3 * v * u * u * 22 + u * u * u * 10;
        Vec2 p = Q(x, y); if (i) _art->drawSegment(prev, p, 8, hexF(0xf6f3ec, op)); prev = p; } }
    _art->drawSolidCircle(Q(168, 12), 13, 0, 24, hexF(0xf6f3ec, op));
    { Vec2 t[3] = {Q(176, 10), Q(262, 30), Q(178, 22)}; _art->drawSolidPoly(t, 3, hexF(0xf7c25c, op)); Vec2 t2[3] = {Q(178, 22), Q(256, 32), Q(262, 30)}; _art->drawSolidPoly(t2, 3, hexF(0xf5b857, 0.9f * op)); }
    { Vec2 t[4] = {Q(172, 6), Q(150, -4), Q(140, 0), Q(160, 4)}; _art->drawSolidPoly(t, 4, hexF(0xff5a4e, op)); }
    _art->drawSolidCircle(Q(170, 11), 2.6f, 0, 10, hexF(0x1a0c08, op));
    _art->drawSegment(Q(100, 76), Q(150, 100), 3.5f, hexF(0x19181b, op)); _art->drawSegment(Q(118, 92), Q(138, 142), 2.5f, hexF(0xf38a2a, op));
    { Vec2 prev; for (int i = 0; i <= 16; i++) { float u = i / 16.f, v = 1 - u; float x = v * v * v * 150 + 3 * v * v * u * 120 + 3 * v * u * u * 96 + u * u * u * 70, y = v * v * v * 50 + 3 * v * v * u * 40 + 3 * v * u * u * 58 + u * u * u * 50;
        Vec2 p = Q(x, y); if (i) _art->drawSegment(prev, p, 3.5f, hexF(0xd7263d, op)); prev = p; } }
  }
  // buttons (hover-less static styling)
  { Rect a = _bStart; _art->drawSolidRect(Vec2(0, 0), Vec2(0, 0), Color4F());
    for (int k = 6; k >= 1; k--) rrect(_art, Rect(a.origin.x - k * 2, a.origin.y - 10 - k * 2 + 6, a.size.width + k * 4, a.size.height + k * 4), a.size.height / 2 + k * 2, Color4F(1, 140 / 255.f, 60 / 255.f, 0.035f * op), Color4F(0, 0, 0, 0));
    const int N = 12; for (int i = 0; i < N; i++) { float u0 = float(i) / N; Color4F c(1, lerpf(210, 154, u0) / 255.f, lerpf(122, 77, u0) / 255.f, op); (void)c; }
    rrect(_art, a, a.size.height / 2, Color4F(1, 0.72f, 0.39f, op), Color4F(1, 1, 1, 0.35f * op));
    rrect(_art, _bMute, _bMute.size.height / 2, Color4F(1, 1, 1, 0.08f * op), Color4F(1, 1, 1, 0.14f * op)); }
  _ui->setOpacity(uint8_t(255 * op));
}
