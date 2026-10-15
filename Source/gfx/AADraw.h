// AADrawNode: drop-in DrawNode whose fills, strokes, circles and segments get a ~1 device-pixel
// coverage fringe (alpha ramp to 0) so HUD/menu shapes have smooth edges at any DPR without MSAA.
// Axmol's stock DrawNode fills polygons with hard edges and its line shader uses step(), so
// rounded buttons stair-step. Methods below hide the DrawNode ones of the same name (non-virtual):
// call them through an AADrawNode* (Hud/Intro store AADrawNode*).
#pragma once
#include "axmol.h"
#include <vector>

namespace pb {
// Label::enableBold() fakes weight with a WHITE 0.9pt shadow, which shows as a light halo on dark text once
// glyphs are rasterized at device resolution; use the label's own text colour for the shadow instead.
inline void boldLabel(ax::Label* l) { l->enableBold(); l->enableShadow(l->getTextColor(), ax::Vec2(0.9f, 0), 0); }
class AADrawNode : public ax::DrawNode {
public:
  static AADrawNode* create() { auto* n = new AADrawNode(); if (n->init()) { n->autorelease(); return n; } delete n; return nullptr; }
  using ax::DrawNode::drawCircle;
  void drawSolidPoly(const ax::Vec2* p, unsigned n, const ax::Color4F& c, float thickness = 0, const ax::Color4F& border = ax::Color4F(0, 0, 0, 0), bool isconvex = false);
  void drawPolygon(const ax::Vec2* p, int n, const ax::Color4F& fill, float thickness, const ax::Color4F& border, bool isconvex = false);
  void drawPoly(const ax::Vec2* p, unsigned n, bool closed, const ax::Color4F& c, float thickness = 1.f);
  void drawSolidRect(const ax::Vec2& a, const ax::Vec2& b, const ax::Color4F& c);
  void drawSolidCircle(const ax::Vec2& ctr, float r, float angle, unsigned segs, const ax::Color4F& c) { drawSolidCircle(ctr, r, angle, segs, 1.f, 1.f, c); }
  void drawSolidCircle(const ax::Vec2& ctr, float r, float angle, unsigned segs, float sx, float sy, const ax::Color4F& c);
  void drawCircle(const ax::Vec2& ctr, float r, float angle, unsigned segs, bool lineToCenter, const ax::Color4F& c, float thickness = 1.f);
  void drawSegment(const ax::Vec2& a, const ax::Vec2& b, float thickness, const ax::Color4F& c);
  void drawDot(const ax::Vec2& p, float r, const ax::Color4F& c) { drawSolidCircle(p, r, 0, 0, c); }
  // node units per device pixel (design -> framebuffer scale); fringe width is one of these
  static float px();
private:
  void tri(const ax::Vec2& a, const ax::Color4F& ca, const ax::Vec2& b, const ax::Color4F& cb, const ax::Vec2& c, const ax::Color4F& cc);
  void fill(const std::vector<ax::Vec2>& p, const ax::Color4F& c, bool convex);
  void stroke(const std::vector<ax::Vec2>& p, bool closed, const ax::Color4F& c, float w);
  static unsigned circleSegs(float r);
};
}  // namespace pb
