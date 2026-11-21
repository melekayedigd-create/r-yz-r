#pragma once
#include <Geode/Geode.hpp>
#include "core/Deco.hpp"

// Everything that talks to Geometry Dash's editor lives here.
namespace bridge {

struct Selection {
    deco::Grid grid;
    std::vector<GameObject*> decoObjects; // non-solid objects in the selection (used by "Save as template")
    int solids = 0;
    int skipped = 0;                      // triggers, portals, orbs... (never touched)
    bool hasBox = false;                  // bounding box of everything selected (for scenes)
    float bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;
};

// the part of the level visible on screen (used for scenes when nothing is selected)
void cameraRect(LevelEditorLayer* lel, float& x0, float& y0, float& x1, float& y1);

Selection readSelection(LevelEditorLayer* lel, cocos2d::CCArray* selected);
void addNearbyHazards(LevelEditorLayer* lel, deco::Grid& grid);

// colour channels: reuse a channel that already has the exact colour, otherwise take a free one
std::vector<int> channelsFor(LevelEditorLayer* lel, deco::Template const& t);

// a group ID that no object uses yet
int freeGroup(LevelEditorLayer* lel);

// objects are turned into level-string pieces, then placed a few per frame (lighter for tablets)
std::vector<std::string> buildChunks(LevelEditorLayer* lel, std::vector<deco::Obj> const& objs, std::vector<int> const& channels,
                                     bool animate, deco::Grid const& grid, int perChunk);
// places one piece, adds the created objects to `into`, returns how many were kept
int placeChunk(LevelEditorLayer* lel, std::string const& chunk, cocos2d::CCArray* into);
void removeAll(LevelEditorLayer* lel, cocos2d::CCArray* objects);

// "Save as template"
bool learnFromSelection(LevelEditorLayer* lel, Selection const& sel, std::string const& name, deco::Template& out);

std::string levelKey(LevelEditorLayer* lel);

} // namespace bridge
