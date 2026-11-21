#include "Deco.hpp"
#include <algorithm>
#include <cmath>
#include <deque>
#include <unordered_map>

namespace deco {

uint64_t hash(uint64_t a, uint64_t b) {
    uint64_t x = a * 0x9E3779B97F4A7C15ull ^ (b + 0x632BE59BD9B4E019ull + (a << 6) + (a >> 2));
    x ^= x >> 33; x *= 0xff51afd7ed558ccdull;
    x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ull;
    x ^= x >> 33;
    return x;
}

// ---------------- Grid ----------------
void Grid::add(int x, int y) {
    if (m_cells.empty()) { minX = maxX = x; minY = maxY = y; }
    m_cells.insert(key(x, y));
    minX = std::min(minX, x); maxX = std::max(maxX, x);
    minY = std::min(minY, y); maxY = std::max(maxY, y);
}
void Grid::addHazard(int x, int y) { m_hazards.insert(key(x, y)); }
bool Grid::has(int x, int y) const { return m_cells.count(key(x, y)) != 0; }
bool Grid::hazard(int x, int y) const { return m_hazards.count(key(x, y)) != 0; }
std::vector<std::pair<int,int>> Grid::cells() const {
    std::vector<std::pair<int,int>> v;
    v.reserve(m_cells.size());
    for (auto k : m_cells) v.push_back({ (int)(k >> 32), (int)(unsigned)(k & 0xffffffffull) });
    std::sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.second != b.second ? a.second < b.second : a.first < b.first; });
    return v;
}

namespace {

constexpr int DX[4] = { 0, 1, 0, -1 };   // Top, Right, Bottom, Left
constexpr int DY[4] = { 1, 0, -1, 0 };

struct Run { int fixed, a, b; };          // top/bottom: fixed=y, a..b = x ; left/right: fixed=x, a..b = y

struct Frame { float tx, ty, nx, ny, rot; };
Frame frameFor(int side) {
    switch (side) {
        case 0: return { 1, 0, 0, 1, 0 };
        case 1: return { 0, -1, 1, 0, 90 };
        case 2: return { -1, 0, 0, -1, 180 };
        default: return { 0, 1, -1, 0, 270 };
    }
}

struct Gen {
    Template const& t;
    Grid const& g;
    Options const& o;
    std::vector<Obj> out;
    std::vector<std::pair<int,int>> cells;
    std::unordered_map<long long, int> depth;
    float axis2 = 0;             // reflection: x' = axis2 - x
    int cxSum = 0;               // mirrored cell x = cxSum - cx
    std::vector<float> hue;
    std::vector<int> slotMap;
    int fi = 0;                  // current feature index
    int prio = 0;

    Gen(Template const& t, Grid const& g, Options const& o) : t(t), g(g), o(o) {}

    static long long key(int x, int y) { return ((long long)x << 32) ^ (unsigned)y; }
    bool has(int x, int y) const { return g.has(x, y); }
    bool exposed(int x, int y, int s) const { return has(x, y) && !has(x + DX[s], y + DY[s]); }
    int depthOf(int x, int y) const { auto it = depth.find(key(x, y)); return it == depth.end() ? -1 : it->second; }

    int mapSlot(int s) const {
        if (s < 0) return s;
        if (s < (int)slotMap.size()) return slotMap[s];
        return s;
    }

    // RNG for one decision spot; with mirror on, both halves get the same random numbers
    Rng rngAt(int cx, int cy, int side, int extra, bool& mirrored) const {
        mirrored = false;
        int ccx = cx, cside = side;
        if (o.mirror) {
            int mx = cxSum - cx;
            if (cx > mx) {
                mirrored = true; ccx = mx;
                if (side == 1) cside = 3; else if (side == 3) cside = 1;
            }
        }
        uint64_t h = hash(o.seed, (uint64_t)fi * 7919 + 13);
        h = hash(h, (uint64_t)(uint32_t)ccx);
        h = hash(h, (uint64_t)(uint32_t)cy);
        h = hash(h, (uint64_t)(cside + 16 * extra));
        return Rng(h);
    }

    float effProb(float p) const { return std::clamp(p * o.density, 0.f, 1.f); }

    // place one piece. (ax, ay) = anchor, frame = local frame, rng for jitter.
    void emit(Piece const& p, float ax, float ay, Frame const& fr, bool orient, bool mirrored, Rng& r,
              float extraV = 1.f, bool cornerFlipX = false, bool cornerFlipY = false) {
        // when mirrored we compute everything at the reflected anchor and reflect the result back
        float bx = mirrored ? axis2 - ax : ax;
        Frame f = fr;
        if (mirrored) {
            // the canonical frame: left <-> right sides
            if (fr.rot == 90) f = frameFor(3); else if (fr.rot == 270) f = frameFor(1);
        }
        float dx = p.dx, dy = p.dy;
        float syMul = 1.f;
        if (p.stretch > 0.f) {
            syMul = r.range(1.f, 1.f + p.stretch);
            dy += (syMul - 1.f) * p.hBase * p.sy / 2.f;
        }
        if (cornerFlipX) dx = -dx;
        if (cornerFlipY) dy = -dy;
        float jx = r.sym(p.jPos), jy = r.sym(p.jPos);
        float x = bx + dx * f.tx + dy * f.nx + jx;
        float y = ay + dx * f.ty + dy * f.ny + jy;
        float rot = p.rot + r.sym(p.jRot);
        bool fx = p.fx, fy = p.fy;
        if (p.randFlip && r.chance(0.5f)) fx = !fx;
        if (cornerFlipX && orient) { fx = !fx; rot = -rot; }
        if (cornerFlipY && orient) { fy = !fy; rot = -rot; }
        if (orient) rot += f.rot;
        float sc = 1.f + r.sym(p.jScale);
        Obj ob;
        ob.id = p.id;
        ob.x = x; ob.y = y; ob.rot = rot;
        ob.sx = p.sx * sc; ob.sy = p.sy * sc * syMul;
        ob.fx = fx; ob.fy = fy;
        if (mirrored) { ob.x = axis2 - ob.x; ob.rot = -ob.rot; ob.fx = !ob.fx; }
        ob.slot = mapSlot(p.slot);
        ob.slot2 = mapSlot(p.slot2 < 0 ? p.slot : p.slot2);
        ob.z = p.z; ob.zo = p.zo;
        ob.anim = p.anim;
        ob.v = p.v * extraV;
        ob.prio = prio;
        // normalise rotation
        while (ob.rot > 180) ob.rot -= 360;
        while (ob.rot <= -180) ob.rot += 360;
        out.push_back(ob);
    }

    Cluster const* pick(Feature const& f, Rng& r) const {
        if (f.opts.empty()) return nullptr;
        float tot = 0;
        for (auto& c : f.opts) tot += c.weight;
        float x = r.f() * tot;
        for (auto& c : f.opts) { x -= c.weight; if (x <= 0) return &c; }
        return &f.opts.back();
    }

    void emitCluster(Cluster const& c, float ax, float ay, Frame const& fr, bool orient, bool mirrored, Rng& r,
                     float vj, bool cfx = false, bool cfy = false) {
        // one shared position jitter for the whole cluster keeps compositions together
        for (auto& p : c.pieces) emit(p, ax, ay, fr, orient, mirrored, r, vj, cfx, cfy);
    }

    // a plain rectangle made of the 15x15 white square (or another square id)
    void rect(int id, float cx, float cy, float w, float h, int slot, int z, int zo, float v = 1.f, float base = 15.f) {
        if (w <= 0.05f || h <= 0.05f) return;
        Obj ob;
        ob.id = id; ob.x = cx; ob.y = cy;
        ob.sx = w / base; ob.sy = h / base;
        ob.slot = ob.slot2 = mapSlot(slot);
        ob.z = z; ob.zo = zo; ob.v = v; ob.prio = prio;
        out.push_back(ob);
    }
    void line(float x0, float y0, float x1, float y1, float th, int slot, int z, int zo, float v = 1.f) {
        float dx = x1 - x0, dy = y1 - y0;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.1f) return;
        Obj ob;
        ob.id = 916; ob.x = (x0 + x1) / 2; ob.y = (y0 + y1) / 2;
        ob.sx = len / 15.f; ob.sy = th / 15.f;
        ob.rot = -std::atan2(dy, dx) * 180.f / 3.14159265f;
        ob.slot = ob.slot2 = mapSlot(slot);
        ob.z = z; ob.zo = zo; ob.v = v; ob.prio = prio;
        out.push_back(ob);
    }
    void tri(float cx, float cy, float w, float h, float rot, int slot, int z, int zo, float v = 1.f) {
        // 693 = right triangle, right angle at bottom-right, 30x30
        Obj ob;
        ob.id = 693; ob.x = cx; ob.y = cy; ob.rot = rot;
        ob.sx = w / 30.f; ob.sy = h / 30.f;
        ob.slot = ob.slot2 = mapSlot(slot);
        ob.z = z; ob.zo = zo; ob.v = v; ob.prio = prio;
        out.push_back(ob);
    }

    std::vector<Run> runs(int side) const {
        std::vector<Run> rs;
        if (side == 0 || side == 2) {
            // rows
            std::vector<std::pair<int,int>> e;
            for (auto& c : cells) if (exposed(c.first, c.second, side)) e.push_back({ c.second, c.first });
            std::sort(e.begin(), e.end());
            for (size_t i = 0; i < e.size();) {
                size_t j = i;
                while (j + 1 < e.size() && e[j + 1].first == e[i].first && e[j + 1].second == e[j].second + 1) j++;
                rs.push_back({ e[i].first, e[i].second, e[j].second });
                i = j + 1;
            }
        } else {
            std::vector<std::pair<int,int>> e;
            for (auto& c : cells) if (exposed(c.first, c.second, side)) e.push_back({ c.first, c.second });
            std::sort(e.begin(), e.end());
            for (size_t i = 0; i < e.size();) {
                size_t j = i;
                while (j + 1 < e.size() && e[j + 1].first == e[i].first && e[j + 1].second == e[j].second + 1) j++;
                rs.push_back({ e[i].first, e[i].second, e[j].second });
                i = j + 1;
            }
        }
        return rs;
    }

    // ---------- features ----------
    void fill(Feature const& f) {
        // merge rows into long rectangles
        std::vector<std::pair<int,int>> e;
        for (auto& c : cells) e.push_back({ c.second, c.first });
        std::sort(e.begin(), e.end());
        for (size_t i = 0; i < e.size();) {
            size_t j = i;
            while (j + 1 < e.size() && e[j + 1].first == e[i].first && e[j + 1].second == e[j].second + 1) j++;
            float x0 = e[i].second * CELL, x1 = (e[j].second + 1) * CELL, y = e[i].first * CELL;
            rect(f.lineId, (x0 + x1) / 2, y + CELL / 2, x1 - x0, CELL, f.slot, f.z, f.zo, f.v);
            i = j + 1;
        }
    }

    // broken lines: segments of random length with gaps, always keeping the two ends (corner brackets look)
    void dashSegs(Feature const& f, float a0, float a1, int cx, int cy, int s, std::vector<std::pair<float,float>>& out) {
        bool m; Rng r = rngAt(cx, cy, s, 9, m);
        float len = a1 - a0;
        if (len < f.dash * 1.6f) { out.push_back({ a0, a1 }); return; }
        float pos = a0;
        bool on = true;
        while (pos < a1 - 0.5f) {
            float seg = on ? f.dash * r.range(0.6f, 1.6f) : f.dash * r.range(0.3f, 0.9f);
            float e = std::min(a1, pos + seg);
            if (a1 - e < f.dash * 0.5f) e = a1;           // don't leave a tiny piece at the end
            if (on) out.push_back({ pos, e });
            pos = e; on = !on;
        }
    }
    void dashH(Feature const& f, float x0, float x1, float cy, int cx, int y, int s) {
        std::vector<std::pair<float,float>> segs; dashSegs(f, x0, x1, cx, y, s, segs);
        for (auto& sg : segs) rect(f.lineId, (sg.first + sg.second) / 2, cy, sg.second - sg.first, f.thick, f.slot, f.z, f.zo, f.v);
    }
    void dashV(Feature const& f, float y0, float y1, float cx, int x, int cy, int s) {
        std::vector<std::pair<float,float>> segs; dashSegs(f, y0, y1, x, cy, s, segs);
        for (auto& sg : segs) rect(f.lineId, cx, (sg.first + sg.second) / 2, f.thick, sg.second - sg.first, f.slot, f.z, f.zo, f.v);
    }

    // edge line runs with correct joins at corners
    void outline(Feature const& f) {
        float t = f.thick, ins = f.inset;
        float eConvex = ins >= 0 ? -ins : (-ins + t);
        float eConcave = ins >= 0 ? (ins + t) : ins;   // ins<0 -> shrink by -ins
        for (int s = 0; s < 4; s++) {
            if (!(f.sides & (1u << s))) continue;
            for (auto& rn : runs(s)) {
                if (s == 0 || s == 2) {
                    int y = rn.fixed;
                    float x0 = rn.a * CELL, x1 = (rn.b + 1) * CELL;
                    x0 -= has(rn.a - 1, y) ? eConcave : eConvex;
                    x1 += has(rn.b + 1, y) ? eConcave : eConvex;
                    float edge = s == 0 ? (y + 1) * CELL : y * CELL;
                    float nrm = s == 0 ? 1.f : -1.f;
                    float cy = edge - nrm * (ins + t / 2);
                    if (f.dash > 0) dashH(f, x0, x1, cy, rn.a, y, s);
                    else rect(f.lineId, (x0 + x1) / 2, cy, x1 - x0, t, f.slot, f.z, f.zo, f.v);
                    if (!f.opts.empty()) {
                        Frame idf = { 1, 0, 0, 1, 0 };
                        if (!has(rn.a - 1, y)) { bool m; Rng r = rngAt(rn.a, y, s, 5, m); if (auto c = pick(f, r)) emitCluster(*c, x0 + t / 2, cy, idf, false, false, r, 1.f); }
                        if (!has(rn.b + 1, y)) { bool m; Rng r = rngAt(rn.b, y, s, 6, m); if (auto c = pick(f, r)) emitCluster(*c, x1 - t / 2, cy, idf, false, false, r, 1.f); }
                    }
                } else {
                    int x = rn.fixed;
                    float y0 = rn.a * CELL, y1 = (rn.b + 1) * CELL;
                    y0 -= has(x, rn.a - 1) ? eConcave : eConvex;
                    y1 += has(x, rn.b + 1) ? eConcave : eConvex;
                    float edge = s == 1 ? (x + 1) * CELL : x * CELL;
                    float nrm = s == 1 ? 1.f : -1.f;
                    float cx = edge - nrm * (ins + t / 2);
                    if (f.dash > 0) dashV(f, y0, y1, cx, x, rn.a, s);
                    else rect(f.lineId, cx, (y0 + y1) / 2, t, y1 - y0, f.slot, f.z, f.zo, f.v);
                }
            }
        }
    }

    void glow(Feature const& f) {
        float gs = f.glowSize;
        int edgeId = f.lineId == 916 ? 503 : f.lineId;
        for (int s = 0; s < 4; s++) {
            if (!(f.sides & (1u << s))) continue;
            Frame fr = frameFor(s);
            for (auto& rn : runs(s)) {
                float a0 = rn.a * CELL, a1 = (rn.b + 1) * CELL;
                if (f.inward) {
                    // shrink at concave ends so the inner glow doesn't poke into walls
                    if (!has(s == 0 || s == 2 ? rn.a - 1 : rn.fixed, s == 0 || s == 2 ? rn.fixed : rn.a - 1)) a0 += 0;
                }
                float len = a1 - a0, mid = (a0 + a1) / 2;
                float edge;
                Obj ob;
                ob.id = edgeId;
                ob.sx = len / 30.f; ob.sy = gs;
                ob.slot = ob.slot2 = mapSlot(f.slot);
                ob.z = f.z; ob.zo = f.zo; ob.v = f.v; ob.prio = prio;
                float gsUse = gs;
                if (f.inward) {
                    // thin parts: keep the inner glow inside the block
                    int ix = (s == 0 || s == 2) ? rn.a : rn.fixed, iy = (s == 0 || s == 2) ? rn.fixed : rn.a;
                    int thick = 0;
                    int ddx = -DX[s], ddy = -DY[s];
                    while (thick < 4 && has(ix + ddx * thick, iy + ddy * thick)) thick++;
                    float maxH = thick * CELL * 0.45f;
                    if (20.f * gsUse > maxH) gsUse = maxH / 20.f;
                    ob.sy = gsUse;
                }
                float h2 = 20.f * gsUse;
                float off = f.inward ? -h2 / 2 : h2 / 2;
                if (s == 0) { edge = (rn.fixed + 1) * CELL; ob.x = mid; ob.y = edge + off; }
                else if (s == 2) { edge = rn.fixed * CELL; ob.x = mid; ob.y = edge - off; }
                else if (s == 1) { edge = (rn.fixed + 1) * CELL; ob.y = mid; ob.x = edge + off; }
                else { edge = rn.fixed * CELL; ob.y = mid; ob.x = edge - off; }
                ob.rot = fr.rot + (f.inward ? 180.f : 0.f);
                while (ob.rot > 180) ob.rot -= 360;
                out.push_back(ob);
            }
        }
        if (!f.corners || f.inward) return;
        // outer convex corners: 504 (20x20, bright at its bottom-right)
        float c = 10.f * gs;
        for (auto& cl : cells) {
            int x = cl.first, y = cl.second;
            bool T = !has(x, y + 1), B = !has(x, y - 1), L = !has(x - 1, y), R = !has(x + 1, y);
            auto put = [&](bool ok, int dxs, int dys, float px, float py, bool fx, bool fy) {
                if (!ok || has(x + dxs, y + dys)) return;
                Obj ob; ob.id = 504; ob.sx = ob.sy = gs;
                ob.x = px + (dxs < 0 ? -c : c); ob.y = py + (dys < 0 ? -c : c);
                ob.fx = fx; ob.fy = fy;
                ob.slot = ob.slot2 = mapSlot(f.slot); ob.z = f.z; ob.zo = f.zo; ob.v = f.v; ob.prio = prio;
                out.push_back(ob);
            };
            float x0 = x * CELL, x1 = x0 + CELL, y0 = y * CELL, y1 = y0 + CELL;
            if (f.sides & 1u && f.sides & 8u) put(T && L, -1, 1, x0, y1, false, false);
            if (f.sides & 1u && f.sides & 2u) put(T && R, 1, 1, x1, y1, true, false);
            if (f.sides & 4u && f.sides & 8u) put(B && L, -1, -1, x0, y0, false, true);
            if (f.sides & 4u && f.sides & 2u) put(B && R, 1, -1, x1, y0, true, true);
        }
    }

    void extrude(Feature const& f) {
        float ex = f.ex, ey = f.ey;
        // top faces
        for (auto& rn : runs(0)) {
            int y = rn.fixed;
            float x0 = rn.a * CELL, x1 = (rn.b + 1) * CELL, yt = (y + 1) * CELL;
            bool wallRight = has(rn.b + 1, y);      // concave: a wall rises at the right end
            bool wallLeft = has(rn.a - 1, y);
            float rx0 = wallLeft ? x0 : x0 + ex;
            rect(916, (rx0 + x1) / 2, yt + ey / 2, x1 - rx0, ey, f.slotTop, f.z, f.zo, f.v);
            if (!wallLeft) tri(x0 + ex / 2, yt + ey / 2, ex, ey, 0, f.slotTop, f.z, f.zo, f.v);
            if (!wallRight && !has(rn.b + 1, y + 1)) tri(x1 + ex / 2, yt + ey / 2, ex, ey, 180, f.slotTop, f.z, f.zo, f.v);
            if (f.lineSlot >= 0) {
                float lt = f.lineThick;
                float lx1 = wallRight ? x1 : x1 + ex;
                line(rx0, yt + ey - lt / 2, lx1, yt + ey - lt / 2, lt, f.lineSlot, f.z, f.zo + 1);
                if (!wallLeft) line(x0, yt, x0 + ex, yt + ey, lt, f.lineSlot, f.z, f.zo + 1);
            }
        }
        // right side faces
        for (auto& rn : runs(1)) {
            int x = rn.fixed;
            float y0 = rn.a * CELL, y1 = (rn.b + 1) * CELL, xr = (x + 1) * CELL;
            bool floorBelow = has(x + 1, rn.a - 1);   // something under the face's start
            bool capAbove = has(x, rn.b + 1);          // block continues above -> no top corner
            float ry0 = y0 + ey;
            rect(916, xr + ex / 2, (ry0 + y1) / 2, ex, y1 - ry0, f.slotSide, f.z, f.zo, f.v);
            if (!floorBelow) tri(xr + ex / 2, y0 + ey / 2, ex, ey, 180, f.slotSide, f.z, f.zo, f.v);
            else rect(916, xr + ex / 2, y0 + ey / 2, ex, ey, f.slotSide, f.z, f.zo, f.v);
            if (!capAbove) tri(xr + ex / 2, y1 + ey / 2, ex, ey, 0, f.slotSide, f.z, f.zo, f.v);
            if (f.lineSlot >= 0) {
                float lt = f.lineThick;
                line(xr + ex - lt / 2, ry0, xr + ex - lt / 2, y1 + (capAbove ? 0 : ey), lt, f.lineSlot, f.z, f.zo + 1);
                if (!floorBelow) line(xr, y0, xr + ex, y0 + ey, lt, f.lineSlot, f.z, f.zo + 1);
                if (!capAbove) line(xr, y1, xr + ex, y1 + ey, lt, f.lineSlot, f.z, f.zo + 1);
            }
        }
    }

    void surface(Feature const& f, float probUse) {
        int s = (int)f.side;
        Frame fr = frameFor(s);
        for (auto& rn : runs(s)) {
            int n = rn.b - rn.a + 1;
            for (int k = 0; k < n; k++) {
                int cx, cy;
                if (s == 0 || s == 2) { cx = rn.a + k; cy = rn.fixed; } else { cx = rn.fixed; cy = rn.a + k; }
                if (f.runEnds && k != 0 && k != n - 1) continue;
                if (f.skipHazard && g.hazard(cx + DX[s], cy + DY[s])) continue;
                bool mir;
                Rng r = rngAt(cx, cy, s, 0, mir);
                if (f.spacing > 1) {
                    // phase from the run start keeps rows tidy
                    bool m2; Rng rp = rngAt(s == 0 || s == 2 ? rn.a : rn.fixed, s == 0 || s == 2 ? rn.fixed : rn.a, s, 7, m2);
                    int phase = f.runEnds ? 0 : rp.i(f.spacing);
                    if ((k + phase) % f.spacing != 0 && !(f.runEnds)) continue;
                }
                if (!r.chance(probUse)) continue;
                Cluster const* c = pick(f, r);
                if (!c) continue;
                float ax, ay;
                switch (s) {
                    case 0: ax = cx * CELL + 15; ay = (cy + 1) * CELL; break;
                    case 1: ax = (cx + 1) * CELL; ay = cy * CELL + 15; break;
                    case 2: ax = cx * CELL + 15; ay = cy * CELL; break;
                    default: ax = cx * CELL; ay = cy * CELL + 15; break;
                }
                emitCluster(*c, ax, ay, fr, f.orient, mir, r, 1.f + r.sym(0.06f));
            }
        }
    }

    void corner(Feature const& f, float probUse) {
        Frame id = { 1, 0, 0, 1, 0 };
        static const int swapLR[4] = { 1, 0, 3, 2 };
        // convex corners: a cell with two exposed sides
        for (auto& cl : cells) {
            int x = cl.first, y = cl.second;
            bool T = !has(x, y + 1), B = !has(x, y - 1), L = !has(x - 1, y), R = !has(x + 1, y);
            struct C { bool ok; int bit; float px, py; bool fx, fy; };
            float x0 = x * CELL, x1 = x0 + CELL, y0 = y * CELL, y1 = y0 + CELL;
            C cs[4] = {
                { T && L, 0, x0, y1, true, false },
                { T && R, 1, x1, y1, false, false },
                { B && L, 2, x0, y0, true, true },
                { B && R, 3, x1, y0, false, true },
            };
            if (!f.convex) break;
            for (auto& c : cs) {
                if (!c.ok || !(f.cornerMask & (1u << c.bit))) continue;
                bool mirNow = o.mirror && x > cxSum - x;
                int cbit = mirNow ? swapLR[c.bit] : c.bit;
                bool mir;
                Rng r = rngAt(x, y, 4 + cbit, 1, mir);
                if (!r.chance(probUse)) continue;
                auto cl2 = pick(f, r);
                if (!cl2) continue;
                // frame: +x outward horizontally, +y outward vertically -> flips
                bool cfx = mir ? !c.fx : c.fx;
                emitCluster(*cl2, c.px, c.py, id, f.orient, mir, r, 1.f, cfx, c.fy);
            }
        }
        if (!f.concave) return;
        // concave corners: lattice points with exactly 3 of 4 cells filled
        std::unordered_set<long long> seen;
        for (auto& cl : cells) {
            for (int ox = 0; ox <= 1; ox++) for (int oy = 0; oy <= 1; oy++) {
                int px = cl.first + ox, py = cl.second + oy;  // lattice point (px*30, py*30)
                if (!seen.insert(key(px, py)).second) continue;
                bool a = has(px - 1, py - 1), b = has(px, py - 1), c = has(px - 1, py), d = has(px, py);
                if (a + b + c + d != 3) continue;
                bool fx = false, fy = false; int bit;
                if (!c) { fx = true; bit = 0; }                   // empty top-left
                else if (!d) { bit = 1; }                         // empty top-right
                else if (!a) { fx = true; fy = true; bit = 2; }   // empty bottom-left
                else { fy = true; bit = 3; }                      // empty bottom-right
                if (!(f.cornerMask & (1u << bit))) continue;
                int mpx = cxSum + 1 - px;
                bool mir = o.mirror && px > mpx;
                int cpx = mir ? mpx : px, cbit = mir ? swapLR[bit] : bit;
                uint64_t h = hash(hash(o.seed, (uint64_t)fi * 7919 + 99), (uint64_t)(uint32_t)cpx);
                h = hash(hash(h, (uint64_t)(uint32_t)py), (uint64_t)cbit);
                Rng r(h);
                if (!r.chance(probUse)) continue;
                auto cl2 = pick(f, r);
                if (!cl2) continue;
                emitCluster(*cl2, px * CELL, py * CELL, id, f.orient, mir, r, 1.f, mir ? !fx : fx, fy);
            }
        }
    }

    void interior(Feature const& f, float probUse) {
        Frame id = { 1, 0, 0, 1, 0 };
        Rng pr(hash(o.seed, (uint64_t)fi * 31 + 5));
        int ox = f.randomPhase ? pr.i(std::max(1, f.px)) : 0;
        int oy = f.randomPhase ? pr.i(std::max(1, f.py)) : 0;
        // keep patterns symmetric: base phase on the structure's left edge
        int baseX = g.minX + ox, baseY = g.minY + oy;
        for (auto& cl : cells) {
            int x = cl.first, y = cl.second;
            if (depthOf(x, y) < f.minDepth) continue;
            int lx = x;
            if (o.mirror) { int mx = cxSum - x; if (x > mx) lx = mx; }
            int rx = ((lx - baseX) % std::max(1, f.px) + f.px) % std::max(1, f.px);
            int ry = ((y - baseY) % std::max(1, f.py) + f.py) % std::max(1, f.py);
            bool ok = true;
            switch (f.pat) {
                case Pattern::Windows: ok = rx == 0 && ry == 0; break;
                case Pattern::Checker: ok = ((lx + y) & 1) == 0; break;
                case Pattern::Rows: ok = ry == 0; break;
                case Pattern::Columns: ok = rx == 0; break;
                case Pattern::Every: ok = true; break;
                case Pattern::Random: ok = true; break;
            }
            if (!ok) continue;
            bool mir;
            Rng r = rngAt(x, y, 12, 3, mir);
            if (!r.chance(probUse)) continue;
            auto c = pick(f, r);
            if (!c) continue;
            emitCluster(*c, x * CELL + 15, y * CELL + 15, id, f.orient, mir, r, 1.f + r.sym(0.05f));
        }
    }

    void band(Feature const& f) {
        // a line under the top surface, following runs whose cells are deep enough
        int needCells = std::max(1, (int)std::ceil((f.dist + f.thick) / CELL));
        for (auto& rn : runs(0)) {
            int y = rn.fixed;
            int start = -1;
            for (int x = rn.a; x <= rn.b + 1; x++) {
                bool ok = x <= rn.b;
                for (int k = 1; ok && k < needCells; k++) ok = has(x, y - k);
                if (ok && start < 0) start = x;
                if (!ok && start >= 0) {
                    float x0 = start * CELL, x1 = x * CELL;
                    float yy = (y + 1) * CELL - f.dist - f.thick / 2;
                    // stay inside an outline of width f.inset at open ends
                    if (!has(start - 1, y)) x0 += f.inset;
                    if (!has(x, y)) x1 -= f.inset;
                    rect(f.lineId, (x0 + x1) / 2, yy, x1 - x0, f.thick, f.slot, f.z, f.zo, f.v);
                    start = -1;
                }
            }
        }
    }

    void ledge(Feature const& f) {
        float t = f.thick, sink = f.inset;
        for (auto& rn : runs(0)) {
            int y = rn.fixed;
            float x0 = rn.a * CELL, x1 = (rn.b + 1) * CELL, yt = (y + 1) * CELL;
            if (!has(rn.a - 1, y)) x0 -= f.over;
            if (!has(rn.b + 1, y)) x1 += f.over;
            float cy = yt - sink + t / 2;
            rect(f.lineId, (x0 + x1) / 2, cy, x1 - x0, t, f.slot, f.z, f.zo, f.v);
            float lt = std::max(1.f, t * 0.28f);
            if (f.slotEdge >= 0) rect(916, (x0 + x1) / 2, cy + t / 2 - lt / 2, x1 - x0, lt, f.slotEdge, f.z, f.zo + 1, f.v);
            if (f.slotUnder >= 0) rect(916, (x0 + x1) / 2, cy - t / 2 + lt / 2, x1 - x0, lt, f.slotUnder, f.z, f.zo + 1, f.v);
        }
    }

    void panel(Feature const& f) {
        // a smaller rectangle inside the blocks (inset on every open side)
        float in = f.inset;
        std::vector<std::pair<int,int>> e;
        for (auto& c : cells) e.push_back({ c.second, c.first });
        std::sort(e.begin(), e.end());
        for (size_t i = 0; i < e.size();) {
            int y = e[i].first, x = e[i].second;
            bool T = !has(x, y + 1), B = !has(x, y - 1);
            size_t j = i;
            while (j + 1 < e.size() && e[j + 1].first == y && e[j + 1].second == e[j].second + 1 &&
                   !has(e[j + 1].second, y + 1) == T && !has(e[j + 1].second, y - 1) == B) j++;
            float x0 = e[i].second * CELL, x1 = (e[j].second + 1) * CELL, y0 = y * CELL, y1 = y0 + CELL;
            if (!has(e[i].second - 1, y)) x0 += in;
            if (!has(e[j].second + 1, y)) x1 -= in;
            if (B) y0 += in;
            if (T) y1 -= in;
            rect(f.lineId, (x0 + x1) / 2, (y0 + y1) / 2, x1 - x0, y1 - y0, f.slot, f.z, f.zo, f.v);
            i = j + 1;
        }
    }

    // circuit wires: leave the block, bend once, end in a square node
    void wire(Feature const& f, float probUse) {
        for (int s = 0; s < 4; s++) {
            if (!(f.sides & (1u << s))) continue;
            for (auto& rn : runs(s)) {
                for (int k = rn.a; k <= rn.b; k++) {
                    int cx = (s == 0 || s == 2) ? k : rn.fixed, cy = (s == 0 || s == 2) ? rn.fixed : k;
                    bool mir; Rng r = rngAt(cx, cy, s, 11, mir);
                    if (!r.chance(probUse)) continue;
                    float sx = cx * CELL + 15 + DX[s] * 15, sy = cy * CELL + 15 + DY[s] * 15;
                    float l1 = r.range(f.wireMin, f.wireMax), l2 = r.range(f.wireMin, f.wireMax);
                    int turn = r.chance(0.5f) ? 1 : -1;
                    if (mir && (s == 0 || s == 2)) turn = -turn;
                    float bx = sx + DX[s] * l1, by = sy + DY[s] * l1;
                    // turn direction: perpendicular
                    float tx = (float)(DY[s] * turn), ty = (float)(-DX[s] * turn);
                    float ex = bx + tx * l2, ey = by + ty * l2;
                    // keep wires out of the structure and away from hazards
                    bool ok = true;
                    for (int q = 1; q <= 8 && ok; q++) {
                        float t = q / 8.f;
                        float px1 = sx + (bx - sx) * t, py1 = sy + (by - sy) * t;
                        float px2 = bx + (ex - bx) * t, py2 = by + (ey - by) * t;
                        int a1 = (int)std::floor(px1 / CELL), b1 = (int)std::floor(py1 / CELL);
                        int a2 = (int)std::floor(px2 / CELL), b2 = (int)std::floor(py2 / CELL);
                        if ((q > 1 && has(a1, b1)) || has(a2, b2) || g.hazard(a1, b1) || g.hazard(a2, b2)) ok = false;
                    }
                    if (!ok) continue;
                    line(sx, sy, bx, by, f.thick, f.slot, f.z, f.zo);
                    line(bx - tx * f.thick / 2, by - ty * f.thick / 2, ex, ey, f.thick, f.slot, f.z, f.zo);
                    int ns = f.nodeSlot >= 0 ? f.nodeSlot : f.slot;
                    for (int n = 0; n < 2; n++) {
                        float nx = n == 0 ? bx : ex, ny = n == 0 ? by : ey;
                        rect(916, nx, ny, f.node, f.node, f.slot, f.z, f.zo + 1);
                        rect(916, nx, ny, f.node * 0.55f, f.node * 0.55f, ns, f.z, f.zo + 2);
                    }
                }
            }
        }
    }

    void scatter(Feature const& f) {
        Frame id = { 1, 0, 0, 1, 0 };
        // candidate empty cells touching the structure
        std::vector<std::pair<int,int>> cand;
        std::unordered_set<long long> seen;
        for (auto& cl : cells) {
            for (int dx = -f.margin; dx <= f.margin; dx++) for (int dy = -f.margin; dy <= f.margin; dy++) {
                int x = cl.first + dx, y = cl.second + dy;
                if (g.hazard(x, y)) continue;
                bool touching = false;
                for (int a = -1; a <= 1 && !touching; a++) for (int b = -1; b <= 1; b++) if (has(x + a, y + b)) { touching = true; break; }
                if (f.hugEdge) {
                    // on the outline: an edge cell or an empty cell right next to the structure
                    if (has(x, y) ? depthOf(x, y) != 0 : !touching) continue;
                } else {
                    // keep one empty cell between the structure and floating pieces
                    if (has(x, y) || touching) continue;
                }
                if (o.mirror) { int mx = cxSum - x; if (x > mx) continue; }
                if (seen.insert(key(x, y)).second) cand.push_back({ x, y });
            }
        }
        std::sort(cand.begin(), cand.end());
        Rng r(hash(o.seed, (uint64_t)fi * 101 + 77));
        int want = (int)std::round(f.perCell * (float)cells.size() * o.density * (o.mirror ? 0.5f : 1.f));
        want = std::min<int>(want, (int)cand.size());
        // partial shuffle
        for (int i = 0; i < want; i++) {
            int j = i + r.i((int)cand.size() - i);
            std::swap(cand[i], cand[j]);
            int x = cand[i].first, y = cand[i].second;
            auto c = pick(f, r);
            if (!c) continue;
            float ax = x * CELL + 15, ay = y * CELL + 15;
            size_t before = out.size();
            emitCluster(*c, ax, ay, id, false, false, r, 1.f + r.sym(0.1f));
            if (o.mirror && x != cxSum - x) {
                size_t after = out.size();
                for (size_t k = before; k < after; k++) {
                    Obj m = out[k];
                    m.x = axis2 - m.x; m.rot = -m.rot; m.fx = !m.fx;
                    out.push_back(m);
                }
            }
        }
    }

    void computeDepth() {
        std::deque<std::pair<int,int>> q;
        for (auto& c : cells) {
            bool edge = false;
            for (int s = 0; s < 4; s++) if (!has(c.first + DX[s], c.second + DY[s])) edge = true;
            if (edge) { depth[key(c.first, c.second)] = 0; q.push_back(c); }
        }
        while (!q.empty()) {
            auto c = q.front(); q.pop_front();
            int d = depth[key(c.first, c.second)];
            for (int s = 0; s < 4; s++) {
                int x = c.first + DX[s], y = c.second + DY[s];
                if (!has(x, y) || depth.count(key(x, y))) continue;
                depth[key(x, y)] = d + 1;
                q.push_back({ x, y });
            }
        }
    }

    void run() {
        cells = g.cells();
        if (cells.empty()) return;
        computeDepth();
        cxSum = g.minX + g.maxX;
        axis2 = (g.minX + g.maxX + 1) * CELL;
        Rng gr(hash(o.seed, 0xABCDEF));
        size_t ns = std::max<size_t>(t.palette.size(), 1);
        slotMap.resize(ns);
        for (size_t i = 0; i < ns; i++) slotMap[i] = (int)i;
        if (gr.chance(t.slotSwap)) {
            for (auto& sw : t.swaps)
                if (sw.first < (int)ns && sw.second < (int)ns) std::swap(slotMap[sw.first], slotMap[sw.second]);
        }
        hue.resize(ns);
        for (size_t i = 0; i < ns; i++) hue[i] = gr.sym(t.hueVar);

        for (fi = 0; fi < (int)t.features.size(); fi++) {
            auto const& f = t.features[fi];
            Rng fr(hash(o.seed, (uint64_t)fi * 977 + 3));
            if (f.chance < 1.f && !fr.chance(f.chance)) continue;
            float p = effProb(f.prob + fr.sym(f.probJ));
            prio = f.priority;
            switch (f.type) {
                case FType::Fill: if (!o.keepBlocks) fill(f); break;
                case FType::Outline: outline(f); break;
                case FType::Glow: glow(f); break;
                case FType::Extrude: extrude(f); break;
                case FType::Surface: surface(f, p); break;
                case FType::Corner: corner(f, p); break;
                case FType::Interior: interior(f, p); break;
                case FType::Band: band(f); break;
                case FType::Scatter: scatter(f); break;
                case FType::Ledge: ledge(f); break;
                case FType::Panel: panel(f); break;
                case FType::Wire: wire(f, p); break;
            }
        }

        // colours: per-use hue shift + optional gradient (done with HSV -> no extra channels)
        float x0 = g.minX * CELL, x1 = (g.maxX + 1) * CELL;
        for (auto& ob : out) {
            int s = std::clamp(ob.slot, 0, (int)ns - 1);
            ob.h = hue[s];
            if (o.gradient && x1 > x0) {
                float tt = std::clamp((ob.x - x0) / (x1 - x0), 0.f, 1.f);
                ob.v *= 1.f - 0.45f * tt;
                ob.h += 18.f * tt;
            }
            ob.v = std::clamp(ob.v, 0.f, 2.f);
        }

        // object cap: drop the least important pieces first
        if (o.maxObjects > 0 && (int)out.size() > o.maxObjects) {
            std::stable_sort(out.begin(), out.end(), [](Obj const& a, Obj const& b) { return a.prio < b.prio; });
            out.resize(o.maxObjects);
        }
    }
};

} // namespace

std::vector<Obj> generate(Template const& t, Grid const& g, Options const& o) {
    Gen gen(t, g, o);
    gen.run();
    return std::move(gen.out);
}

} // namespace deco
