#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>
#include "Bridge.hpp"

// ---------- shared state ----------
struct DecoSettings {
    int density = 1;        // 0 light, 1 medium, 2 heavy
    bool gradient = false;
    bool mirror = false;
    bool animate = true;
    bool keepBlocks = false;
    static DecoSettings load();
    void save() const;
    float densityValue() const { return density == 0 ? 0.55f : density == 2 ? 1.8f : 1.f; }
};

struct AppliedRecord { std::string templateId; uint64_t seed; int minX, maxX, minY, maxY; };

class TemplateStore {
public:
    static TemplateStore& get();
    std::vector<deco::Template> const& all();      // built-in + custom
    deco::Template const* find(std::string const& id);
    void reloadCustom();
    bool saveCustom(deco::Template const& t);
    void deleteCustom(std::string const& id);
    bool isFavorite(std::string const& id);
    void setFavorite(std::string const& id, bool fav);
    // "match nearby"
    void remember(std::string const& level, AppliedRecord const& r);
    AppliedRecord const* nearest(std::string const& level, deco::Grid const& g);
private:
    std::vector<deco::Template> m_all;
    bool m_loaded = false;
    std::map<std::string, std::vector<AppliedRecord>> m_applied;
    std::filesystem::path dir();
};

// ---------- floating preview panel (Shuffle / Apply / Cancel) ----------
class PreviewPanel : public cocos2d::CCNode {
public:
    static PreviewPanel* create(EditorUI* ui, deco::Template const& t, bridge::Selection sel, uint64_t seed);
    void setArea(float x0, float y0, float x1, float y1) { m_ax0 = x0; m_ay0 = y0; m_ax1 = x1; m_ay1 = y1; }
    void regenerate();
    void cancel();
    static PreviewPanel* current(EditorUI* ui);
protected:
    bool init(EditorUI* ui, deco::Template const& t, bridge::Selection sel, uint64_t seed);
    void onShuffle(cocos2d::CCObject*);
    void onApply(cocos2d::CCObject*);
    void onCancel(cocos2d::CCObject*);
    void finishApply();
    void tick(float);
    std::vector<std::string> m_chunks;
    int m_next = 0;
    int m_total = 0;
    bool m_applyWhenDone = false;
    void finishCancel();
    void updateLabel();
    bool m_busy = false;
    EditorUI* m_ui = nullptr;
    deco::Template m_template;
    bridge::Selection m_sel;
    uint64_t m_seed = 1;
    int m_variation = 1;
    float m_ax0 = 0, m_ay0 = 0, m_ax1 = 0, m_ay1 = 0;   // scene area
    geode::Ref<cocos2d::CCArray> m_objects;
    cocos2d::CCLabelBMFont* m_label = nullptr;
    cocos2d::CCLabelBMFont* m_info = nullptr;
};

// ---------- template picker ----------
class PickerPopup : public geode::Popup {
public:
    static PickerPopup* create(EditorUI* ui, bridge::Selection sel);
protected:
    bool init(EditorUI* ui, bridge::Selection sel);
    void buildList();
    void buildOptions();
    void onUse(cocos2d::CCObject*);
    void onFav(cocos2d::CCObject*);
    void onDelete(cocos2d::CCObject*);
    void onDensity(cocos2d::CCObject*);
    void onToggle(cocos2d::CCObject*);
    void onMatch(cocos2d::CCObject*);
    void onSaveTemplate(cocos2d::CCObject*);
    void onRandom(cocos2d::CCObject*);
    void start(deco::Template const& t, uint64_t seed);
    EditorUI* m_ui = nullptr;
    bridge::Selection m_sel;
    DecoSettings m_set;
    geode::ScrollLayer* m_scroll = nullptr;
    CCMenuItemSpriteExtra* m_densityBtn = nullptr;
    std::vector<std::string> m_ids;   // index -> template id for button tags
};

// ---------- name prompt for "Save as template" ----------
class NamePopup : public geode::Popup {
public:
    static NamePopup* create(std::function<void(std::string)> done);
protected:
    bool init(std::function<void(std::string)> done);
    void onOk(cocos2d::CCObject*);
    geode::TextInput* m_input = nullptr;
    std::function<void(std::string)> m_done;
};

void openDecorator(EditorUI* ui);
void refreshUndoButton(EditorUI* ui);
void undoLastDecoration(EditorUI* ui);
void forgetLastDecoration();
