// Scenes: instead of decorating blocks, fill an area with a full background look,
// like the backdrop of a cutscene. Everything goes on the B layers (behind gameplay).
#include "Deco.hpp"
#include <algorithm>
#include <cmath>

namespace deco {
namespace {

struct SG {
    Options const& o;
    Rng r;
    float x0, y0, x1, y1;
    std::vector<Obj> out;
    SG(Options const& o, float a, float b, float c, float d) : o(o), r(hash(o.seed, 0x5CE11E)), x0(a), y0(b), x1(c), y1(d) {}

    float W() const { return x1 - x0; }
    float H() const { return y1 - y0; }
    int count(float base) const { return std::max(1, (int)std::round(base * o.density)); }

    Obj& obj(int id, float x, float y, float sx, float sy, int slot, int z, int zo, float v = 1.f, float rot = 0, bool fx = false, bool fy = false) {
        Obj ob; ob.id = id; ob.x = x; ob.y = y; ob.sx = sx; ob.sy = sy; ob.rot = rot; ob.fx = fx; ob.fy = fy;
        ob.slot = ob.slot2 = slot; ob.z = z; ob.zo = zo; ob.v = v;
        out.push_back(ob);
        return out.back();
    }
    // rectangle from the 15x15 white square
    void rect(float cx, float cy, float w, float h, int slot, int z, int zo, float v = 1.f, float rot = 0) {
        if (w < 0.3f || h < 0.3f) return;
        obj(916, cx, cy, w / 15.f, h / 15.f, slot, z, zo, v, rot);
    }
    void frame(float cx, float cy, float w, float h, float t, int slot, int z, int zo, float v = 1.f) {
        rect(cx, cy + h / 2 - t / 2, w, t, slot, z, zo, v);
        rect(cx, cy - h / 2 + t / 2, w, t, slot, z, zo, v);
        rect(cx - w / 2 + t / 2, cy, t, h, slot, z, zo, v);
        rect(cx + w / 2 - t / 2, cy, t, h, slot, z, zo, v);
    }
    void line(float ax, float ay, float bx, float by, float t, int slot, int z, int zo, float v = 1.f) {
        float dx = bx - ax, dy = by - ay, len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.5f) return;
        obj(916, (ax + bx) / 2, (ay + by) / 2, len / 15.f, t / 15.f, slot, z, zo, v, -std::atan2(dy, dx) * 57.2958f);
    }
    // soft round glow made of 4 quarter gradients (1012, bright at its bottom-right corner)
    void radial(float cx, float cy, float rad, int slot, int z, int zo, float v = 1.f) {
        float s = rad / 30.f;
        obj(1012, cx - rad / 2, cy + rad / 2, s, s, slot, z, zo, v, 0, false, false);
        obj(1012, cx + rad / 2, cy + rad / 2, s, s, slot, z, zo, v, 0, true, false);
        obj(1012, cx - rad / 2, cy - rad / 2, s, s, slot, z, zo, v, 0, false, true);
        obj(1012, cx + rad / 2, cy - rad / 2, s, s, slot, z, zo, v, 0, true, true);
    }
    // a horizontal band of fog / light (1011 = 30x30 gradient, bright at the bottom)
    void band(float y, float h, int slot, int z, int zo, float v = 1.f, bool down = false) {
        obj(1011, (x0 + x1) / 2, y + (down ? -h / 2 : h / 2), W() / 30.f, h / 30.f, slot, z, zo, v, down ? 180.f : 0.f);
    }
    // a building / tower silhouette standing on y, with optional lit windows and top lines
    void building(float cx, float w, float h, float yb, int slot, int z, int zo, int winSlot, float winChance, float v = 1.f, int edgeSlot = -1) {
        rect(cx, yb + h / 2, w, h, slot, z, zo, v);
        if (edgeSlot >= 0) rect(cx, yb + h - 1.f, w, 2.f, edgeSlot, z, zo + 1);
        if (winSlot >= 0 && winChance > 0) {
            for (float wy = yb + 10; wy < yb + h - 8; wy += 10)
                for (float wx = cx - w / 2 + 6; wx < cx + w / 2 - 4; wx += 8)
                    if (r.chance(winChance)) rect(wx, wy, 3.5f, 4.5f, winSlot, z, zo + 1, r.range(0.6f, 1.f));
        }
        if (r.chance(0.3f)) rect(cx + r.sym(w / 4), yb + h + 6, 1.2f, 12, slot, z, zo, v);   // antenna
    }
    void stars(int n, int slot, int z, float maxScale = 0.35f) {
        for (int i = 0; i < n; i++) {
            float x = r.range(x0, x1), y = r.range(y0 + H() * 0.25f, y1);
            if (r.chance(0.25f)) obj(54, x, y, maxScale, maxScale, slot, z, 2, r.range(0.6f, 1.f), r.range(0, 70));
            else { float s = r.range(0.12f, maxScale * 0.8f); obj(1764, x, y, s, s, slot, z, 2, r.range(0.4f, 1.f)); }
        }
    }
    void clouds(int n, int slot, int z, float yMin, float yMax, float scale, bool cartoon = false) {
        static const int soft[] = { 129, 130, 131, 48, 49 };
        static const int toon[] = { 936, 937, 938 };
        for (int i = 0; i < n; i++) {
            int id = cartoon ? toon[r.i(3)] : soft[r.i(5)];
            float s = scale * r.range(0.7f, 1.4f);
            obj(id, r.range(x0, x1), r.range(yMin, yMax), s * r.range(1.f, 1.8f), s, slot, z, r.i(3), r.range(0.8f, 1.f), 0, r.chance(0.5f));
        }
    }
};

// ---------------- the scenes ----------------
void microcosm(SG& g) {
    // slots: 0 tile, 1 tile light, 2 cream, 3 blue wire, 4 warm glow, 5 BG, 6 white, 7 deep red
    float W = g.W(), H = g.H();
    g.band(g.y0, H * 0.45f, 4, ZB4, 0, 0.45f);
    // panels of red tiles with cream frames
    int nPanels = g.count(std::clamp(W / 140.f, 2.f, 9.f));
    struct P { float cx, cy, w, h; };
    std::vector<P> panels;
    for (int i = 0; i < nPanels * 4 && (int)panels.size() < nPanels; i++) {
        float w = std::round(g.r.range(3, 8)) * 30, h = std::round(g.r.range(2, 5)) * 30;
        float cx = std::round(g.r.range(g.x0 + w / 2, std::max(g.x0 + w / 2 + 1, g.x1 - w / 2)) / 30) * 30;
        float cy = std::round(g.r.range(g.y0 + h / 2, std::max(g.y0 + h / 2 + 1, g.y1 - h / 2)) / 30) * 30;
        bool clash = false;
        for (auto& p : panels) if (std::abs(p.cx - cx) < (p.w + w) / 2 + 12 && std::abs(p.cy - cy) < (p.h + h) / 2 + 12) clash = true;
        if (clash) continue;
        panels.push_back({ cx, cy, w, h });
    }
    for (auto& p : panels) {
        g.radial(p.cx, p.cy, std::max(p.w, p.h) * 0.75f, 4, ZB4, 1, 0.55f);
        // tiles
        for (float tx = p.cx - p.w / 2 + 15; tx < p.cx + p.w / 2; tx += 30)
            for (float ty = p.cy - p.h / 2 + 15; ty < p.cy + p.h / 2; ty += 30) {
                float vv = g.r.range(0.75f, 1.15f);
                g.rect(tx, ty, 28.5f, 28.5f, g.r.chance(0.25f) ? 1 : 0, ZB3, 0, vv);
                if (g.r.chance(0.15f)) g.rect(tx, ty + 4, 18, 2.2f, 1, ZB3, 1, 1.2f);
            }
        // gradient shine from the top
        g.obj(1011, p.cx, p.cy + p.h / 2 - 15, p.w / 30.f, 1.f, 4, ZB3, 2, 0.35f, 180);
        // broken cream frame + corner brackets
        float t = 2.2f, inx = 7;
        g.rect(p.cx - p.w * 0.12f, p.cy + p.h / 2, p.w * 0.76f, t, 2, ZB2, 0);
        g.rect(p.cx + p.w / 2, p.cy - p.h * 0.1f, t, p.h * 0.8f, 2, ZB2, 0);
        g.rect(p.cx - p.w / 2, p.cy + p.h * 0.15f, t, p.h * 0.7f, 2, ZB2, 0);
        g.rect(p.cx + p.w * 0.2f, p.cy - p.h / 2, p.w * 0.6f, t, 2, ZB2, 0);
        float bx = p.cx - p.w / 2 + inx, by = p.cy + p.h / 2 - inx;
        g.rect(bx + 4, by, 9, 2, 6, ZB2, 1); g.rect(bx, by - 4, 2, 9, 6, ZB2, 1);
        float bx2 = p.cx + p.w / 2 - inx, by2 = p.cy - p.h / 2 + inx;
        g.rect(bx2 - 4, by2, 9, 2, 6, ZB2, 1); g.rect(bx2, by2 + 4, 2, 9, 6, ZB2, 1);
        g.rect(p.cx - p.w / 2 + 25, p.cy + p.h / 2 - 9, 22, 1.8f, 2, ZB2, 1, 0.9f);
        // hollow squares near the frame
        if (g.r.chance(0.7f)) { float sx = p.cx - p.w / 2 - 14, sy = p.cy - p.h / 2 + 8; g.frame(sx, sy, 10, 10, 2, 2, ZB2, 1); g.rect(sx + 5, sy + 20, 2, 30, 2, ZB2, 0); }
        if (g.r.chance(0.6f)) { float sx = p.cx + p.w / 2 - 12, sy = p.cy + p.h / 2 - 12; g.frame(sx, sy, 8, 8, 1.6f, 6, ZB2, 1); }
        // roof triangle outline on some panels
        if (g.r.chance(0.35f)) {
            float rx = p.cx + g.r.sym(p.w / 4), ry = p.cy + p.h / 2 + 10;
            g.obj(1766, rx, ry, 3.6f, 2.f, 2, ZB2, 0);
            g.obj(1766, rx, ry - 1.5f, 2.6f, 1.3f, 7, ZB2, 1);
        }
    }
    // blue circuit wires with square nodes, between and around panels
    int nWires = g.count(panels.size() * 1.5f);
    for (int i = 0; i < nWires && !panels.empty(); i++) {
        auto& p = panels[g.r.i((int)panels.size())];
        float sx = p.cx + g.r.sym(p.w / 2.5f), sy = p.cy + g.r.sym(p.h / 2.5f);
        float l1 = g.r.range(40, 110) * (g.r.chance(0.5f) ? 1 : -1), l2 = g.r.range(30, 90) * (g.r.chance(0.5f) ? 1 : -1);
        bool horizFirst = g.r.chance(0.5f);
        float bx = horizFirst ? sx + l1 : sx, by = horizFirst ? sy : sy + l1;
        float ex = horizFirst ? bx : bx + l2, ey = horizFirst ? by + l2 : by;
        g.line(sx, sy, bx, by, 2.4f, 3, ZB2, 3);
        g.line(bx, by, ex, ey, 2.4f, 3, ZB2, 3);
        float nodes[3][2] = { { sx, sy }, { bx, by }, { ex, ey } };
        for (auto& n : nodes) { g.rect(n[0], n[1], 7, 7, 3, ZB2, 4); g.rect(n[0], n[1], 3.6f, 3.6f, 7, ZB2, 5); }
    }
    // dust
    for (int i = 0; i < g.count(W * H / 2500.f); i++) g.rect(g.r.range(g.x0, g.x1), g.r.range(g.y0, g.y1), 2.5f, 2.5f, g.r.chance(0.7f) ? 2 : 3, ZB2, 6, g.r.range(0.3f, 0.8f));
}

void neonSkyline(SG& g) {
    // 0 far, 1 mid, 2 pink edge, 3 cyan edge, 4 window, 5 BG, 6 horizon glow, 7 moon, 8 cyan glow
    float W = g.W(), H = g.H();
    g.band(g.y0, H * 0.55f, 6, ZB4, 0, 0.8f);
    float mx = g.r.range(g.x0 + W * 0.2f, g.x1 - W * 0.2f), my = g.y0 + H * g.r.range(0.6f, 0.8f), mr = std::min(H * 0.18f, 70.f);
    g.radial(mx, my, mr * 3.2f, 6, ZB4, 1, 0.6f);
    g.obj(50, mx, my, mr / 15.f, mr / 15.f, 7, ZB4, 2);
    g.stars(g.count(W / 25.f), 7, ZB4, 0.3f);
    for (float x = g.x0; x < g.x1;) { float w = g.r.range(25, 60); g.building(x + w / 2, w, H * g.r.range(0.2f, 0.5f), g.y0, 0, ZB4, 3, 4, 0.12f); x += w + g.r.range(-6, 4); }
    for (float x = g.x0; x < g.x1;) {
        float w = g.r.range(30, 75);
        g.building(x + w / 2, w, H * g.r.range(0.12f, 0.35f), g.y0, 1, ZB3, 0, 4, 0.2f, 1.f, g.r.chance(0.5f) ? 2 : 3);
        x += w + g.r.range(10, 40);
    }
    for (int i = 0; i < g.count(W / 150.f); i++) { float x = g.r.range(g.x0, g.x1); g.line(x, g.y0, x + g.r.range(-30, 30), g.y0 + H * 0.9f, 3, 8, ZB4, 2, 0.35f); }
}

void pinkAmbition(SG& g) {
    // 0 far tower, 1 near tower, 2 light trim, 3 window warm, 4 cloud, 5 BG, 6 fog, 7 triangle, 8 warm glow
    float W = g.W(), H = g.H();
    g.band(g.y0, H * 0.7f, 6, ZB4, 0, 0.6f);
    float cx = g.r.range(g.x0 + W * 0.3f, g.x1 - W * 0.3f);
    // the big glowing arched window
    float aw = std::min(W * 0.18f, 80.f), ah = std::min(H * 0.55f, 160.f);
    g.radial(cx, g.y0 + ah * 0.6f, ah * 1.6f, 8, ZB4, 1, 0.7f);
    g.building(cx, aw * 1.8f, ah * 1.25f, g.y0, 0, ZB4, 2, -1, 0, 0.85f);
    g.rect(cx, g.y0 + ah / 2, aw, ah, 3, ZB4, 4, 0.85f);
    g.obj(50, cx, g.y0 + ah, aw / 30.f, aw / 30.f, 3, ZB4, 4, 0.85f);
    g.rect(cx, g.y0 + ah / 2, 2, ah, 0, ZB4, 5, 0.9f);
    // towers made of stacked blocks
    for (int layer = 0; layer < 2; layer++) {
        int slot = layer == 0 ? 0 : 1, z = layer == 0 ? ZB4 : ZB3;
        for (float x = g.x0; x < g.x1;) {
            float w = g.r.range(35, 80);
            if (std::abs(x + w / 2 - cx) < aw * 1.3f && layer == 0) { x += w; continue; }
            float y = g.y0, top = H * g.r.range(layer ? 0.15f : 0.35f, layer ? 0.4f : 0.8f);
            float ww = w;
            while (y < g.y0 + top) {
                float sh = g.r.range(15, 45);
                g.rect(x + w / 2, y + sh / 2, ww, sh, slot, z, 0, g.r.range(0.9f, 1.05f));
                g.rect(x + w / 2, y + sh - 1, ww + 4, 2.4f, 2, z, 1, layer ? 0.9f : 0.6f);
                if (g.r.chance(0.35f)) g.rect(x + w / 2 + g.r.sym(ww / 4), y + sh / 2, 5, sh * 0.5f, 3, z, 2, 0.8f);
                y += sh;
                ww = std::max(14.f, ww * g.r.range(0.65f, 1.05f));
            }
            x += w + g.r.range(layer ? 30.f : 5.f, layer ? 90.f : 25.f);
        }
    }
    g.clouds(g.count(W / 120.f), 4, ZB3, g.y0 + H * 0.7f, g.y1, 2.2f);
    g.band(g.y0, H * 0.2f, 6, ZB2, 0, 0.3f);
    for (int i = 0; i < g.count(W / 90.f); i++) {
        float x = g.r.range(g.x0, g.x1), y = g.r.range(g.y0 + H * 0.3f, g.y1);
        float s = g.r.range(1.2f, 2.8f);
        g.obj(1766, x, y, s, s, 7, ZB2, 3, 1.f, 180);
        g.obj(1766, x, y + 0.6f * s, s * 0.55f, s * 0.55f, 5, ZB2, 4, 1.f, 180);
    }
}

void galaxy(SG& g) {
    // 0 planet, 1 ring, 2 nebula A, 3 nebula B, 4 star, 5 BG, 6 planet shade
    float W = g.W(), H = g.H();
    for (int i = 0; i < g.count(std::max(2.f, W / 200.f)); i++)
        g.radial(g.r.range(g.x0, g.x1), g.r.range(g.y0, g.y1), g.r.range(120, 260), g.r.chance(0.5f) ? 2 : 3, ZB4, 0, g.r.range(0.4f, 0.75f));
    g.stars(g.count(W * H / 900.f), 4, ZB4, 0.4f);
    float px = g.r.range(g.x0 + W * 0.15f, g.x1 - W * 0.15f), py = g.r.range(g.y0 + H * 0.45f, g.y1 - H * 0.2f), pr = std::min(H * 0.22f, 90.f);
    g.radial(px, py, pr * 2.6f, 2, ZB4, 1, 0.8f);
    g.obj(50, px, py, pr / 15.f, pr / 15.f, 0, ZB3, 0);
    g.obj(50, px + pr * 0.25f, py - pr * 0.2f, pr / 15.f * 0.85f, pr / 15.f * 0.85f, 6, ZB3, 1, 1.f);
    g.obj(51, px, py, pr / 15.f * 1.7f, pr / 15.f * 0.45f, 1, ZB3, 2, 1.f, -18);
    for (int i = 0; i < g.count(W / 300.f); i++) {
        float x = g.r.range(g.x0, g.x1), y = g.r.range(g.y0 + H * 0.5f, g.y1), l = g.r.range(40, 90);
        g.line(x, y, x - l, y - l * 0.5f, 1.5f, 4, ZB3, 3, 0.8f);
        g.obj(1764, x, y, 0.5f, 0.5f, 4, ZB3, 4);
    }
}

void crystalCave(SG& g) {
    // 0 rock, 1 crystal, 2 shine, 3 glow, 4 BG, 5 rock light
    float W = g.W(), H = g.H();
    g.band(g.y0, H * 0.4f, 3, ZB4, 0, 0.6f);
    g.band(g.y1, H * 0.3f, 3, ZB4, 0, 0.4f, true);
    // stalactites from the top and rock bumps at the bottom
    for (float x = g.x0; x < g.x1; x += g.r.range(12, 30)) {
        float len = g.r.range(20, H * 0.35f), w = g.r.range(10, 26);
        g.obj(149, x, g.y1 - len / 2, w / 30.f, len / 30.f, g.r.chance(0.25f) ? 1 : 0, ZB3, 0, g.r.range(0.8f, 1.1f), 180);
    }
    for (float x = g.x0; x < g.x1; x += g.r.range(30, 70)) {
        float s = g.r.range(1.2f, 3.f);
        g.obj(50, x, g.y0, s, s * 0.6f, 0, ZB3, 0, g.r.range(0.8f, 1.f));
    }
    for (int i = 0; i < g.count(W / 40.f); i++) {
        float x = g.r.range(g.x0, g.x1), s = g.r.range(0.6f, 1.4f);
        int id = 151 + g.r.i(3);
        g.radial(x, g.y0 + 15 * s, 40 * s, 3, ZB4, 1, 0.5f);
        g.obj(id, x, g.y0 + 18 * s, s, s, 1, ZB3, 2, 1.f, g.r.sym(20));
        g.obj(153, x + 8 * s, g.y0 + 8 * s, s * 0.6f, s * 0.6f, 2, ZB3, 3, 1.f, 20);
    }
    for (int i = 0; i < g.count(W * H / 3000.f); i++) g.obj(53, g.r.range(g.x0, g.x1), g.r.range(g.y0, g.y1), 0.2f, 0.2f, 2, ZB2, 4, g.r.range(0.4f, 0.9f));
}

void sunnyHills(SG& g) {
    // 0 sky light, 1 sun, 2 cloud, 3 hill far, 4 hill near, 5 BG, 6 outline, 7 flower
    float W = g.W(), H = g.H();
    g.band(g.y0, H * 0.6f, 0, ZB4, 0, 0.7f);
    float sx = g.r.range(g.x0 + W * 0.1f, g.x1 - W * 0.1f), sy = g.y1 - H * 0.2f;
    g.radial(sx, sy, 140, 1, ZB4, 1, 0.5f);
    g.obj(50, sx, sy, 2.4f, 2.4f, 1, ZB4, 2);
    g.clouds(g.count(W / 140.f), 2, ZB4, g.y0 + H * 0.55f, g.y1 - 10, 1.4f, true);
    for (float x = g.x0 - 40; x < g.x1 + 40; x += g.r.range(70, 140)) { float s = g.r.range(4.f, 7.f); g.obj(50, x, g.y0 - s * 4, s, s * 0.8f, 3, ZB3, 0); }
    for (float x = g.x0 - 40; x < g.x1 + 40; x += g.r.range(60, 110)) {
        float s = g.r.range(3.f, 5.f);
        g.obj(50, x, g.y0 - s * 6, s + 0.25f, s * 0.75f + 0.25f, 6, ZB3, 1);
        g.obj(50, x, g.y0 - s * 6, s, s * 0.75f, 4, ZB3, 2);
        if (g.r.chance(0.6f)) { float fx = x + g.r.sym(s * 8); g.obj(1765, fx, g.y0 + 4, 1.6f, 1.6f, 7, ZB3, 3, 1.f, 45); }
    }
}

} // namespace

std::vector<Obj> generateScene(Template const& t, float x0, float y0, float x1, float y1, Options const& o) {
    if (x1 - x0 < 60) { float c = (x0 + x1) / 2; x0 = c - 30; x1 = c + 30; }
    if (y1 - y0 < 60) { float c = (y0 + y1) / 2; y0 = c - 30; y1 = c + 30; }
    SG g(o, x0, y0, x1, y1);
    switch (t.scene) {
        case 1: microcosm(g); break;
        case 2: neonSkyline(g); break;
        case 3: pinkAmbition(g); break;
        case 4: galaxy(g); break;
        case 5: crystalCave(g); break;
        default: sunnyHills(g); break;
    }
    // per-use colour variation
    Rng hr(hash(o.seed, 77));
    std::vector<float> hue(std::max<size_t>(t.palette.size(), 1));
    for (auto& h : hue) h = hr.sym(t.hueVar);
    for (auto& ob : g.out) {
        int s = std::clamp(ob.slot, 0, (int)hue.size() - 1);
        ob.h = hue[s];
        if (o.gradient) {
            float tt = std::clamp((ob.x - x0) / std::max(1.f, x1 - x0), 0.f, 1.f);
            ob.v *= 1.f - 0.4f * tt;
        }
        ob.prio = 3;
    }
    if (o.maxObjects > 0 && (int)g.out.size() > o.maxObjects) g.out.resize(o.maxObjects);
    return std::move(g.out);
}

std::vector<Template> const& builtinScenes() {
    static std::vector<Template> v = [] {
        auto S = [](int r, int g, int b, bool blend = false, int fixed = 0) { Slot s; s.c = { r, g, b }; s.blend = blend; s.fixed = fixed; s.force = fixed > 0; return s; };
        std::vector<Template> v;
        auto add = [&](char const* id, char const* name, char const* desc, int kind, std::vector<Slot> pal, float hueVar = 6) {
            Template t; t.id = id; t.name = name; t.by = "Scene"; t.desc = desc; t.scene = kind; t.palette = std::move(pal); t.hueVar = hueVar;
            v.push_back(t);
        };
        add("scene-microcosm", "Microcosm (scene)", "Red tile panels, cream frames & brackets, blue circuit wires", 1,
            { S(120, 26, 32), S(165, 44, 48), S(250, 226, 190), S(120, 200, 255), S(255, 110, 90, true), S(46, 6, 12, false, 1000), S(255, 246, 236), S(70, 10, 18) }, 4);
        add("scene-neon-skyline", "Neon Skyline (scene)", "Night city skyline, neon roof lines, big moon", 2,
            { S(26, 16, 52), S(40, 22, 70), S(255, 60, 190), S(60, 220, 255), S(255, 220, 140), S(10, 6, 24, false, 1000), S(230, 50, 170, true), S(255, 240, 250), S(60, 200, 255, true) });
        add("scene-pink-ambition", "Pink Ambition (scene)", "Pink fog, stacked pixel towers, glowing arched window", 3,
            { S(214, 120, 128), S(184, 86, 98), S(255, 205, 200), S(255, 222, 150), S(255, 190, 200), S(200, 110, 118, false, 1000), S(255, 200, 205, true), S(255, 120, 140), S(255, 210, 150, true) }, 4);
        add("scene-galaxy", "Galaxy (scene)", "Nebula clouds, stars, a ringed planet, shooting stars", 4,
            { S(110, 180, 255), S(230, 220, 255), S(140, 60, 255, true), S(255, 70, 190, true), S(255, 255, 255), S(8, 6, 26, false, 1000), S(30, 40, 110) }, 12);
        add("scene-crystal-cave", "Crystal Cave (scene)", "Stalactites, glowing crystals and sparkles", 5,
            { S(36, 18, 58), S(190, 95, 255), S(240, 200, 255), S(150, 60, 255, true), S(16, 8, 28, false, 1000), S(70, 40, 100) }, 10);
        add("scene-sunny-hills", "Sunny Hills (scene)", "Cartoon sky, sun, fluffy clouds and round hills", 6,
            { S(230, 250, 255, true), S(255, 230, 120), S(255, 255, 255), S(150, 215, 140), S(110, 195, 100), S(110, 190, 240, false, 1000), S(50, 90, 70), S(255, 130, 190) });
        return v;
    }();
    return v;
}

} // namespace deco
