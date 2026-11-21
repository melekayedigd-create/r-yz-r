// The 20 built-in templates (v1.1).
// Redesigned after studying Myriam's reference pictures:
//  - strong contrast: dark, layered insides + bright rims + soft glow halos
//  - layers: fill -> inset panel -> inner gradient -> rim -> inner line -> cap/ledge -> details
//  - two main hues + one small accent colour
//  - details live on the edges: caps that stick out, hanging roots/vines/icicles, corner pieces,
//    circuit lines with dots, big dim gears behind the structure, sparkles around it
#include "Deco.hpp"

namespace deco {
namespace {

// ---------- builders ----------
struct PB {
    Piece p;
    PB(int id) { p.id = id; p.z = ZT2; p.zo = 6; }
    PB& at(float dx, float dy) { p.dx = dx; p.dy = dy; return *this; }
    PB& sc(float s) { p.sx = p.sy = s; return *this; }
    PB& sc(float x, float y) { p.sx = x; p.sy = y; return *this; }
    PB& r(float d) { p.rot = d; return *this; }
    PB& c(int s) { p.slot = s; return *this; }
    PB& c2(int s) { p.slot2 = s; return *this; }
    PB& z(int z, int zo) { p.z = z; p.zo = zo; return *this; }
    PB& zo(int zo) { p.zo = zo; return *this; }
    PB& jit(float pos, float rot = 0, float scale = 0) { p.jPos = pos; p.jRot = rot; p.jScale = scale; return *this; }
    PB& flip() { p.randFlip = true; return *this; }
    PB& fx() { p.fx = true; return *this; }
    PB& fy() { p.fy = true; return *this; }
    PB& anim() { p.anim = true; return *this; }
    PB& v(float v) { p.v = v; return *this; }
    PB& grow(float s, float hBase) { p.stretch = s; p.hBase = hBase; return *this; }
    operator Piece() const { return p; }
};
// a 15x15 white square sized in units
PB sq(float w, float h) { PB b(916); b.sc(w / 15.f, h / 15.f); b.p.hBase = 15.f; return b; }
// a thin line hanging down from its anchor, growing randomly longer (roots, chains, vines)
PB hang(float w, float h, float grow) { PB b = sq(w, h); b.at(0, h / 2); b.grow(grow, 15.f); return b; }

Cluster C(std::initializer_list<Piece> ps, float w = 1.f) { Cluster c; c.pieces = ps; c.weight = w; return c; }

Feature fill(int slot, int zo = 0, float v = 1.f) {
    Feature f; f.type = FType::Fill; f.slot = slot; f.z = ZT2; f.zo = zo; f.v = v; return f;
}
Feature panel(int slot, float inset, int zo = 1, float v = 1.f) {
    Feature f; f.type = FType::Panel; f.slot = slot; f.inset = inset; f.z = ZT2; f.zo = zo; f.v = v; return f;
}
Feature outline(int slot, float thick, float inset = 0, int zo = 5, unsigned sides = 0xF) {
    Feature f; f.type = FType::Outline; f.slot = slot; f.thick = thick; f.inset = inset; f.z = ZT2; f.zo = zo; f.sides = sides; return f;
}
// an inner line with a dot at each open end (circuit look)
Feature circuit(int slot, float thick, float inset, int dotSlot, float dot = 0.55f, int zo = 4) {
    Feature f = outline(slot, thick, inset, zo);
    Piece d = PB(1764).c(dotSlot).sc(dot).zo(zo + 1);
    f.opts = { C({ d }) };
    return f;
}
Feature glow(int slot, float size, bool inward = false, unsigned sides = 0xF, int zo = 0) {
    Feature f; f.type = FType::Glow; f.slot = slot; f.glowSize = size; f.inward = inward; f.sides = sides;
    f.z = inward ? ZT2 : ZB1; f.zo = inward ? (zo ? zo : 2) : zo;
    return f;
}
Feature extrude(float ex, float ey, int top, int side, int line = -1, float lt = 1.5f, int zo = 4) {
    Feature f; f.type = FType::Extrude; f.ex = ex; f.ey = ey; f.slotTop = top; f.slotSide = side; f.lineSlot = line; f.lineThick = lt;
    f.z = ZT2; f.zo = zo; return f;
}
// a slab on top that sticks out past the ends (caps / platforms)
Feature ledge(int slot, float thick, float over, int edgeSlot = -1, int underSlot = -1, float sink = 2.f, int zo = 7) {
    Feature f; f.type = FType::Ledge; f.slot = slot; f.thick = thick; f.over = over; f.slotEdge = edgeSlot; f.slotUnder = underSlot;
    f.inset = sink; f.z = ZT2; f.zo = zo; return f;
}
Feature surf(Side s, float prob, std::initializer_list<Cluster> cs, int spacing = 1, bool orient = true) {
    Feature f; f.type = FType::Surface; f.side = s; f.prob = prob; f.opts = cs; f.spacing = spacing; f.orient = orient;
    f.priority = 1; return f;
}
Feature corner(float prob, unsigned mask, std::initializer_list<Cluster> cs, bool orient = false, bool concave = false) {
    Feature f; f.type = FType::Corner; f.prob = prob; f.cornerMask = mask; f.opts = cs; f.orient = orient;
    f.concave = concave; f.convex = !concave; f.priority = 1; return f;
}
Feature inter(Pattern pat, float prob, std::initializer_list<Cluster> cs, int px = 2, int py = 2, int minDepth = 0) {
    Feature f; f.type = FType::Interior; f.pat = pat; f.prob = prob; f.opts = cs; f.px = px; f.py = py; f.minDepth = minDepth;
    f.orient = false; f.priority = 2; return f;
}
Feature band(float dist, float thick, int slot, float inset = 0, int zo = 4) {
    Feature f; f.type = FType::Band; f.dist = dist; f.thick = thick; f.slot = slot; f.inset = inset; f.z = ZT2; f.zo = zo; return f;
}
Feature scatter(float perCell, int margin, std::initializer_list<Cluster> cs) {
    Feature f; f.type = FType::Scatter; f.perCell = perCell; f.margin = margin; f.opts = cs; f.priority = 3; return f;
}
// big dim objects behind the structure's edges (gears, rings...)
Feature backdrop(float perCell, std::initializer_list<Cluster> cs) {
    Feature f = scatter(perCell, 1, cs); f.hugEdge = true; f.priority = 3; return f;
}
Feature withChance(Feature f, float c) { f.chance = c; return f; }
Feature withJ(Feature f, float j) { f.probJ = j; return f; }

Slot S(int r, int g, int b, bool blend = false, char const* name = "") { Slot s; s.c = { r, g, b }; s.blend = blend; s.name = name; return s; }

constexpr unsigned TL = 1, TR = 2, BL = 4, BR = 8, TOPS = TL | TR, BOTS = BL | BR, ALL = 0xF;
constexpr unsigned SIDES_LRB = (1u << 1) | (1u << 2) | (1u << 3);
constexpr unsigned TOP_ONLY = 1u;

// spike accent with a smaller inner spike (picture: magenta / cyan triangles)
Cluster spikeIn(int outer, int inner, float s = 1.f, float w = 1.f) {
    return C({ PB(1766).c(outer).sc(2.4f * s).at(0, 9.6f * s), PB(1766).c(inner).sc(1.3f * s).at(0, 7.4f * s).zo(7) }, w);
}
// a gear behind the structure, slowly spinning
Cluster gearBack(int slot, float s, int id = 87, float w = 1.f) {
    return C({ PB(id).c(slot).sc(s).z(ZB3, 0).jit(6, 30, 0.25f).v(0.55f).anim() }, w);
}

std::vector<Template> make() {
    std::vector<Template> v;

    // 1 ---------------------------------------------------------------- Neon Grid
    {
        Template t; t.id = "neon-grid"; t.name = "Neon Grid"; t.by = "Myriam";
        t.desc = "Pink & cyan neon rims, glowing caps, circuit lines";
        t.palette = { S(255, 45, 175, false, "Neon A"), S(40, 225, 255, false, "Neon B"), S(18, 6, 28, false, "Fill"),
                      S(255, 30, 160, true, "Glow A"), S(30, 200, 255, true, "Glow B"), S(58, 14, 60, false, "Panel"),
                      S(255, 235, 250, false, "Hot") };
        t.swaps = { { 0, 1 }, { 3, 4 } }; t.slotSwap = 0.45f; t.hueVar = 6;
        t.features = {
            fill(2),
            panel(5, 6),
            glow(3, 1.4f, true, TOP_ONLY),
            outline(0, 2.5f),
            [] { Feature f = circuit(1, 1.f, 6, 1); f.dash = 22.f; return f; }(),
            glow(3, 1.3f),
            ledge(0, 4, 4, 6, -1, 2.f),
            withJ(inter(Pattern::Random, 0.25f, {
                C({ PB(917).c(1).sc(0.7f).jit(7).zo(3) }),
                C({ sq(12, 1.2f).c(0).jit(6).zo(3).v(0.8f), PB(1764).c(0).sc(0.4f).at(7, 0).zo(4) }, 0.7f),
            }, 2, 2, 1), 0.1f),
            withChance(corner(0.6f, TOPS, { spikeIn(0, 1, 0.8f) }), 0.85f),
            withChance(backdrop(0.05f, { gearBack(4, 1.1f, 87), gearBack(3, 0.8f, 86, 0.6f) }), 0.6f),
            withChance(scatter(0.07f, 3, { C({ PB(917).c(1).sc(0.6f).jit(8) }), C({ PB(1764).c(6).sc(0.3f).jit(8) }) }), 0.8f),
        };
        v.push_back(t);
    }
    // 2 ---------------------------------------------------------------- Pixel Pop City
    {
        Template t; t.id = "pixel-pop"; t.name = "Pixel Pop City"; t.by = "Myriam";
        t.desc = "Bright pixel buildings with bold borders and light caps";
        t.palette = { S(235, 50, 195, false, "Main"), S(130, 0, 110, false, "Border"), S(255, 130, 238, false, "Light"),
                      S(80, 150, 230, false, "Main B"), S(25, 95, 195, false, "Border B"), S(160, 205, 255, false, "Light B"),
                      S(205, 30, 165, false, "Shade") };
        t.swaps = { { 0, 3 }, { 1, 4 }, { 2, 5 } }; t.slotSwap = 0.4f; t.hueVar = 5;
        t.features = {
            fill(0),
            panel(6, 7, 1, 1.f),
            extrude(6, 8, 2, 1, 1, 2.5f),
            outline(1, 4.f),
            ledge(2, 5, 3, -1, 1, 4.f),
            withJ(inter(Pattern::Random, 0.4f, {
                C({ PB(917).c(1).sc(1.1f).jit(8).zo(3) }),
                C({ PB(917).c(1).sc(0.7f).jit(8).zo(3), PB(917).c(1).sc(0.5f).at(6, -5).jit(6).zo(3) }),
                C({ sq(7, 7).c(2).jit(7).zo(3) }, 0.5f),
            }, 2, 2, 1), 0.1f),
            surf(Side::Left, 0.25f, { C({ sq(14, 14).c(1).at(0, 5).zo(2), sq(9, 9).c(2).at(0, 5).zo(3) }) }, 2),
            surf(Side::Right, 0.25f, { C({ sq(14, 14).c(1).at(0, 5).zo(2), sq(9, 9).c(2).at(0, 5).zo(3) }) }, 2),
            withChance(scatter(0.06f, 2, { C({ PB(917).c(1).sc(1.f).jit(5) }), C({ PB(917).c(4).sc(0.7f).jit(5) }) }), 0.7f),
        };
        v.push_back(t);
    }
    // 3 ---------------------------------------------------------------- Blue Spire Glow
    {
        Template t; t.id = "blue-spire"; t.name = "Blue Spire Glow"; t.by = "Myriam";
        t.desc = "Deep blue panels, bright glowing caps, + and x marks, crystal tips";
        t.palette = { S(140, 190, 255, false, "Edge"), S(40, 100, 255, true, "Glow"), S(6, 10, 48, false, "Fill"),
                      S(225, 240, 255, false, "Tips"), S(40, 50, 140, false, "Panel"), S(100, 210, 255, false, "Cap"),
                      S(200, 80, 220, false, "Accent") };
        t.hueVar = 8;
        t.features = {
            fill(2),
            panel(4, 5),
            glow(1, 1.2f, true, TOP_ONLY),
            outline(0, 2.5f),
            circuit(0, 0.9f, 5, 0, 0.5f),
            glow(1, 1.5f),
            ledge(5, 6, 3, 3, 4, 3.f),
            corner(0.8f, TOPS, { C({ PB(1766).c(3).sc(2.2f).at(-7, 8.8f + 4) }), C({ PB(1766).c(3).sc(1.6f).at(-6, 6.4f + 4), PB(1766).c(0).sc(1.1f).at(-13, 4.4f + 4) }, 0.5f) }),
            withChance(surf(Side::Top, 0.25f, { spikeIn(5, 2, 0.9f) }, 2), 0.7f),
            withJ(inter(Pattern::Random, 0.22f, {
                C({ sq(9, 2.2f).c(5).r(45).zo(3), sq(9, 2.2f).c(5).r(-45).zo(3) }),             // x
                C({ sq(8, 1.8f).c(5).zo(3), sq(1.8f, 8).c(5).zo(3) }, 0.8f),                       // +
                C({ sq(5, 1.5f).c(6).r(45).zo(3), sq(5, 1.5f).c(6).r(-45).zo(3) }, 0.4f),          // small x
            }, 2, 2, 1), 0.1f),
            withChance(scatter(0.05f, 2, { C({ PB(1764).c(3).sc(0.4f).jit(8) }), C({ PB(51).c(6).sc(0.25f).jit(6) }, 0.5f) }), 0.6f),
        };
        v.push_back(t);
    }
    // 4 ---------------------------------------------------------------- Golden Fortress
    {
        Template t; t.id = "golden-fortress"; t.name = "Golden Fortress"; t.by = "Myriam";
        t.desc = "Dark towers with gold trim, lit windows and 3D roofs";
        t.palette = { S(230, 190, 110, false, "Gold"), S(26, 18, 12, false, "Fill"), S(255, 228, 140, false, "Window"),
                      S(255, 165, 55, true, "Warm glow"), S(62, 46, 30, false, "Roof"), S(95, 62, 30, false, "Frame"),
                      S(48, 34, 22, false, "Panel") };
        t.hueVar = 5;
        Cluster window = C({ sq(19, 19).c(0).zo(6), sq(16, 16).c(2).zo(7), sq(16, 1.6f).c(5).zo(8), sq(1.6f, 16).c(5).zo(8),
                             PB(503).c(3).sc(0.7f, 0.7f).at(0, -4).z(ZT2, 9) });
        Cluster vent = C({ sq(1.8f, 18).c(0).at(-6, 0).zo(6), sq(1.8f, 18).c(0).at(0, 0).zo(6), sq(1.8f, 18).c(0).at(6, 0).zo(6) }, 0.35f);
        Cluster darkWin = C({ sq(19, 19).c(0).zo(6), sq(16, 16).c(5).zo(7), sq(16, 1.6f).c(1).zo(8), sq(1.6f, 16).c(1).zo(8) }, 0.4f);
        Cluster spike = C({ PB(149).c(0).sc(0.78f).at(0, 11.7f).zo(3), PB(149).c(1).sc(0.62f).at(0, 10.4f).zo(4) });
        t.features = {
            fill(1),
            panel(6, 4),
            extrude(9, 9, 4, 1, 0, 1.6f),
            outline(0, 2.f),
            outline(0, 0.8f, 4.f, 4),
            withChance(glow(3, 1.f), 0.7f),
            withJ(inter(Pattern::Windows, 0.7f, { window, vent, darkWin }, 2, 2), 0.15f),
            withChance(surf(Side::Top, 0.25f, { spike }, 2), 0.85f),
            withChance(surf(Side::Bottom, 0.3f, { spike }, 2), 0.85f),
            withChance(surf(Side::Left, 0.18f, { C({ sq(22, 11).c(0).at(0, 7).zo(2), sq(19, 8).c(4).at(0, 7).zo(3) }) }, 2), 0.7f),
            withChance(surf(Side::Right, 0.18f, { C({ sq(22, 11).c(0).at(0, 7).zo(2), sq(19, 8).c(4).at(0, 7).zo(3) }) }, 2), 0.7f),
        };
        v.push_back(t);
    }
    // 5 ---------------------------------------------------------------- Techno Panels
    {
        Template t; t.id = "techno-panels"; t.name = "Techno Panels"; t.by = "Myriam";
        t.desc = "Glowing white-rim slabs, red & blue circuits, gears behind";
        t.palette = { S(255, 75, 95, false, "Neon A"), S(110, 170, 255, false, "Neon B"), S(22, 18, 40, false, "Fill"),
                      S(255, 225, 235, false, "White rim"), S(255, 60, 90, true, "Glow"), S(95, 235, 255, false, "Pad"),
                      S(60, 40, 90, false, "Panel"), S(120, 120, 255, true, "Glow B") };
        t.swaps = { { 0, 1 }, { 4, 7 } }; t.slotSwap = 0.45f;
        Cluster stripes = C({ sq(26, 26).c(6).zo(2), sq(18, 3.5f).c(0).r(-45).at(-7, 0).zo(3), sq(18, 3.5f).c(0).r(-45).at(0, 0).zo(3), sq(18, 3.5f).c(0).r(-45).at(7, 0).zo(3) });
        Cluster chips = C({ PB(917).c(3).sc(0.9f).at(-6, 0).zo(3), PB(917).c(2).sc(0.55f).at(-6, 0).zo(4), PB(917).c(3).sc(0.9f).at(6, 0).zo(3), PB(917).c(2).sc(0.55f).at(6, 0).zo(4) }, 0.7f);
        t.features = {
            fill(2),
            panel(6, 5),
            outline(3, 3.f),
            outline(0, 2.f, 3.f, 5),
            glow(4, 1.2f),
            ledge(3, 4, 6, -1, 0, 2.f),
            circuit(0, 1.f, 9, 3, 0.6f),
            withJ(inter(Pattern::Windows, 0.65f, { stripes, chips }, 3, 2, 1), 0.1f),
            surf(Side::Bottom, 0.25f, { C({ PB(50).c(5).sc(0.5f).at(0, 0).z(ZT1, 0) }) }, 2),
            withChance(backdrop(0.06f, { gearBack(7, 1.f, 87), gearBack(4, 0.7f, 97, 0.7f) }), 0.75f),
            [] { Feature f; f.type = FType::Wire; f.prob = 0.1f; f.slot = 5; f.nodeSlot = 2; f.thick = 2.f; f.node = 6.f; f.z = ZT2; f.zo = 3; f.priority = 2; return f; }(),
            withChance(scatter(0.05f, 2, { C({ PB(917).c(1).sc(0.6f).jit(6) }) }), 0.6f),
        };
        v.push_back(t);
    }
    // 6 ---------------------------------------------------------------- Rooftop Day (modern villa)
    {
        Template t; t.id = "rooftop-day"; t.name = "Rooftop Villa"; t.by = "Myriam";
        t.desc = "Concrete slabs, wood slats on top, vines with red flowers";
        t.palette = { S(205, 205, 210, false, "Concrete"), S(245, 245, 250, false, "Slab"), S(42, 32, 28, false, "Wood"),
                      S(140, 140, 150, false, "Shadow"), S(70, 140, 60, false, "Vine"), S(200, 60, 50, false, "Flower"),
                      S(60, 48, 40, false, "Dark wood"), S(255, 255, 255, true, "Light") };
        t.hueVar = 3;
        Cluster vine = C({ hang(3.f, 16, 2.f).c(4).at(0, 5).zo(8).jit(2), PB(1764).c(4).sc(1.2f).at(-3, 6).zo(9), PB(1764).c(4).sc(1.f).at(3, 14).zo(9), PB(1764).c(5).sc(0.7f).at(2, 10).zo(10) });
        t.features = {
            fill(0),
            inter(Pattern::Rows, 1.f, { C({ sq(30, 1.2f).c(3).at(0, -14).zo(2) }) }, 1, 1),     // slab joints
            withJ(inter(Pattern::Random, 0.18f, { C({ sq(26, 24).c(6).zo(2), sq(24, 1.2f).c(2).at(0, 6).zo(3), sq(24, 1.2f).c(2).at(0, -2).zo(3) }) }, 2, 2, 1), 0.1f),
            outline(3, 1.5f, 0, 5, SIDES_LRB),
            ledge(1, 4, 6, -1, 3, 1.f),
            withChance(surf(Side::Top, 1.f, { C({ sq(3, 22).c(2).at(-10, 13).zo(2), sq(3, 22).c(2).at(0, 13).zo(2), sq(3, 22).c(2).at(10, 13).zo(2),
                                                   sq(30, 2.5f).c(1).at(0, 24).zo(3) }) }), 0.6f),
            surf(Side::Left, 0.3f, { vine }, 1, false),
            surf(Side::Right, 0.3f, { vine }, 1, false),
            withChance(surf(Side::Top, 0.25f, { C({ PB(149).c(1).sc(0.5f).at(0, 9.5f).zo(3) }) }, 3), 0.6f),
            withChance(glow(7, 0.5f), 0.5f),
        };
        v.push_back(t);
    }
    // 7 ---------------------------------------------------------------- Flat Modern
    {
        Template t; t.id = "flat-modern"; t.name = "Flat Modern"; t.by = "Myriam";
        t.desc = "Clean navy blocks, white diamonds, lavender light pillars";
        t.palette = { S(42, 52, 82, false, "Navy"), S(240, 240, 255, false, "White"), S(195, 165, 255, false, "Lavender"),
                      S(175, 135, 255, true, "Light"), S(72, 82, 112, false, "Border"), S(52, 64, 98, false, "Panel") };
        t.features = {
            fill(0),
            panel(5, 5),
            outline(4, 1.5f),
            ledge(1, 2.5f, 3, -1, -1, 0.5f),
            inter(Pattern::Random, 0.3f, { C({ PB(53).c(1).sc(0.3f).zo(3) }) }),
            withChance(surf(Side::Top, 0.35f, { C({ sq(22, 3.5f).c(0).at(0, 6), sq(22, 3.5f).c(0).at(0, 11.5f), sq(22, 3.5f).c(0).at(0, 17) }) }, 2), 0.8f),
            surf(Side::Right, 0.3f, { C({ PB(1293).c(3).sc(1.f, 2.f).at(0, 7.5f).z(ZB1, 0) }) }, 2, false),
            surf(Side::Left, 0.3f, { C({ PB(1293).c(3).sc(1.f, 2.f).at(0, 7.5f).z(ZB1, 0) }) }, 2, false),
            withChance(surf(Side::Bottom, 0.35f, { C({ PB(1766).c(0).sc(1.6f).at(0, 6.4f) }) }), 0.75f),
            corner(0.35f, TOPS, { C({ PB(53).c(1).sc(0.35f).at(-8, -8).zo(4) }) }),
        };
        v.push_back(t);
    }
    // 8 ---------------------------------------------------------------- Mystic Outline
    {
        Template t; t.id = "mystic-outline"; t.name = "Mystic Outline"; t.by = "Myriam";
        t.desc = "Thin cyan lines, purple runes, tiny squares and canes";
        t.palette = { S(130, 240, 255, false, "Cyan"), S(170, 90, 255, false, "Purple"), S(8, 10, 28, false, "Fill"),
                      S(90, 220, 255, true, "Cyan glow"), S(150, 70, 255, true, "Purple glow"), S(18, 24, 52, false, "Panel") };
        t.swaps = { { 0, 1 }, { 3, 4 } }; t.slotSwap = 0.25f;
        t.features = {
            fill(2),
            panel(5, 8),
            outline(0, 1.5f),
            glow(3, 0.8f),
            withJ(inter(Pattern::Random, 0.32f, {
                C({ sq(14, 2).c(1).jit(7).zo(3) }),
                C({ sq(2, 16).c(1).jit(7).zo(3) }),
                C({ sq(8, 1.6f).c(1).at(-3, 6).zo(3), sq(1.6f, 6).c(1).at(-6.2f, 3.8f).zo(3) }, 0.6f),
                C({ PB(917).c(0).sc(0.85f).zo(3), PB(917).c(2).sc(0.6f).zo(4) }, 0.7f),
            }), 0.1f),
            corner(0.5f, TOPS, { C({ PB(1766).c(0).sc(2.6f).at(-11, 10.4f), PB(1766).c(1).sc(1.6f).at(-11, 9.6f).zo(7) }) }),
            withChance(surf(Side::Top, 0.12f, { C({ PB(1766).c(0).sc(1.5f).at(-4, 6), PB(1766).c(0).sc(1.5f).at(5, 6) }) }), 0.7f),
            surf(Side::Left, 0.12f, { C({ sq(16, 11).c(1).at(0, 3).v(0.85f).zo(2), sq(12, 2).c(0).r(-35).at(0, 3).zo(3) }) }, 3),
            surf(Side::Right, 0.12f, { C({ sq(16, 11).c(1).at(0, 3).v(0.85f).zo(2), sq(12, 2).c(0).r(35).at(0, 3).zo(3) }) }, 3),
            withChance(scatter(0.07f, 3, {
                C({ sq(1.5f, 40).c(0).at(0, -5), PB(1831).c(0).sc(0.4f).at(6, 18) }),
                C({ PB(917).c(0).sc(0.7f), PB(917).c(2).sc(0.45f).zo(7) }, 1.5f),
            }), 0.8f),
        };
        v.push_back(t);
    }
    // 9 ---------------------------------------------------------------- Circuit Glow
    {
        Template t; t.id = "circuit-glow"; t.name = "Circuit Glow"; t.by = "Myriam";
        t.desc = "Bright white blocks, purple aura, circuit traces and vents";
        t.palette = { S(246, 244, 255, false, "White"), S(150, 60, 255, true, "Aura"), S(45, 32, 75, false, "Trace"),
                      S(232, 222, 255, false, "Edge"), S(222, 216, 240, false, "Top"), S(198, 190, 222, false, "Side") };
        t.features = {
            fill(0),
            extrude(10, 10, 4, 5, 3, 1.5f),
            outline(3, 1.5f, -1.5f),
            circuit(2, 1.f, 6, 2, 0.7f),
            glow(1, 2.f),
            withJ(inter(Pattern::Random, 0.32f, {
                C({ sq(16, 1.6f).c(2).at(-3, -4).zo(3), sq(1.6f, 12).c(2).at(5, 2).zo(3), PB(1764).c(2).sc(0.6f).at(-11, -4).zo(4), PB(1764).c(2).sc(0.6f).at(5, 8).zo(4) }),
                C({ sq(1.6f, 16).c(2).at(-5, 0).zo(3), sq(1.6f, 16).c(2).at(0, 0).zo(3), sq(1.6f, 16).c(2).at(5, 0).zo(3) }, 0.7f),
                C({ PB(1764).c(2).sc(0.35f).at(-5, 0).zo(3), PB(1764).c(2).sc(0.35f).at(0, 0).zo(3), PB(1764).c(2).sc(0.35f).at(5, 0).zo(3) }, 0.6f),
                C({ sq(14, 14).c(2).zo(3), sq(11, 11).c(0).zo(4), PB(1764).c(2).sc(0.6f).zo(5) }, 0.5f),
            }, 2, 2, 1), 0.1f),
            corner(0.35f, ALL, { C({ PB(1830).c(2).sc(0.6f).at(-9, -9).zo(3) }) }, true, true),
        };
        v.push_back(t);
    }
    // 10 --------------------------------------------------------------- Amethyst Crystal
    {
        Template t; t.id = "amethyst"; t.name = "Amethyst Crystal"; t.by = "Claude";
        t.desc = "Purple crystal rock, glowing crystal clusters above and below";
        t.palette = { S(34, 8, 62, false, "Rock"), S(190, 95, 255, false, "Crystal"), S(238, 195, 255, false, "Shine"),
                      S(160, 60, 255, true, "Glow"), S(110, 50, 170, false, "Edge"), S(60, 22, 100, false, "Panel"),
                      S(90, 230, 255, false, "Cyan") };
        t.hueVar = 10;
        Cluster c1 = C({ PB(151).c(1).sc(0.75f).at(-3, 15).r(-8).jit(2, 6, 0.2f), PB(153).c(2).sc(0.7f).at(5, 6.6f).r(14).jit(2, 6, 0.2f) });
        Cluster c2 = C({ PB(152).c(1).sc(0.7f).at(0, 10.5f).jit(3, 10, 0.2f), PB(153).c(6).sc(0.55f).at(-6, 5).r(-20).jit(2, 6) });
        Cluster c3 = C({ PB(153).c(2).sc(0.8f).at(0, 7.6f).jit(5, 15, 0.2f) }, 0.7f);
        t.features = {
            fill(0),
            panel(5, 5),
            glow(3, 1.f, true),
            outline(4, 2.f),
            glow(3, 1.4f),
            withJ(surf(Side::Top, 0.65f, { c1, c2, c3 }), 0.15f),
            withChance(surf(Side::Bottom, 0.45f, { C({ PB(152).c(1).sc(0.6f).at(0, 9).jit(3, 8, 0.25f) }), c3 }), 0.85f),
            inter(Pattern::Random, 0.25f, { C({ PB(53).c(2).sc(0.3f).jit(8).zo(3) }), C({ PB(1766).c(1).sc(1.f).jit(8, 30).zo(3) }), C({ PB(53).c(6).sc(0.22f).jit(8).zo(3) }, 0.4f) }),
            withChance(scatter(0.07f, 2, { C({ PB(53).c(2).sc(0.25f).jit(6) }), C({ PB(54).c(2).sc(0.22f).jit(6, 30) }) }), 0.8f),
        };
        v.push_back(t);
    }
    // 11 --------------------------------------------------------------- Miku Neon
    {
        Template t; t.id = "miku-neon"; t.name = "Miku Neon"; t.by = "Claude";
        t.desc = "Magenta stage with cyan rims, triangles in triangles, notes";
        t.palette = { S(60, 230, 230, false, "Cyan"), S(255, 80, 200, false, "Magenta"), S(70, 10, 80, false, "Fill"),
                      S(40, 220, 255, true, "Cyan glow"), S(245, 255, 255, false, "White"), S(255, 60, 200, true, "Pink glow"),
                      S(120, 20, 130, false, "Panel") };
        t.swaps = { { 0, 1 }, { 3, 5 } }; t.slotSwap = 0.25f;
        auto triIn = [](float x, float y, bool down) {
            float r = down ? 180.f : 0.f;
            return C({ PB(1766).c(1).sc(2.6f).r(r).at(x, y).zo(3), PB(1766).c(0).sc(1.7f).r(r).at(x, y + (down ? 1.5f : -1.5f)).zo(4),
                       PB(1766).c(2).sc(1.f).r(r).at(x, y + (down ? 2.4f : -2.4f)).zo(5) });
        };
        t.features = {
            fill(2),
            panel(6, 5),
            glow(5, 1.2f, true, TOP_ONLY),
            outline(0, 3.f),
            outline(1, 1.f, 6.f, 4),
            glow(3, 1.1f),
            withJ(inter(Pattern::Windows, 0.6f, { triIn(0, 0, false), triIn(0, 0, true) }, 2, 2, 1), 0.15f),
            withChance(surf(Side::Bottom, 0.35f, { C({ PB(1766).c(1).sc(2.4f).at(0, 9.6f), PB(1766).c(0).sc(1.3f).at(0, 7.4f).zo(7) }) }), 0.8f),
            withChance(surf(Side::Top, 0.22f, { C({ PB(60).c(4).sc(0.5f).at(0, 9).jit(3, 12) }), spikeIn(0, 1, 0.8f) }), 0.85f),
            withChance(scatter(0.07f, 2, { C({ PB(60).c(1).sc(0.4f).jit(6, 15) }), C({ PB(1766).c(0).sc(1.f).jit(6, 30) }), C({ PB(1764).c(3).sc(0.5f).jit(6) }) }), 0.85f),
        };
        v.push_back(t);
    }
    // 12 --------------------------------------------------------------- Fortress Stone
    {
        Template t; t.id = "fortress-stone"; t.name = "Fortress Stone"; t.by = "Claude";
        t.desc = "Grey brick walls, metal caps, vines, chains and torches";
        t.palette = { S(92, 92, 104, false, "Stone"), S(38, 38, 46, false, "Mortar"), S(196, 196, 206, false, "Metal cap"),
                      S(56, 56, 64, false, "Crack"), S(255, 150, 50, true, "Torch glow"), S(255, 205, 95, false, "Flame"),
                      S(70, 200, 60, false, "Vine"), S(120, 120, 132, false, "Cap shade") };
        t.hueVar = 4;
        Cluster vine = C({ hang(3, 12, 2.5f).c(6).zo(8).jit(3, 6), PB(1764).c(6).sc(0.9f).at(-3, 6).zo(9), PB(1764).c(6).sc(0.7f).at(3, 14).zo(9) });
        t.features = {
            fill(0),
            withJ(inter(Pattern::Every, 0.9f, { C({ sq(30, 1.6f).c(1).at(0, 0).zo(2), sq(30, 1.6f).c(1).at(0, 15).zo(2),
                                                   sq(1.6f, 15).c(1).at(-7, 7.5f).zo(2), sq(1.6f, 15).c(1).at(8, -7.5f).zo(2) }) }), 0.1f),
            outline(1, 2.5f),
            ledge(2, 6, 4, -1, 7, 3.f),
            inter(Pattern::Random, 0.12f, { C({ PB(1369).c(3).sc(0.5f).jit(6, 180).zo(3) }), C({ PB(1371).c(3).sc(0.5f).jit(6, 180).zo(3) }) }),
            withChance(surf(Side::Top, 1.f, { C({ sq(15, 12).c(0).at(0, 9).zo(2), sq(15, 3).c(2).at(0, 14).zo(3), sq(1.5f, 12).c(1).at(-7, 9).zo(3), sq(1.5f, 12).c(1).at(7, 9).zo(3) }) }, 2), 0.6f),
            withChance(surf(Side::Bottom, 0.3f, { vine, C({ PB(41).c(1).sc(0.45f).at(0, 15.6f) }, 0.5f) }, 1, false), 0.85f),
            surf(Side::Left, 0.15f, { vine }, 2, false),
            surf(Side::Right, 0.15f, { vine }, 2, false),
            withChance(surf(Side::Left, 0.1f, { C({ sq(3, 12).c(1).at(0, 1).zo(3), PB(1766).c(5).sc(1.3f, 1.8f).at(0, 10).zo(4), PB(504).c(4).sc(1.2f).at(0, 10).z(ZB1, 0) }) }, 3, false), 0.6f),
        };
        v.push_back(t);
    }
    // 13 --------------------------------------------------------------- Overgrown Forest (cartoon)
    {
        Template t; t.id = "overgrown"; t.name = "Overgrown Forest"; t.by = "Claude";
        t.desc = "Cartoon blocks: thick outline, grass strip, leaves and flowers";
        t.palette = { S(232, 186, 160, false, "Ground"), S(110, 205, 80, false, "Grass"), S(46, 70, 80, false, "Outline"),
                      S(250, 120, 190, false, "Flower"), S(214, 160, 136, false, "Panel"), S(70, 160, 70, false, "Leaf dark"),
                      S(255, 245, 160, false, "Flower centre") };
        t.hueVar = 6;
        Cluster flower = C({ PB(1765).c(3).sc(2.2f).r(45).zo(9), PB(1764).c(6).sc(0.6f).zo(10) }, 0.5f);
        t.features = {
            fill(0),
            panel(4, 6, 1),
            outline(2, 3.f),
            band(0, 5, 1, 3),
            surf(Side::Top, 0.8f, {
                C({ PB(406).c(1).sc(1.1f).at(0, 7.5f).flip().jit(5, 0, 0.2f) }),
                C({ PB(407).c(1).sc(1.f).at(4, 5.5f).flip().jit(5), PB(408).c(5).sc(1.f).at(-6, 4).flip() }),
                C({ PB(908).c(5).sc(0.55f).at(0, 5.5f).flip().jit(4, 0, 0.15f) }, 0.6f),
                C({ PB(939).c(3).c2(1).sc(0.7f).at(0, 2).jit(5) }, 0.3f),
            }),
            corner(0.6f, ALL, { flower }, false, false),
            withChance(surf(Side::Bottom, 0.3f, { C({ hang(2.5f, 10, 1.5f).c(5).zo(8).jit(3, 6), PB(1764).c(1).sc(0.7f).at(0, 12) }) }, 1, false), 0.8f),
            corner(0.9f, ALL, { C({ PB(1764).c(2).sc(0.45f).at(-5, -5).zo(6) }) }),
            surf(Side::Left, 0.15f, { C({ hang(2.5f, 8, 1.2f).c(5).at(0, 3).jit(2, 5) }) }, 1, false),
            surf(Side::Right, 0.15f, { C({ hang(2.5f, 8, 1.2f).c(5).at(0, 3).jit(2, 5) }) }, 1, false),
        };
        v.push_back(t);
    }
    // 14 --------------------------------------------------------------- Frozen Peak
    {
        Template t; t.id = "frozen-peak"; t.name = "Frozen Peak"; t.by = "Claude";
        t.desc = "Deep blue ice rock, snow caps, long icicles and ice shards";
        t.palette = { S(40, 80, 150, false, "Rock"), S(246, 251, 255, false, "Snow"), S(20, 40, 95, false, "Deep"),
                      S(120, 205, 255, true, "Glow"), S(200, 238, 255, false, "Icicle"), S(70, 120, 190, false, "Panel"),
                      S(150, 230, 255, false, "Shard") };
        t.features = {
            fill(0),
            panel(5, 6),
            glow(2, 1.3f, true, 0xE),
            outline(1, 1.5f),
            surf(Side::Top, 1.f, { C({ PB(1893).c(1).at(0, -6).zo(7) }), C({ PB(1896).c(1).at(0, -6).zo(7) }), C({ PB(1897).c(1).at(0, -6).zo(7) }), C({ PB(1898).c(1).at(0, -6).zo(7) }) }),
            surf(Side::Bottom, 0.65f, { C({ PB(149).c(4).sc(0.32f, 0.7f).at(0, 10.5f).grow(1.2f, 30).jit(3) }),
                                        C({ PB(149).c(4).sc(0.25f, 0.5f).at(-6, 7.5f).grow(1.f, 30), PB(149).c(4).sc(0.2f, 0.4f).at(6, 6).grow(1.f, 30) }) }),
            withChance(surf(Side::Top, 0.25f, { C({ PB(1766).c(6).sc(1.4f, 3.f).at(0, 12).r(-12).jit(4, 15), PB(1766).c(1).sc(0.9f, 2.f).at(6, 8).r(18) }) }, 2), 0.8f),
            withChance(glow(3, 1.f), 0.8f),
            inter(Pattern::Random, 0.15f, { C({ PB(1766).c(6).sc(1.f, 2.f).jit(8, 40).zo(3).v(0.8f) }) }, 2, 2, 1),
            withChance(scatter(0.06f, 2, { C({ PB(1765).c(1).sc(0.7f).jit(6, 45) }), C({ PB(1766).c(6).sc(1.2f).jit(6, 60) }) }), 0.8f),
        };
        v.push_back(t);
    }
    // 15 --------------------------------------------------------------- Inferno
    {
        Template t; t.id = "inferno"; t.name = "Inferno"; t.by = "Claude";
        t.desc = "Red-hot plates, glowing spikes, hanging tags and embers";
        t.palette = { S(40, 8, 12, false, "Rock"), S(255, 90, 40, false, "Lava"), S(255, 222, 120, false, "Hot"),
                      S(255, 60, 40, true, "Glow"), S(230, 50, 70, false, "Edge"), S(90, 20, 25, false, "Panel"),
                      S(255, 140, 180, false, "Pink") };
        t.hueVar = 6;
        Cluster tag = C({ hang(1.2f, 6, 0.5f).c(1).zo(6), sq(5, 9).c(4).at(0, 12).zo(7), sq(3, 1.2f).c(2).at(0, 12).zo(8) });
        t.features = {
            fill(0),
            panel(5, 5),
            glow(3, 1.4f, true, 1u << 2),
            outline(4, 2.f),
            outline(1, 0.8f, 5, 4),
            ledge(1, 4, 5, 2, -1, 2.f),
            withJ(surf(Side::Top, 0.35f, { spikeIn(4, 2, 1.f), C({ PB(1766).c(1).sc(1.1f, 2.f).at(0, 12).jit(3, 8), PB(504).c(3).sc(0.6f).at(0, 8).z(ZB1, 0) }, 0.6f) }), 0.1f),
            surf(Side::Bottom, 0.4f, { spikeIn(4, 2, 0.9f), tag }),
            inter(Pattern::Random, 0.25f, { C({ PB(1369).c(1).sc(0.6f).jit(5, 180).zo(3) }), C({ PB(1655).c(2).sc(0.5f).jit(5, 180).zo(3) }, 0.5f) }),
            glow(3, 1.2f),
            withChance(scatter(0.08f, 2, { C({ PB(1764).c(2).sc(0.3f).jit(8) }), C({ PB(1764).c(1).sc(0.4f).jit(8) }) }), 0.85f),
        };
        v.push_back(t);
    }
    // 16 --------------------------------------------------------------- Deep Ocean
    {
        Template t; t.id = "deep-ocean"; t.name = "Deep Ocean"; t.by = "Claude";
        t.desc = "Deep blue reef blocks, tall seaweed, coral and bubbles";
        t.palette = { S(10, 42, 92, false, "Deep"), S(60, 200, 230, false, "Aqua"), S(205, 245, 255, false, "Foam"),
                      S(40, 150, 255, true, "Glow"), S(255, 122, 100, false, "Coral"), S(42, 172, 122, false, "Seaweed"),
                      S(20, 70, 130, false, "Panel") };
        t.features = {
            fill(0),
            panel(6, 5),
            glow(3, 1.f, true, TOP_ONLY),
            outline(1, 1.5f),
            band(0, 3, 2, 1.5f),
            surf(Side::Top, 0.7f, {
                C({ PB(1756).c(5).sc(0.8f, 0.6f).at(0, 9).grow(1.8f, 30).jit(4, 10) }),
                C({ PB(1756).c(5).sc(0.7f, 0.5f).at(-5, 7.5f).grow(1.5f, 30).jit(3, 8), PB(1756).c(5).sc(0.7f, 0.7f).at(4, 10.5f).grow(1.5f, 30).jit(3, 8) }),
                C({ PB(123).c(4).sc(0.5f).at(0, 7.5f).flip() }, 0.6f),
                C({ PB(125).c(4).sc(0.5f).at(0, 7.5f).flip() }, 0.4f),
            }),
            inter(Pattern::Random, 0.22f, { C({ PB(51).c(2).sc(0.2f).jit(8).zo(3) }) }),
            withChance(scatter(0.12f, 3, { C({ PB(51).c(2).sc(0.22f).jit(8, 0, 0.4f) }), C({ PB(51).c(2).sc(0.14f).jit(8) }) }), 0.9f),
            glow(3, 0.9f),
        };
        v.push_back(t);
    }
    // 17 --------------------------------------------------------------- Candy Pop (cartoon)
    {
        Template t; t.id = "candy-pop"; t.name = "Candy Pop"; t.by = "Claude";
        t.desc = "Pastel cartoon blocks: bolts, inset panels, icing and flowers";
        t.palette = { S(225, 242, 255, false, "Candy"), S(255, 255, 255, false, "Icing"), S(40, 80, 100, false, "Outline"),
                      S(255, 120, 190, false, "Flower"), S(110, 170, 200, false, "Panel"), S(120, 210, 120, false, "Leaf"),
                      S(255, 240, 140, false, "Centre") };
        t.swaps = { { 0, 1 } }; t.slotSwap = 0.0f; t.hueVar = 10;
        Cluster flower = C({ PB(1765).c(3).sc(2.4f).r(45).zo(9), PB(1764).c(6).sc(0.65f).zo(10) });
        t.features = {
            fill(0),
            panel(4, 7, 1),
            outline(2, 3.f),
            outline(2, 1.2f, 7.f, 2),
            corner(0.9f, ALL, { C({ PB(1764).c(2).sc(0.45f).at(-4, -4).zo(6) }) }),
            corner(0.7f, ALL, { flower }, false, false),
            surf(Side::Top, 0.45f, { C({ PB(406).c(5).sc(1.1f).at(0, 7.5f).flip() }), C({ PB(407).c(5).sc(1.f).at(3, 5.5f).flip() }) }),
            withChance(surf(Side::Bottom, 0.3f, { C({ PB(1766).c(1).sc(1.4f).at(0, 5.6f), PB(1766).c(2).sc(0.6f).at(0, 4.4f).zo(7).v(0.7f) }) }, 2), 0.7f),
            surf(Side::Left, 0.3f, { C({ hang(2.5f, 12, 2.f).c(5).at(0, 3).jit(2, 5), PB(1765).c(3).sc(1.6f).r(45).at(0, 14).zo(9) }) }, 2, false),
            surf(Side::Right, 0.3f, { C({ hang(2.5f, 12, 2.f).c(5).at(0, 3).jit(2, 5), PB(1765).c(3).sc(1.6f).r(45).at(0, 14).zo(9) }) }, 2, false),
            withChance(scatter(0.04f, 2, { C({ PB(52).c(3).sc(0.3f).jit(6, 15) }) }), 0.5f),
        };
        v.push_back(t);
    }
    // 18 --------------------------------------------------------------- Haunted Night
    {
        Template t; t.id = "haunted"; t.name = "Haunted Night"; t.by = "Claude";
        t.desc = "Gothic dark ruins, pink thorn glow, tall windows, hanging lamps";
        t.palette = { S(28, 14, 30, false, "Dark"), S(232, 222, 200, false, "Bone"), S(255, 90, 170, true, "Pink glow"),
                      S(150, 110, 120, false, "Frame"), S(70, 30, 60, false, "Thorn"), S(48, 26, 44, false, "Panel"),
                      S(255, 200, 220, false, "Window light") };
        Cluster window = C({ sq(14, 22).c(3).zo(4), sq(11, 19).c(6).v(0.85f).zo(5), PB(50).c(3).sc(0.47f).at(0, 11).zo(4), PB(50).c(6).sc(0.37f).at(0, 11).v(0.85f).zo(5),
                             sq(1.2f, 26).c(3).zo(6), sq(11, 1.2f).c(3).at(0, 2).zo(6) });
        Cluster lamp = C({ hang(1, 8, 0.8f).c(3).zo(6), sq(10, 3).c(3).at(0, 9).zo(7), PB(503).c(2).sc(0.5f, 1.f).at(0, 20).r(180).z(ZB1, 0) });
        t.features = {
            fill(0),
            panel(5, 5),
            glow(2, 1.2f, true, TOP_ONLY),
            outline(3, 2.f),
            glow(2, 1.2f),
            withJ(inter(Pattern::Windows, 0.75f, { window }, 2, 2, 0), 0.1f),
            corner(0.7f, TOPS, { C({ PB(1766).c(3).sc(2.f).at(-8, 8).zo(6), PB(1766).c(0).sc(1.1f).at(-8, 6.6f).zo(7) }) }),
            surf(Side::Top, 0.35f, { C({ PB(123).c(4).sc(0.7f).at(0, 10).flip() }), C({ PB(126).c(4).sc(0.7f).at(0, 10).flip() }), C({ PB(1596).c(1).sc(0.6f).at(0, 5).flip() }, 0.3f) }),
            surf(Side::Left, 0.35f, { C({ PB(124).c(4).sc(0.6f).at(0, 7).r(0).flip() }) }),
            surf(Side::Right, 0.35f, { C({ PB(124).c(4).sc(0.6f).at(0, 7).flip() }) }),
            withChance(surf(Side::Bottom, 0.25f, { lamp, C({ PB(106).c(3).sc(0.45f).at(0, 15) }, 0.6f) }, 2, false), 0.85f),
            withChance(scatter(0.05f, 2, { C({ PB(1764).c(2).sc(0.45f).jit(8) }) }), 0.8f),
        };
        v.push_back(t);
    }
    // 19 --------------------------------------------------------------- Clockwork (steel platforms)
    {
        Template t; t.id = "clockwork"; t.name = "Clockwork"; t.by = "Claude";
        t.desc = "Steel-blue platforms, glowing caps, X braces and big gears";
        t.palette = { S(60, 70, 140, false, "Steel"), S(18, 18, 45, false, "Dark"), S(200, 215, 255, false, "Cap"),
                      S(120, 140, 230, false, "Brace"), S(110, 130, 255, true, "Glow"), S(40, 48, 100, false, "Panel"),
                      S(255, 255, 255, false, "White"), S(255, 120, 200, true, "Pink glow") };
        Cluster brace = C({ sq(34, 2.2f).c(3).r(45).zo(3), sq(34, 2.2f).c(3).r(-45).zo(3) });
        t.features = {
            fill(1),
            panel(5, 4),
            withJ(inter(Pattern::Every, 0.7f, { brace }, 1, 1, 1), 0.1f),
            outline(3, 2.f),
            ledge(2, 5, 5, 6, 5, 2.f),
            glow(4, 1.f),
            corner(0.7f, ALL, { C({ PB(1764).c(2).sc(0.5f).at(-6, -6).zo(7) }) }),
            withChance(surf(Side::Bottom, 0.3f, { C({ sq(6, 14).c(3).at(0, 7), sq(9, 3).c(2).at(0, 14).zo(7) }) }, 2, false), 0.8f),
            withChance(backdrop(0.08f, { gearBack(4, 1.3f, 87), gearBack(3, 0.9f, 86, 0.7f), gearBack(7, 0.7f, 97, 0.4f) }), 0.9f),
            withChance(surf(Side::Top, 0.2f, { C({ PB(86).c(3).sc(0.6f).at(0, 0).anim().z(ZT1, 0) }) }, 3), 0.6f),
        };
        v.push_back(t);
    }
    // 20 --------------------------------------------------------------- Galaxy Dream
    {
        Template t; t.id = "galaxy"; t.name = "Galaxy Dream"; t.by = "Claude";
        t.desc = "Glittering space panels, hanging black roots, purple gears";
        t.palette = { S(14, 8, 40, false, "Space"), S(140, 60, 255, true, "Nebula"), S(255, 255, 255, false, "Star"),
                      S(110, 190, 255, false, "Edge"), S(255, 80, 200, true, "Pink nebula"), S(30, 40, 110, false, "Panel"),
                      S(8, 6, 14, false, "Roots") };
        t.swaps = { { 1, 4 } }; t.slotSwap = 0.4f; t.hueVar = 12;
        t.features = {
            fill(0),
            panel(5, 4),
            glow(1, 1.2f, true),
            outline(3, 2.f),
            circuit(3, 0.8f, 4, 2, 0.4f),
            glow(4, 1.f),
            inter(Pattern::Every, 0.7f, { C({ PB(1764).c(2).sc(0.18f).jit(10).zo(3), PB(54).c(2).sc(0.15f).jit(10, 30).zo(3) }), C({ PB(1764).c(3).sc(0.15f).jit(10).zo(3) }, 0.6f) }, 1, 1, 0),
            surf(Side::Bottom, 0.85f, {
                C({ hang(1.6f, 12, 2.5f).c(6).zo(3).jit(5), hang(1.2f, 8, 2.f).c(6).at(6, 4).zo(3) }),
                C({ hang(2.2f, 16, 2.f).c(6).zo(3).jit(5), hang(1.f, 6, 2.f).c(6).at(-7, 3).zo(3) }),
                C({ PB(1766).c(6).sc(1.6f, 3.f).at(0, 12).r(180).zo(3) }, 0.5f),
            }, 1, false),
            withChance(surf(Side::Top, 0.25f, { C({ PB(1766).c(3).sc(1.6f).at(-5, 6.4f), PB(1766).c(3).sc(1.2f).at(5, 4.8f) }) }, 2), 0.7f),
            withChance(backdrop(0.06f, { gearBack(1, 1.2f, 87), gearBack(4, 0.9f, 86, 0.7f) }), 0.8f),
            withChance(scatter(0.14f, 3, { C({ PB(54).c(2).sc(0.25f).jit(8, 30, 0.3f) }), C({ PB(1764).c(2).sc(0.3f).jit(8) }), C({ PB(1764).c(3).sc(0.4f).jit(8) }, 0.5f) }), 0.9f),
        };
        v.push_back(t);
    }
    // 21 --------------------------------------------------------------- Microcosm (red & white)
    {
        Template t; t.id = "microcosm"; t.name = "Microcosm"; t.by = "Myriam";
        t.desc = "Red tile panels, broken cream frames, corner brackets, blue circuit wires";
        t.palette = { S(118, 26, 32, false, "Tile"), S(160, 44, 48, false, "Tile light"), S(250, 226, 190, false, "Cream"),
                      S(120, 200, 255, false, "Wire"), S(255, 110, 90, true, "Warm glow"), S(64, 8, 16, false, "Deep"),
                      S(255, 246, 236, false, "White"), S(255, 140, 110, true, "Soft glow") };
        t.hueVar = 4;
        t.features = {
            fill(5),
            withJ(inter(Pattern::Every, 0.92f, { C({ sq(27.5f, 27.5f).c(0).zo(1) }, 1.f), C({ sq(27.5f, 27.5f).c(1).zo(1) }, 0.35f),
                                                  C({ sq(27.5f, 27.5f).c(0).zo(1), sq(16, 2).c(1).at(0, 5).zo(2).v(1.2f) }, 0.25f) }, 1, 1, 0), 0.05f),
            glow(4, 1.3f, true, TOP_ONLY, 3),
            [] { Feature f = outline(2, 2.2f, -1.5f, 6); f.dash = 26.f; return f; }(),
            corner(1.f, ALL, { C({ sq(9, 2).c(6).at(-11.5f, -7).zo(7), sq(2, 9).c(6).at(-7, -11.5f).zo(7) }) }),
            withChance(corner(0.5f, BOTS | TL, { C({ PB(917).c(2).sc(1.35f).at(9, -2).zo(7), PB(917).c(5).sc(0.85f).at(9, -2).zo(8), sq(2, 26).c(2).at(9, 14).zo(7) }) }), 0.8f),
            [] { Feature f; f.type = FType::Wire; f.prob = 0.14f; f.slot = 3; f.nodeSlot = 5; f.thick = 2.4f; f.node = 7.f;
                 f.wireMin = 25; f.wireMax = 70; f.z = ZT2; f.zo = 3; f.priority = 2; return f; }(),
            withChance(surf(Side::Top, 0.3f, { C({ PB(1766).c(2).sc(3.6f, 2.f).at(0, 8), PB(1766).c(5).sc(2.6f, 1.3f).at(0, 6.5f).zo(7) }) }, 4), 0.8f),
            glow(7, 1.1f),
            withChance(scatter(0.06f, 2, { C({ sq(2.5f, 2.5f).c(2).jit(8).v(0.7f) }), C({ PB(917).c(2).sc(0.7f).jit(6), PB(917).c(5).sc(0.4f).jit(0).zo(7) }, 0.5f) }), 0.8f),
        };
        v.push_back(t);
    }
    return v;
}

} // namespace

std::vector<Template> const& builtinTemplates() {
    static std::vector<Template> v = make();
    return v;
}

} // namespace deco
