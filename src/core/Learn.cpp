// "Save as template": learn a template from a structure you decorated by hand.
#include "Deco.hpp"
#include <cmath>
#include <map>
#include <sstream>

namespace deco {

namespace {
int fl(float v) { return (int)std::floor(v / CELL); }
}

Template learnTemplate(Grid const& g, std::vector<SampleObj> const& objs, std::vector<Slot> const& palette, std::string const& name) {
    Template t;
    t.custom = true;
    t.by = "You";
    t.name = name;
    t.id = "custom-" + name;
    t.desc = "Learned from your own decoration";
    t.palette = palette;
    t.hueVar = 0;

    // class: 0..3 surfaces (Top, Right, Bottom, Left), 4 interior, 5..8 corners (TL, TR, BL, BR), 9 scatter
    std::map<std::pair<int, long long>, Cluster> groups;
    auto key = [](int x, int y) { return ((long long)x << 32) ^ (unsigned)y; };

    for (auto const& o : objs) {
        int cx = fl(o.x), cy = fl(o.y);
        Piece p;
        p.id = o.id; p.rot = o.rot; p.sx = o.sx; p.sy = o.sy; p.fx = o.fx; p.fy = o.fy;
        p.slot = o.slot; p.slot2 = o.slot2; p.z = o.z; p.zo = o.zo;
        int cls = -1; long long anchor = 0;
        if (g.has(cx, cy)) {
            cls = 4; anchor = key(cx, cy);
            p.dx = o.x - (cx * CELL + 15); p.dy = o.y - (cy * CELL + 15);
        } else if (g.has(cx, cy - 1)) {          // sits on a top surface
            cls = 0; anchor = key(cx, cy - 1);
            p.dx = o.x - (cx * CELL + 15); p.dy = o.y - cy * CELL;
        } else if (g.has(cx, cy + 1)) {          // hangs under a bottom surface
            cls = 2; anchor = key(cx, cy + 1);
            float ax = cx * CELL + 15, ay = (cy + 1) * CELL;
            p.dx = -(o.x - ax); p.dy = -(o.y - ay);
        } else if (g.has(cx - 1, cy)) {          // right side of a block
            cls = 1; anchor = key(cx - 1, cy);
            float ax = cx * CELL, ay = cy * CELL + 15;
            p.dx = -(o.y - ay); p.dy = o.x - ax;
        } else if (g.has(cx + 1, cy)) {          // left side of a block
            cls = 3; anchor = key(cx + 1, cy);
            float ax = (cx + 1) * CELL, ay = cy * CELL + 15;
            p.dx = o.y - ay; p.dy = -(o.x - ax);
        } else {
            // diagonal neighbours -> corners
            struct D { int ox, oy, cls; bool fx, fy; };
            D ds[4] = { { 1, -1, 5, true, false }, { -1, -1, 6, false, false }, { 1, 1, 7, true, true }, { -1, 1, 8, false, true } };
            for (auto& d : ds) {
                if (!g.has(cx + d.ox, cy + d.oy)) continue;
                cls = d.cls; anchor = key(cx + d.ox, cy + d.oy);
                // corner point of the block cell
                float px = (cx + d.ox) * CELL + (d.fx ? 0 : CELL);
                float py = (cy + d.oy) * CELL + (d.fy ? 0 : CELL);
                p.dx = (o.x - px) * (d.fx ? -1.f : 1.f);
                p.dy = (o.y - py) * (d.fy ? -1.f : 1.f);
                break;
            }
            if (cls < 0) {
                cls = 9; anchor = key(cx, cy);
                p.dx = o.x - (cx * CELL + 15); p.dy = o.y - (cy * CELL + 15);
            }
        }
        groups[{ cls, anchor }].pieces.push_back(p);
    }

    // opportunities in the sample, to turn counts into probabilities
    int opp[10] = {};
    for (auto& c : g.cells()) {
        int x = c.first, y = c.second;
        bool T = !g.has(x, y + 1), R = !g.has(x + 1, y), B = !g.has(x, y - 1), L = !g.has(x - 1, y);
        opp[0] += T; opp[1] += R; opp[2] += B; opp[3] += L; opp[4] += 1;
        opp[5] += T && L; opp[6] += T && R; opp[7] += B && L; opp[8] += B && R;
    }
    std::vector<Cluster> byCls[10];
    for (auto& [k, c] : groups) byCls[k.first].push_back(c);

    for (int cls = 0; cls < 10; cls++) {
        if (byCls[cls].empty()) continue;
        Feature f;
        f.opts = byCls[cls];
        f.orient = false;
        float n = (float)byCls[cls].size();
        if (cls <= 3) {
            f.type = FType::Surface; f.side = (Side)cls; f.priority = 1;
            f.prob = std::min(1.f, n / std::max(1, opp[cls]));
        } else if (cls == 4) {
            f.type = FType::Interior; f.pat = Pattern::Random; f.priority = 2;
            f.prob = std::min(1.f, n / std::max(1, opp[4]));
        } else if (cls <= 8) {
            f.type = FType::Corner; f.convex = true; f.priority = 1;
            f.cornerMask = 1u << (cls - 5);
            f.prob = std::min(1.f, n / std::max(1, opp[cls]));
        } else {
            f.type = FType::Scatter; f.priority = 3; f.margin = 3;
            f.perCell = n / std::max(1, opp[4]);
        }
        t.features.push_back(f);
    }
    return t;
}

// ---------------- save / load ----------------
std::string serialize(Template const& t) {
    std::ostringstream s;
    s << "ADT1\n";
    s << "name " << t.name << "\n";
    for (auto& sl : t.palette) s << "slot " << sl.c.r << " " << sl.c.g << " " << sl.c.b << " " << (sl.blend ? 1 : 0) << " " << sl.fixed << "\n";
    for (auto& f : t.features) {
        s << "feat " << (int)f.type << " " << (int)f.side << " " << f.prob << " " << f.perCell << " " << f.cornerMask << " " << f.priority << "\n";
        for (auto& c : f.opts) {
            s << "clus " << c.weight << "\n";
            for (auto& p : c.pieces)
                s << "p " << p.id << " " << p.dx << " " << p.dy << " " << p.rot << " " << p.sx << " " << p.sy << " "
                  << p.slot << " " << p.slot2 << " " << p.z << " " << p.zo << " " << (int)p.fx << " " << (int)p.fy << "\n";
        }
    }
    s << "end\n";
    return s.str();
}

bool deserialize(std::string const& text, Template& t) {
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || line.rfind("ADT1", 0) != 0) return false;
    t = Template();
    t.custom = true; t.by = "You"; t.hueVar = 0; t.desc = "Learned from your own decoration";
    Feature* f = nullptr;
    Cluster* c = nullptr;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string tag;
        ls >> tag;
        if (tag == "name") { std::getline(ls, t.name); if (!t.name.empty() && t.name[0] == ' ') t.name.erase(0, 1); t.id = "custom-" + t.name; }
        else if (tag == "slot") { Slot sl; int b = 0; ls >> sl.c.r >> sl.c.g >> sl.c.b >> b; sl.blend = b; if (!(ls >> sl.fixed)) sl.fixed = 0; t.palette.push_back(sl); }
        else if (tag == "feat") {
            Feature nf; int ty = 0, sd = 0;
            ls >> ty >> sd >> nf.prob >> nf.perCell >> nf.cornerMask >> nf.priority;
            if (ty < 0 || ty > (int)FType::Wire) return false;
            nf.type = (FType)ty; nf.side = (Side)(sd & 3); nf.orient = false; nf.convex = true; nf.margin = 3;
            t.features.push_back(nf);
            f = &t.features.back(); c = nullptr;
        }
        else if (tag == "clus" && f) { Cluster nc; ls >> nc.weight; f->opts.push_back(nc); c = &f->opts.back(); }
        else if (tag == "p" && c) {
            Piece p; int fx = 0, fy = 0;
            ls >> p.id >> p.dx >> p.dy >> p.rot >> p.sx >> p.sy >> p.slot >> p.slot2 >> p.z >> p.zo >> fx >> fy;
            p.fx = fx; p.fy = fy;
            c->pieces.push_back(p);
        }
        else if (tag == "end") break;
    }
    return !t.name.empty() && !t.features.empty();
}

} // namespace deco
