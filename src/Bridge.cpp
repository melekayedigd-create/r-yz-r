#include "Bridge.hpp"
#include <cmath>
#include <set>
#include <sstream>
#include <iomanip>

using namespace geode::prelude;

namespace bridge {

namespace {

bool isStructure(GameObject* o) {
    return o->m_objectType == GameObjectType::Solid || o->m_objectType == GameObjectType::Slope;
}
bool isHazard(GameObject* o) {
    return o->m_objectType == GameObjectType::Hazard || o->m_objectType == GameObjectType::AnimatedHazard;
}

// mark every 30x30 cell that the object's hitbox covers by at least ~45%
template <class F>
void forCells(CCRect const& r, F&& f) {
    if (r.size.width <= 0 || r.size.height <= 0) return;
    int x0 = (int)std::floor(r.getMinX() / 30.f), x1 = (int)std::floor((r.getMaxX() - 0.01f) / 30.f);
    int y0 = (int)std::floor(r.getMinY() / 30.f), y1 = (int)std::floor((r.getMaxY() - 0.01f) / 30.f);
    for (int x = x0; x <= x1; x++) for (int y = y0; y <= y1; y++) {
        float ox = std::min(r.getMaxX(), x * 30.f + 30.f) - std::max(r.getMinX(), x * 30.f);
        float oy = std::min(r.getMaxY(), y * 30.f + 30.f) - std::max(r.getMinY(), y * 30.f);
        if (ox * oy >= 30.f * 30.f * 0.45f) f(x, y);
    }
}

GJEffectManager* settingsEM(LevelEditorLayer* lel) {
    if (lel->m_levelSettings && lel->m_levelSettings->m_effectManager) return lel->m_levelSettings->m_effectManager;
    return lel->m_effectManager;
}

void usedChannels(LevelEditorLayer* lel, std::set<int>& used) {
    auto objs = lel->m_objects;
    if (!objs) return;
    for (unsigned i = 0; i < objs->count(); i++) {
        auto o = static_cast<GameObject*>(objs->objectAtIndex(i));
        if (!o) continue;
        if (o->m_baseColor) used.insert(o->m_baseColor->m_colorID);
        if (o->m_detailColor) used.insert(o->m_detailColor->m_colorID);
        if (auto eff = typeinfo_cast<EffectGameObject*>(o)) used.insert(eff->m_targetColor);
    }
}

void setChannel(LevelEditorLayer* lel, int ch, deco::Rgb c, bool blend) {
    ccColor3B col = { (GLubyte)c.r, (GLubyte)c.g, (GLubyte)c.b };
    GJEffectManager* ems[2] = { settingsEM(lel), lel->m_effectManager };
    for (int k = 0; k < 2; k++) {
        auto em = ems[k];
        if (!em || (k == 1 && em == ems[0])) continue;
        auto a = em->getColorAction(ch);
        if (!a) continue;
        a->m_fromColor = col;
        a->m_toColor = col;
        a->m_color = col;
        a->m_blending = blend;
        a->m_fromOpacity = 1.f;
        a->m_toOpacity = 1.f;
        a->m_currentOpacity = 1.f;
        a->m_copyID = 0;
        a->m_playerColor = 0;
        if (a->m_colorSprite) em->updateColorAction(a);
    }
}

std::string fmt2(float v) {
    std::ostringstream s;
    s << std::fixed << std::setprecision(2) << v;
    std::string r = s.str();
    while (!r.empty() && r.back() == '0') r.pop_back();
    if (!r.empty() && r.back() == '.') r.pop_back();
    if (r == "-0") r = "0";
    return r;
}

} // namespace

Selection readSelection(LevelEditorLayer* lel, CCArray* selected) {
    Selection s;
    if (!selected) return s;
    for (unsigned i = 0; i < selected->count(); i++) {
        auto o = typeinfo_cast<GameObject*>(selected->objectAtIndex(i));
        if (!o) continue;
        {
            auto p = o->getPosition();
            if (!s.hasBox) { s.bx0 = s.bx1 = p.x; s.by0 = s.by1 = p.y; s.hasBox = true; }
            s.bx0 = std::min(s.bx0, p.x); s.bx1 = std::max(s.bx1, p.x);
            s.by0 = std::min(s.by0, p.y); s.by1 = std::max(s.by1, p.y);
        }
        if (isStructure(o)) {
            s.solids++;
            forCells(o->getObjectRect(), [&](int x, int y) { s.grid.add(x, y); });
        } else if (isHazard(o)) {
            forCells(o->getObjectRect(), [&](int x, int y) { s.grid.addHazard(x, y); });
        } else if (o->m_objectType == GameObjectType::Decoration && !o->m_isTrigger) {
            s.decoObjects.push_back(o);
        } else {
            s.skipped++;
        }
    }
    // a structure made only of tiny pieces may not cover any full cell: fall back to object centres
    if (s.grid.empty() && s.solids > 0) {
        for (unsigned i = 0; i < selected->count(); i++) {
            auto o = typeinfo_cast<GameObject*>(selected->objectAtIndex(i));
            if (o && isStructure(o)) {
                auto p = o->getPosition();
                s.grid.add((int)std::floor(p.x / 30.f), (int)std::floor(p.y / 30.f));
            }
        }
    }
    if (lel) addNearbyHazards(lel, s.grid);
    return s;
}

void addNearbyHazards(LevelEditorLayer* lel, deco::Grid& grid) {
    if (grid.empty() || !lel->m_objects) return;
    float x0 = (grid.minX - 3) * 30.f, x1 = (grid.maxX + 4) * 30.f;
    float y0 = (grid.minY - 3) * 30.f, y1 = (grid.maxY + 4) * 30.f;
    auto objs = lel->m_objects;
    for (unsigned i = 0; i < objs->count(); i++) {
        auto o = static_cast<GameObject*>(objs->objectAtIndex(i));
        if (!o) continue;
        auto p = o->getPosition();
        if (p.x < x0 || p.x > x1 || p.y < y0 || p.y > y1) continue;
        // hazards, orbs, pads and portals: keep decoration away from anything you play with
        auto t = o->m_objectType;
        if (t == GameObjectType::Decoration || t == GameObjectType::Solid || t == GameObjectType::Slope || o->m_isTrigger) continue;
        forCells(o->getObjectRect(), [&](int x, int y) { grid.addHazard(x, y); });
        grid.addHazard((int)std::floor(p.x / 30.f), (int)std::floor(p.y / 30.f));
    }
}

std::vector<int> channelsFor(LevelEditorLayer* lel, deco::Template const& t) {
    std::vector<int> out(t.palette.size(), 1);
    std::set<int> used;
    usedChannels(lel, used);
    auto em = settingsEM(lel);
    int first = (int)Mod::get()->getSettingValue<int64_t>("first-channel");
    first = std::clamp(first, 1, 990);
    std::set<int> taken;
    for (size_t i = 0; i < t.palette.size(); i++) {
        auto const& sl = t.palette[i];
        if (sl.fixed > 0) {
            out[i] = sl.fixed;
            if (sl.force && Mod::get()->getSettingValue<bool>("scene-bg")) setChannel(lel, sl.fixed, sl.c, sl.blend);
            continue;
        }
        int found = 0;
        // 1) a channel that already has exactly this colour (same template used before)
        if (em) {
            for (int ch = 1; ch <= 999 && !found; ch++) {
                if (taken.count(ch) || !em->colorExists(ch)) continue;
                auto a = em->getColorAction(ch);
                if (!a || a->m_copyID != 0 || a->m_playerColor != 0) continue;
                if (a->m_fromColor.r == sl.c.r && a->m_fromColor.g == sl.c.g && a->m_fromColor.b == sl.c.b && a->m_blending == sl.blend)
                    found = ch;
            }
        }
        // 2) a free channel
        for (int ch = first; ch <= 999 && !found; ch++) {
            if (taken.count(ch) || used.count(ch)) continue;
            if (em && em->colorExists(ch)) {
                auto a = em->getColorAction(ch);
                if (a && a->isInUse()) continue;
            }
            found = ch;
        }
        if (!found) found = 1;
        taken.insert(found);
        out[i] = found;
        setChannel(lel, found, sl.c, sl.blend);
    }
    lel->updateLevelColors();
    return out;
}

int freeGroup(LevelEditorLayer* lel) {
    std::set<int> used;
    auto objs = lel->m_objects;
    if (objs) {
        for (unsigned i = 0; i < objs->count(); i++) {
            auto o = static_cast<GameObject*>(objs->objectAtIndex(i));
            if (!o || !o->m_groups) continue;
            for (int k = 0; k < o->m_groupCount && k < 10; k++) used.insert((*o->m_groups)[k]);
            if (auto eff = typeinfo_cast<EffectGameObject*>(o)) used.insert(eff->m_targetGroupID);
        }
    }
    for (int g = 1; g < 9999; g++) if (!used.count(g)) return g;
    return 9999;
}

std::vector<std::string> buildChunks(LevelEditorLayer* lel, std::vector<deco::Obj> const& objs, std::vector<int> const& channels,
                                     bool animate, deco::Grid const& grid, int perChunk) {
    std::vector<std::string> chunks;
    if (objs.empty()) return chunks;
    perChunk = std::max(1, perChunk);
    bool anyAnim = false;
    for (auto& o : objs) if (o.anim) { anyAnim = true; break; }
    int group = (animate && anyAnim) ? freeGroup(lel) : 0;
    int layer = lel->m_currentLayer < 0 ? 0 : lel->m_currentLayer;
    auto ch = [&](int slot) { return (slot >= 0 && slot < (int)channels.size()) ? channels[slot] : 1; };

    std::ostringstream s;
    int inChunk = 0;
    auto flush = [&]() {
        if (inChunk > 0) chunks.push_back(s.str());
        s.str(""); s.clear(); inChunk = 0;
    };
    for (auto& o : objs) {
        if (inChunk > 0) s << ";";
        s << "1," << o.id << ",2," << fmt2(o.x) << ",3," << fmt2(o.y);
        if (std::abs(o.rot) > 0.01f) s << ",6," << fmt2(o.rot);
        if (o.fx) s << ",4,1";
        if (o.fy) s << ",5,1";
        if (std::abs(o.sx - 1.f) > 0.001f) s << ",128," << fmt2(o.sx);
        if (std::abs(o.sy - 1.f) > 0.001f) s << ",129," << fmt2(o.sy);
        s << ",21," << ch(o.slot) << ",22," << ch(o.slot2);
        s << ",24," << o.z << ",25," << o.zo;
        if (layer > 0) s << ",20," << layer;
        if (std::abs(o.h) > 0.5f || std::abs(o.v - 1.f) > 0.01f) {
            std::string hsv = fmt2(o.h) + "a1a" + fmt2(o.v) + "a0a0";
            s << ",41,1,43," << hsv << ",42,1,44," << hsv;
        }
        if (group && o.anim) s << ",57," << group;
        if (++inChunk >= perChunk) flush();
    }
    flush();
    if (group) {
        // one rotate trigger before the structure: the group spins slowly for the whole level
        float tx = std::max(15.f, grid.minX * 30.f - 360.f);
        float ty = (grid.maxY + 3) * 30.f + 15.f;
        std::ostringstream t;
        t << "1,1346,2," << fmt2(tx) << ",3," << fmt2(ty) << ",51," << group << ",68,0,69,200,10,600";
        chunks.push_back(t.str());
    }
    return chunks;
}

int placeChunk(LevelEditorLayer* lel, std::string const& chunk, CCArray* into) {
    gd::string str = chunk;
    auto arr = lel->createObjectsFromString(str, true, true);
    if (!arr) return 0;
    auto added = CCArray::create();
    // safety pass: anything that could touch the player is removed (or made passable)
    for (unsigned i = 0; i < arr->count(); i++) {
        auto o = typeinfo_cast<GameObject*>(arr->objectAtIndex(i));
        if (!o) continue;
        auto t = o->m_objectType;
        bool trigger = o->m_isTrigger || o->m_objectID == 1346;
        if (t == GameObjectType::Decoration || trigger) {
            added->addObject(o);
        } else if (t == GameObjectType::Solid || t == GameObjectType::Slope) {
            o->m_isPassable = true;
            added->addObject(o);
        } else {
            lel->removeObject(o, true);
        }
    }
    if (added->count()) lel->updateObjectColors(added);
    if (into) into->addObjectsFromArray(added);
    return (int)added->count();
}

void removeAll(LevelEditorLayer* lel, CCArray* objects) {
    if (!objects) return;
    if (lel->m_editorUI) lel->m_editorUI->deselectAll();
    for (unsigned i = 0; i < objects->count(); i++) {
        auto o = static_cast<GameObject*>(objects->objectAtIndex(i));
        if (o && o->getParent()) lel->removeObject(o, true);
    }
    objects->removeAllObjects();
}

bool learnFromSelection(LevelEditorLayer* lel, Selection const& sel, std::string const& name, deco::Template& out) {
    if (sel.grid.empty() || sel.decoObjects.empty()) return false;
    auto em = settingsEM(lel);
    std::vector<deco::Slot> palette;
    std::map<int, int> chToSlot;
    auto slotFor = [&](int ch) {
        if (ch <= 0) ch = 1;
        auto it = chToSlot.find(ch);
        if (it != chToSlot.end()) return it->second;
        deco::Slot sl;
        if (ch >= 1000) {
            sl.fixed = ch; // special channels (BG, Obj, 3DL, P1...) stay as they are
        } else if (em && em->colorExists(ch)) {
            if (auto a = em->getColorAction(ch)) {
                sl.c = { a->m_fromColor.r, a->m_fromColor.g, a->m_fromColor.b };
                sl.blend = a->m_blending;
                if (a->m_copyID != 0 || a->m_playerColor != 0) sl.fixed = ch;
            }
        }
        palette.push_back(sl);
        int idx = (int)palette.size() - 1;
        chToSlot[ch] = idx;
        return idx;
    };
    std::vector<deco::SampleObj> samples;
    for (auto o : sel.decoObjects) {
        deco::SampleObj so;
        auto p = o->getPosition();
        so.id = o->m_objectID;
        so.x = p.x; so.y = p.y;
        so.rot = o->getRotation();
        so.sx = o->m_scaleX == 0 ? 1.f : o->m_scaleX;
        so.sy = o->m_scaleY == 0 ? 1.f : o->m_scaleY;
        so.fx = o->isFlipX(); so.fy = o->isFlipY();
        so.slot = slotFor(o->m_baseColor ? o->m_baseColor->m_colorID : 0);
        so.slot2 = o->m_detailColor ? slotFor(o->m_detailColor->m_colorID) : so.slot;
        so.z = (int)(o->m_zLayer == ZLayer::Default ? o->m_defaultZLayer : o->m_zLayer);
        so.zo = o->m_zOrder;
        samples.push_back(so);
    }
    out = deco::learnTemplate(sel.grid, samples, palette, name);
    return !out.features.empty();
}

void cameraRect(LevelEditorLayer* lel, float& x0, float& y0, float& x1, float& y1) {
    auto win = CCDirector::get()->getWinSize();
    auto ol = lel->m_objectLayer;
    float sc = ol ? ol->getScale() : 1.f;
    if (sc <= 0.01f) sc = 1.f;
    CCPoint pos = ol ? ol->getPosition() : CCPoint(0, 0);
    // skip the editor's bottom toolbar
    x0 = (-pos.x) / sc; x1 = (win.width - pos.x) / sc;
    y0 = (win.height * 0.3f - pos.y) / sc; y1 = (win.height - pos.y) / sc;
}

std::string levelKey(LevelEditorLayer* lel) {
    if (!lel || !lel->m_level) return "level";
    std::string n = lel->m_level->m_levelName;
    return n.empty() ? "level" : n;
}

} // namespace bridge
