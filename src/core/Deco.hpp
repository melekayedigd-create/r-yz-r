// Auto Decorator - core engine (no Geode / GD dependency, so it can be tested on any PC)
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>

namespace deco {

// GD Z layers (same numbers as the level string key 24)
constexpr int ZB4 = -3, ZB3 = -1, ZB2 = 1, ZB1 = 3, ZT1 = 5, ZT2 = 7, ZT3 = 9;
constexpr float CELL = 30.f;

struct Rgb { int r = 255, g = 255, b = 255; };
struct Slot { Rgb c; bool blend = false; std::string name; int fixed = 0; /* >0: always use this GD colour channel */ bool force = false; /* also set that channel's colour */ };

// One object of a decoration "cluster", positioned relative to an anchor.
struct Piece {
    int id = 916;
    float dx = 0, dy = 0;     // offset from the anchor (units). For sides: dx = along the edge, dy = outward
    float rot = 0;            // clockwise degrees (GD convention)
    float sx = 1, sy = 1;     // scale
    int slot = 0;             // palette slot for the main colour
    int slot2 = -1;           // palette slot for the detail colour (-1 = same as slot)
    int z = ZT1, zo = 0;      // z layer / z order
    bool fx = false, fy = false;
    float jPos = 0, jRot = 0, jScale = 0; // random jitter ranges
    bool randFlip = false;    // random horizontal flip
    bool anim = false;        // gets the animation group (rotate trigger)
    float v = 1.f;            // brightness multiplier (HSV value)
    float stretch = 0.f;      // random extra length outward (hanging roots, vines, pillars)
    float hBase = 15.f;       // the object's own height in units (for stretch)
};

struct Cluster {
    std::vector<Piece> pieces;
    float weight = 1.f;
};

enum class FType { Fill, Outline, Glow, Extrude, Surface, Corner, Interior, Band, Scatter, Ledge, Panel, Wire };
enum class Side { Top = 0, Right = 1, Bottom = 2, Left = 3 };
enum class Pattern { Random, Windows, Checker, Rows, Columns, Every };

struct Feature {
    FType type = FType::Surface;
    float chance = 1.f;       // chance that this feature is used at all (per use) -> variation
    float prob = 1.f;         // chance per opportunity (cell / corner / edge)
    float probJ = 0.f;        // random +/- on prob per use
    int priority = 0;         // higher = removed first when the object cap is hit
    std::vector<Cluster> opts;

    // shared simple settings
    int slot = 0, slot2 = -1;
    int z = ZT1, zo = 0;
    float v = 1.f;

    // Fill / Outline / Glow / Band
    int lineId = 916;         // 916 = plain white square (15x15)
    float thick = 2.f;        // line thickness (units)
    float inset = 0.f;        // inward offset (negative = outside)
    float glowSize = 1.f;     // glow height scale
    bool inward = false;      // glow fades into the block instead of out of it
    bool corners = true;      // add corner glow pieces
    unsigned sides = 0xF;     // bit per Side to include (outline/glow/surface)

    // Extrude (3D look toward top-right)
    float ex = 6.f, ey = 6.f;
    int slotTop = 0, slotSide = 1, lineSlot = -1;
    float lineThick = 1.5f;

    // Surface (Top/Bottom/Left/Right)
    Side side = Side::Top;
    int spacing = 1;          // every Nth cell along a run
    bool runEnds = false;     // only at the ends of runs
    bool orient = true;       // rotate pieces with the side (bottom = upside down)
    bool skipHazard = true;

    // Corner
    bool convex = true, concave = false;
    unsigned cornerMask = 0xF; // bit0 TL, bit1 TR, bit2 BL, bit3 BR

    // Interior
    Pattern pat = Pattern::Random;
    int minDepth = 0;
    int px = 2, py = 2;       // pattern period
    bool randomPhase = true;

    // Band
    float dist = 10.f;        // distance below the top surface (units)

    // Scatter
    float perCell = 0.1f;
    int margin = 2;
    bool hugEdge = false;     // place right on/around the structure's edge (big gears behind it...)

    // Ledge (slab on top runs that sticks out past the ends)
    float over = 6.f;
    int slotEdge = -1, slotUnder = -1;

    // Outline: broken (dashed) lines. 0 = solid
    float dash = 0.f;

    // Wire (circuit wires leaving the structure, square nodes at bends and ends)
    float wireMin = 20.f, wireMax = 60.f;
    float node = 6.f;          // node square size
    int nodeSlot = -1;
};

struct Template {
    std::string id, name, by, desc;
    std::vector<Slot> palette;
    std::vector<Feature> features;
    float hueVar = 8.f;       // per use hue variation (degrees, done with HSV so no new channels)
    float slotSwap = 0.f;     // chance to swap the slot pairs below for a use (colour variant)
    std::vector<std::pair<int,int>> swaps;
    bool custom = false;
    int scene = 0;            // >0: a "scene" (backdrop / cutscene look) instead of decorating blocks
};

struct Options {
    uint64_t seed = 1;
    float density = 1.f;      // 0.55 light, 1 medium, 1.6 heavy
    bool gradient = false;
    bool mirror = false;
    bool animate = true;
    bool keepBlocks = false;  // skip Fill features (keep the user's own block look)
    int maxObjects = 1500;
};

struct Obj {
    int id = 916;
    float x = 0, y = 0, rot = 0, sx = 1, sy = 1;
    bool fx = false, fy = false;
    int slot = 0, slot2 = 0;
    int z = ZT1, zo = 0;
    bool anim = false;
    float h = 0, s = 1, v = 1;
    int prio = 0;
};

class Grid {
public:
    void add(int x, int y);
    void addHazard(int x, int y);
    bool has(int x, int y) const;
    bool hazard(int x, int y) const;
    bool empty() const { return m_cells.empty(); }
    int size() const { return (int)m_cells.size(); }
    int minX = 0, maxX = -1, minY = 0, maxY = -1;
    std::vector<std::pair<int,int>> cells() const;
private:
    static long long key(int x, int y) { return ((long long)x << 32) ^ (unsigned)y; }
    std::unordered_set<long long> m_cells, m_hazards;
};

std::vector<Obj> generate(Template const& t, Grid const& g, Options const& o);

// Built-in templates (20)
std::vector<Template> const& builtinTemplates();

// Scenes: fill a whole area with a background look (sky, skyline, glow, particles...)
std::vector<Obj> generateScene(Template const& t, float x0, float y0, float x1, float y1, Options const& o);
std::vector<Template> const& builtinScenes();

// ---- learning a template from a hand decorated sample ----
struct SampleObj {
    int id; float x, y, rot, sx, sy; bool fx, fy;
    int slot, slot2; int z, zo;
};
// palette must already be filled by the caller (one slot per distinct colour channel found)
Template learnTemplate(Grid const& g, std::vector<SampleObj> const& objs, std::vector<Slot> const& palette, std::string const& name);
std::string serialize(Template const& t);
bool deserialize(std::string const& text, Template& out);

// tiny deterministic RNG
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ull) {}
    uint64_t next() {
        uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    float f() { return (next() >> 40) / float(1ull << 24); }      // [0,1)
    float range(float a, float b) { return a + (b - a) * f(); }
    float sym(float r) { return r == 0 ? 0 : range(-r, r); }
    int i(int n) { return n <= 0 ? 0 : int(next() % (uint64_t)n); }
    bool chance(float p) { return f() < p; }
};
uint64_t hash(uint64_t a, uint64_t b);

} // namespace deco
