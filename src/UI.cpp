#include "UI.hpp"
#include <chrono>
#include <fstream>
#include <sstream>

using namespace geode::prelude;

// =====================================================================
// settings
// =====================================================================
DecoSettings DecoSettings::load() {
    DecoSettings s;
    auto m = Mod::get();
    s.density = (int)m->getSavedValue<int64_t>("density", 1);
    s.gradient = m->getSavedValue<bool>("gradient", false);
    s.mirror = m->getSavedValue<bool>("mirror", false);
    s.animate = m->getSavedValue<bool>("animate", true);
    s.keepBlocks = m->getSavedValue<bool>("keep-blocks", false);
    s.density = std::clamp(s.density, 0, 2);
    return s;
}
void DecoSettings::save() const {
    auto m = Mod::get();
    m->setSavedValue<int64_t>("density", density);
    m->setSavedValue<bool>("gradient", gradient);
    m->setSavedValue<bool>("mirror", mirror);
    m->setSavedValue<bool>("animate", animate);
    m->setSavedValue<bool>("keep-blocks", keepBlocks);
}

static uint64_t freshSeed() {
    static uint64_t counter = 0;
    auto t = (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
    return deco::hash(t, ++counter);
}

// =====================================================================
// template store
// =====================================================================
TemplateStore& TemplateStore::get() { static TemplateStore s; return s; }

std::filesystem::path TemplateStore::dir() { return Mod::get()->getSaveDir() / "templates"; }

void TemplateStore::reloadCustom() {
    m_all = deco::builtinTemplates();
    for (auto& sc : deco::builtinScenes()) m_all.push_back(sc);
    std::error_code ec;
    auto d = dir();
    std::filesystem::create_directories(d, ec);
    std::vector<deco::Template> customs;
    for (auto it = std::filesystem::directory_iterator(d, ec); !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
        if (it->path().extension() != ".adt") continue;
        std::ifstream in(it->path(), std::ios::binary);
        std::stringstream ss; ss << in.rdbuf();
        deco::Template t;
        if (deco::deserialize(ss.str(), t)) customs.push_back(t);
    }
    std::sort(customs.begin(), customs.end(), [](auto& a, auto& b) { return a.name < b.name; });
    for (auto& t : customs) m_all.push_back(t);
    m_loaded = true;
}

std::vector<deco::Template> const& TemplateStore::all() {
    if (!m_loaded) reloadCustom();
    return m_all;
}

deco::Template const* TemplateStore::find(std::string const& id) {
    for (auto& t : all()) if (t.id == id) return &t;
    return nullptr;
}

static std::string safeFileName(std::string const& n) {
    std::string r;
    for (char c : n) r += (std::isalnum((unsigned char)c) || c == '-' || c == '_') ? c : '_';
    if (r.empty()) r = "template";
    return r;
}

bool TemplateStore::saveCustom(deco::Template const& t) {
    std::error_code ec;
    std::filesystem::create_directories(dir(), ec);
    std::ofstream out(dir() / (safeFileName(t.name) + ".adt"), std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << deco::serialize(t);
    out.close();
    reloadCustom();
    return true;
}

void TemplateStore::deleteCustom(std::string const& id) {
    auto t = find(id);
    if (!t || !t->custom) return;
    std::error_code ec;
    std::filesystem::remove(dir() / (safeFileName(t->name) + ".adt"), ec);
    reloadCustom();
}

bool TemplateStore::isFavorite(std::string const& id) {
    auto favs = Mod::get()->getSavedValue<std::string>("favorites", "");
    return favs.find(";" + id + ";") != std::string::npos;
}

void TemplateStore::setFavorite(std::string const& id, bool fav) {
    auto favs = Mod::get()->getSavedValue<std::string>("favorites", "");
    auto key = ";" + id + ";";
    auto pos = favs.find(key);
    if (fav && pos == std::string::npos) favs += key;
    if (!fav && pos != std::string::npos) favs.erase(pos, key.size());
    Mod::get()->setSavedValue<std::string>("favorites", favs);
}

void TemplateStore::remember(std::string const& level, AppliedRecord const& r) {
    auto& v = m_applied[level];
    v.push_back(r);
    if (v.size() > 40) v.erase(v.begin());
    std::ostringstream s;
    for (auto& a : v) s << a.templateId << "," << a.seed << "," << a.minX << "," << a.maxX << "," << a.minY << "," << a.maxY << "|";
    Mod::get()->setSavedValue<std::string>("applied:" + level, s.str());
}

AppliedRecord const* TemplateStore::nearest(std::string const& level, deco::Grid const& g) {
    auto& v = m_applied[level];
    if (v.empty()) {
        auto saved = Mod::get()->getSavedValue<std::string>("applied:" + level, "");
        std::stringstream ss(saved);
        std::string item;
        while (std::getline(ss, item, '|')) {
            std::stringstream is(item);
            std::string f[6];
            int n = 0;
            while (n < 6 && std::getline(is, f[n], ',')) n++;
            if (n < 6) continue;
            AppliedRecord r;
            r.templateId = f[0];
            r.seed = std::strtoull(f[1].c_str(), nullptr, 10);
            r.minX = std::atoi(f[2].c_str()); r.maxX = std::atoi(f[3].c_str());
            r.minY = std::atoi(f[4].c_str()); r.maxY = std::atoi(f[5].c_str());
            v.push_back(r);
        }
    }
    if (v.empty() || g.empty()) return nullptr;
    float cx = (g.minX + g.maxX) / 2.f, cy = (g.minY + g.maxY) / 2.f;
    AppliedRecord const* best = nullptr;
    float bestD = 1e30f;
    for (auto& r : v) {
        // distance between boxes (0 if they overlap)
        float dx = std::max({ 0.f, (float)r.minX - g.maxX, (float)g.minX - r.maxX });
        float dy = std::max({ 0.f, (float)r.minY - g.maxY, (float)g.minY - r.maxY });
        float d = dx * dx + dy * dy + 0.001f * ((cx - (r.minX + r.maxX) / 2.f) * (cx - (r.minX + r.maxX) / 2.f) + (cy - (r.minY + r.maxY) / 2.f) * (cy - (r.minY + r.maxY) / 2.f));
        if (d < bestD) { bestD = d; best = &r; }
    }
    return best;
}

// =====================================================================
// preview panel
// =====================================================================
static char const* const PANEL_ID = GEODE_MOD_ID "/preview-panel";

PreviewPanel* PreviewPanel::current(EditorUI* ui) {
    return ui ? static_cast<PreviewPanel*>(ui->getChildByID(PANEL_ID)) : nullptr;
}

PreviewPanel* PreviewPanel::create(EditorUI* ui, deco::Template const& t, bridge::Selection sel, uint64_t seed) {
    auto r = new PreviewPanel();
    if (r->init(ui, t, std::move(sel), seed)) { r->autorelease(); return r; }
    delete r;
    return nullptr;
}

static CCMenuItemSpriteExtra* textButton(char const* text, CCObject* target, SEL_MenuHandler sel, char const* bg = "GJ_button_01.png", float scale = 0.6f) {
    auto spr = ButtonSprite::create(text, "bigFont.fnt", bg, 0.8f);
    spr->setScale(scale);
    return CCMenuItemSpriteExtra::create(spr, target, sel);
}

bool PreviewPanel::init(EditorUI* ui, deco::Template const& t, bridge::Selection sel, uint64_t seed) {
    if (!CCNode::init()) return false;
    m_ui = ui; m_template = t; m_sel = std::move(sel); m_seed = seed;
    this->setID(PANEL_ID);
    auto win = CCDirector::get()->getWinSize();
    float w = 280.f, h = 66.f;
    this->setContentSize({ w, h });
    this->setAnchorPoint({ 0.5f, 0.5f });
    this->ignoreAnchorPointForPosition(false);
    this->setPosition({ win.width / 2.f, win.height - 78.f });

    auto bg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
    bg->setContentSize({ w, h });
    bg->setColor({ 0, 0, 0 });
    bg->setOpacity(170);
    bg->setPosition({ w / 2, h / 2 });
    this->addChild(bg);

    m_label = CCLabelBMFont::create("", "goldFont.fnt");
    m_label->setScale(0.5f);
    m_label->setPosition({ w / 2, h - 12 });
    this->addChild(m_label);
    m_info = CCLabelBMFont::create("", "chatFont.fnt");
    m_info->setScale(0.55f);
    m_info->setPosition({ w / 2, h - 27 });
    this->addChild(m_info);

    auto menu = CCMenu::create();
    menu->setPosition({ 0, 0 });
    auto shuffle = textButton("Shuffle", this, menu_selector(PreviewPanel::onShuffle), "GJ_button_02.png");
    shuffle->setPosition({ w / 2 - 88, 17 });
    auto apply = textButton("Apply", this, menu_selector(PreviewPanel::onApply), "GJ_button_01.png");
    apply->setPosition({ w / 2, 17 });
    auto cancel = textButton("Cancel", this, menu_selector(PreviewPanel::onCancel), "GJ_button_06.png");
    cancel->setPosition({ w / 2 + 88, 17 });
    menu->addChild(shuffle);
    menu->addChild(apply);
    menu->addChild(cancel);
    this->addChild(menu);
    menu->setTouchPriority(-510);

    m_objects = CCArray::create();
    return true;
}

void PreviewPanel::updateLabel() {
    m_label->setString(fmt::format("{}  -  variation #{}", m_template.name, m_variation).c_str());
    m_label->limitLabelWidth(260.f, 0.5f, 0.2f);
    int placed = m_objects ? (int)m_objects->count() : 0;
    if (!m_chunks.empty() && m_next < (int)m_chunks.size())
        m_info->setString(fmt::format("Placing... {} / {}", placed, m_total).c_str());
    else
        m_info->setString(fmt::format("{} objects  -  Shuffle for a new look, Apply to keep", placed).c_str());
    m_info->limitLabelWidth(260.f, 0.55f, 0.2f);
}

void PreviewPanel::regenerate() {
    auto lel = m_ui->m_editorLayer;
    this->unschedule(schedule_selector(PreviewPanel::tick));
    if (m_objects) bridge::removeAll(lel, m_objects);
    m_objects = CCArray::create();
    auto set = DecoSettings::load();
    deco::Options o;
    o.seed = m_seed;
    o.density = set.densityValue();
    o.gradient = set.gradient;
    o.mirror = set.mirror;
    o.animate = set.animate;
    o.keepBlocks = set.keepBlocks;
    // Heavy has no object cap: it uses as many objects as the template wants
    o.maxObjects = set.density == 2 ? 0 : (int)Mod::get()->getSettingValue<int64_t>("max-objects");
    auto objs = m_template.scene ? deco::generateScene(m_template, m_ax0, m_ay0, m_ax1, m_ay1, o)
                                 : deco::generate(m_template, m_sel.grid, o);
    auto channels = bridge::channelsFor(lel, m_template);
    int perFrame = (int)Mod::get()->getSettingValue<int64_t>("objects-per-frame");
    m_chunks = bridge::buildChunks(lel, objs, channels, set.animate, m_sel.grid, perFrame);
    m_next = 0;
    m_total = (int)objs.size();
    if (m_chunks.empty()) {
        updateLabel();
        Notification::create("Nothing to place here - try another template", NotificationIcon::Warning)->show();
        return;
    }
    updateLabel();
    // place the objects a few at a time, one piece per frame
    this->schedule(schedule_selector(PreviewPanel::tick));
}

void PreviewPanel::tick(float) {
    if (m_next >= (int)m_chunks.size()) {
        this->unschedule(schedule_selector(PreviewPanel::tick));
        m_chunks.clear();
        updateLabel();
        if (m_applyWhenDone) {
            m_applyWhenDone = false;
            Ref<PreviewPanel> self = this;
            Loader::get()->queueInMainThread([self]() { self->finishApply(); });
        }
        return;
    }
    bridge::placeChunk(m_ui->m_editorLayer, m_chunks[m_next], m_objects);
    m_next++;
    updateLabel();
}

void PreviewPanel::onShuffle(CCObject*) {
    if (m_busy) return;
    m_variation++;
    m_seed = deco::hash(m_seed, (uint64_t)m_variation * 2654435761ull);
    regenerate();
}

// Apply / Cancel remove this panel. Doing that inside the button's own callback can
// free the button while the game is still using it (a crash on some tablets), so the
// work is done on the next frame instead.
void PreviewPanel::onApply(CCObject*) {
    if (m_busy) return;
    m_busy = true;
    this->setVisible(false);
    if (!m_chunks.empty() && m_next < (int)m_chunks.size()) { m_applyWhenDone = true; return; }
    Ref<PreviewPanel> self = this;
    Loader::get()->queueInMainThread([self]() { self->finishApply(); });
}

// the last applied decoration, removable with the "Undo Deco" button
static Ref<CCArray> s_lastApplied;
static LevelEditorLayer* s_lastLayer = nullptr;

void refreshUndoButton(EditorUI* ui) {
    if (!ui) return;
    if (auto b = ui->getChildByIDRecursive(GEODE_MOD_ID "/deco-undo"))
        b->setVisible(s_lastApplied && s_lastApplied->count() > 0 && s_lastLayer == ui->m_editorLayer);
}

void undoLastDecoration(EditorUI* ui) {
    if (!ui || !s_lastApplied || s_lastLayer != ui->m_editorLayer) return;
    int n = (int)s_lastApplied->count();
    bridge::removeAll(ui->m_editorLayer, s_lastApplied);
    s_lastApplied = nullptr;
    s_lastLayer = nullptr;
    refreshUndoButton(ui);
    Notification::create(fmt::format("Removed the last decoration ({} objects)", n), NotificationIcon::Info)->show();
}

void forgetLastDecoration() {
    s_lastApplied = nullptr;
    s_lastLayer = nullptr;
}

void PreviewPanel::finishApply() {
    log::info("apply: start");
    auto lel = m_ui ? m_ui->m_editorLayer : nullptr;
    int n = m_objects ? (int)m_objects->count() : 0;
    this->unschedule(schedule_selector(PreviewPanel::tick));
    if (lel && n > 0) {
        // NOTE: CCArray::createWithArray() copies every element, and game objects
        // can't be copied (that was the Apply crash). Just add the same objects.
        auto kept = CCArray::create();
        kept->addObjectsFromArray(m_objects);
        s_lastApplied = kept;
        s_lastLayer = lel;
        log::info("apply: kept {} objects for Undo Deco", n);
        auto& g = m_sel.grid;
        TemplateStore::get().remember("level", { m_template.id, m_seed, g.minX, g.maxX, g.minY, g.maxY });
        log::info("apply: remembered");
    }
    m_objects = nullptr;
    auto ui = m_ui;
    this->removeFromParent();
    refreshUndoButton(ui);
    if (n > 0) Notification::create(fmt::format("Decorated with {} objects - tap Undo Deco to remove", n), NotificationIcon::Success)->show();
    log::info("apply: done");
}

void PreviewPanel::cancel() {
    // used when another preview replaces this one (not from our own button)
    this->unschedule(schedule_selector(PreviewPanel::tick));
    m_chunks.clear();
    if (m_objects && m_ui) bridge::removeAll(m_ui->m_editorLayer, m_objects);
    m_objects = nullptr;
    this->removeFromParent();
}

void PreviewPanel::onCancel(CCObject*) {
    if (m_busy) return;
    m_busy = true;
    this->setVisible(false);
    Ref<PreviewPanel> self = this;
    Loader::get()->queueInMainThread([self]() { self->finishCancel(); });
}

void PreviewPanel::finishCancel() {
    this->unschedule(schedule_selector(PreviewPanel::tick));
    m_chunks.clear();
    if (m_objects && m_ui) bridge::removeAll(m_ui->m_editorLayer, m_objects);
    m_objects = nullptr;
    this->removeFromParent();
}

// =====================================================================
// picker popup
// =====================================================================
PickerPopup* PickerPopup::create(EditorUI* ui, bridge::Selection sel) {
    auto r = new PickerPopup();
    if (r->init(ui, std::move(sel))) { r->autorelease(); return r; }
    delete r;
    return nullptr;
}

static ccColor3B c3(deco::Rgb c) { return { (GLubyte)c.r, (GLubyte)c.g, (GLubyte)c.b }; }

static CCNode* makeThumb(deco::Template const& t) {
    auto node = CCNode::create();
    node->setContentSize({ 30, 30 });
    node->setAnchorPoint({ 0.5f, 0.5f });
    if (t.palette.empty()) return node;
    int fillS = 0, lineS = t.palette.size() > 1 ? 1 : 0, glowS = -1, accent = -1;
    bool hasFill = false, hasLine = false;
    for (auto& f : t.features) {
        if (f.type == deco::FType::Fill && !hasFill) { fillS = f.slot; hasFill = true; }
        if (f.type == deco::FType::Outline && !hasLine) { lineS = f.slot; hasLine = true; }
        if (f.type == deco::FType::Glow && !f.inward && glowS < 0) glowS = f.slot;
    }
    for (int i = (int)t.palette.size() - 1; i >= 0; i--)
        if (i != fillS && i != lineS && i != glowS && !t.palette[i].blend) { accent = i; break; }
    auto clampS = [&](int s) { return std::clamp(s, 0, (int)t.palette.size() - 1); };
    auto add = [&](int slot, float size, GLubyte op) {
        auto l = CCLayerColor::create({ 255, 255, 255, op }, size, size);
        l->setColor(c3(t.palette[clampS(slot)].c));
        l->setOpacity(op);
        l->ignoreAnchorPointForPosition(false);
        l->setAnchorPoint({ 0.5f, 0.5f });
        l->setPosition({ 15, 15 });
        node->addChild(l);
        return l;
    };
    if (glowS >= 0) add(glowS, 34, 90);
    add(lineS, 26, 255);
    add(fillS, 20, 255);
    if (accent >= 0) {
        auto a = add(accent, 7, 255);
        a->setPosition({ 15, 15 });
    }
    return node;
}

bool PickerPopup::init(EditorUI* ui, bridge::Selection sel) {
    if (!Popup::init(440.f, 280.f)) return false;
    m_ui = ui;
    m_sel = std::move(sel);
    m_set = DecoSettings::load();
    this->setTitle("Auto Decorator");
    buildList();
    buildOptions();
    return true;
}

void PickerPopup::buildList() {
    if (m_scroll) { m_scroll->removeFromParent(); m_scroll = nullptr; }
    auto& store = TemplateStore::get();
    auto const& all = store.all();
    // favourites first
    std::vector<deco::Template const*> order;
    for (auto& t : all) if (store.isFavorite(t.id)) order.push_back(&t);
    for (auto& t : all) if (!store.isFavorite(t.id)) order.push_back(&t);

    float listW = 280.f, listH = 214.f, rowH = 36.f;
    m_scroll = ScrollLayer::create({ listW, listH });
    m_scroll->setPosition({ 12.f, 22.f });
    m_mainLayer->addChild(m_scroll);

    auto bg = CCLayerColor::create({ 0, 0, 0, 80 }, listW, listH);
    bg->setPosition({ 12.f, 22.f });
    m_mainLayer->addChild(bg, -1);

    float contentH = std::max(listH, rowH * order.size());
    auto content = m_scroll->m_contentLayer;
    content->setContentSize({ listW, contentH });
    auto menu = CCMenu::create();
    menu->setPosition({ 0, 0 });
    menu->setContentSize({ listW, contentH });
    content->addChild(menu, 2);

    m_ids.clear();
    for (size_t i = 0; i < order.size(); i++) {
        auto const& t = *order[i];
        float y = contentH - rowH * i - rowH / 2;
        int tag = (int)m_ids.size();
        m_ids.push_back(t.id);

        if (i % 2 == 0) {
            auto stripe = CCLayerColor::create({ 255, 255, 255, 12 }, listW, rowH);
            stripe->setPosition({ 0, y - rowH / 2 });
            content->addChild(stripe, 0);
        }
        auto thumb = makeThumb(t);
        thumb->setPosition({ 22, y });
        content->addChild(thumb, 1);

        auto name = CCLabelBMFont::create(t.name.c_str(), "bigFont.fnt");
        name->setAnchorPoint({ 0, 0.5f });
        name->setScale(0.4f);
        name->limitLabelWidth(140.f, 0.4f, 0.15f);
        name->setPosition({ 44, y + 6 });
        content->addChild(name, 1);

        auto by = CCLabelBMFont::create(fmt::format("{}{}", t.custom ? "Your template" : (t.scene ? std::string("Scene - no blocks needed") : "by " + t.by), t.desc.empty() ? "" : "  -  " + t.desc).c_str(), "chatFont.fnt");
        by->setAnchorPoint({ 0, 0.5f });
        by->setScale(0.42f);
        by->limitLabelWidth(150.f, 0.42f, 0.15f);
        by->setColor({ 200, 200, 200 });
        by->setPosition({ 44, y - 8 });
        content->addChild(by, 1);

        auto star = CCSprite::createWithSpriteFrameName("GJ_starsIcon_001.png");
        star->setScale(0.7f);
        if (!store.isFavorite(t.id)) { star->setColor({ 90, 90, 90 }); star->setOpacity(160); }
        auto favBtn = CCMenuItemSpriteExtra::create(star, this, menu_selector(PickerPopup::onFav));
        favBtn->setTag(tag);
        favBtn->setPosition({ 208, y });
        menu->addChild(favBtn);

        if (t.custom) {
            auto trash = CCSprite::createWithSpriteFrameName("GJ_trashBtn_001.png");
            trash->setScale(0.45f);
            auto del = CCMenuItemSpriteExtra::create(trash, this, menu_selector(PickerPopup::onDelete));
            del->setTag(tag);
            del->setPosition({ 186, y });
            menu->addChild(del);
        }

        auto use = textButton("Use", this, menu_selector(PickerPopup::onUse), "GJ_button_01.png", 0.5f);
        use->setTag(tag);
        use->setPosition({ 250, y });
        menu->addChild(use);
    }
    m_scroll->scrollToTop();
}

static CCMenuItemToggler* addToggle(CCMenu* menu, CCNode* parent, CCObject* target, SEL_MenuHandler sel, char const* text, bool on, int tag, CCPoint pos) {
    auto t = CCMenuItemToggler::createWithStandardSprites(target, sel, 0.55f);
    t->toggle(on);
    t->setTag(tag);
    t->setPosition(pos);
    menu->addChild(t);
    auto l = CCLabelBMFont::create(text, "bigFont.fnt");
    l->setScale(0.32f);
    l->setAnchorPoint({ 0, 0.5f });
    l->setPosition({ pos.x + 13, pos.y });
    parent->addChild(l);
    return t;
}

static char const* densityName(int d) { return d == 0 ? "Light" : d == 2 ? "Heavy" : "Medium"; }

void PickerPopup::buildOptions() {
    float x = 312.f;
    auto menu = CCMenu::create();
    menu->setPosition({ 0, 0 });
    m_mainLayer->addChild(menu, 3);

    auto dl = CCLabelBMFont::create("Density", "goldFont.fnt");
    dl->setScale(0.45f);
    dl->setPosition({ x + 58, 228 });
    m_mainLayer->addChild(dl);
    m_densityBtn = textButton(densityName(m_set.density), this, menu_selector(PickerPopup::onDensity), "GJ_button_04.png", 0.5f);
    m_densityBtn->setPosition({ x + 58, 208 });
    menu->addChild(m_densityBtn);

    addToggle(menu, m_mainLayer, this, menu_selector(PickerPopup::onToggle), "Gradient", m_set.gradient, 1, { x + 4, 182 });
    addToggle(menu, m_mainLayer, this, menu_selector(PickerPopup::onToggle), "Mirror", m_set.mirror, 2, { x + 4, 162 });
    addToggle(menu, m_mainLayer, this, menu_selector(PickerPopup::onToggle), "Animate", m_set.animate, 3, { x + 4, 142 });
    addToggle(menu, m_mainLayer, this, menu_selector(PickerPopup::onToggle), "Keep my blocks", m_set.keepBlocks, 4, { x + 4, 122 });

    auto rnd = textButton("Random", this, menu_selector(PickerPopup::onRandom), "GJ_button_02.png", 0.45f);
    rnd->setPosition({ x + 58, 96 });
    menu->addChild(rnd);
    auto match = textButton("Match nearby", this, menu_selector(PickerPopup::onMatch), "GJ_button_02.png", 0.45f);
    match->setPosition({ x + 58, 72 });
    menu->addChild(match);
    auto save = textButton("Save as template", this, menu_selector(PickerPopup::onSaveTemplate), "GJ_button_05.png", 0.4f);
    save->setPosition({ x + 58, 48 });
    menu->addChild(save);

    auto info = CCLabelBMFont::create(fmt::format("{} blocks selected", m_sel.grid.size()).c_str(), "chatFont.fnt");
    info->limitLabelWidth(110.f, 0.5f, 0.2f);
    info->setScale(0.5f);
    info->setPosition({ x + 58, 26 });
    info->setColor({ 200, 200, 200 });
    m_mainLayer->addChild(info);
}

void PickerPopup::start(deco::Template const& t, uint64_t seed) {
    if (!t.scene && m_sel.grid.empty()) {
        FLAlertLayer::create("Auto Decorator", "Select the <cy>blocks</c> of a structure first, then tap Deco.\n<cg>Scenes</c> (at the bottom of the list) work without blocks: they fill the screen or the selected area.", "OK")->show();
        return;
    }
    auto ui = m_ui;
    auto sel = m_sel;
    deco::Template copy = t;
    this->onClose(nullptr);
    if (auto old = PreviewPanel::current(ui)) old->cancel();
    ui->deselectAll();
    auto panel = PreviewPanel::create(ui, copy, sel, seed);
    if (!panel) return;
    if (copy.scene) {
        // scenes fill the selected area, or what's on screen if only one thing (or nothing) is selected
        float x0, y0, x1, y1;
        if (sel.hasBox && sel.bx1 - sel.bx0 > 90 && sel.by1 - sel.by0 > 60) { x0 = sel.bx0; y0 = sel.by0; x1 = sel.bx1; y1 = sel.by1; }
        else bridge::cameraRect(ui->m_editorLayer, x0, y0, x1, y1);
        panel->setArea(x0, y0, x1, y1);
    }
    ui->addChild(panel, 200);
    panel->regenerate();
}

void PickerPopup::onUse(CCObject* sender) {
    int tag = sender->getTag();
    if (tag < 0 || tag >= (int)m_ids.size()) return;
    if (auto t = TemplateStore::get().find(m_ids[tag])) start(*t, freshSeed());
}

void PickerPopup::onRandom(CCObject*) {
    auto const& all = TemplateStore::get().all();
    if (all.empty()) return;
    auto s = freshSeed();
    start(all[s % all.size()], deco::hash(s, 99));
}

void PickerPopup::onFav(CCObject* sender) {
    int tag = sender->getTag();
    if (tag < 0 || tag >= (int)m_ids.size()) return;
    auto& store = TemplateStore::get();
    auto id = m_ids[tag];
    store.setFavorite(id, !store.isFavorite(id));
    // rebuild on the next frame (we are inside the button's callback)
    Ref<PickerPopup> self = this;
    Loader::get()->queueInMainThread([self]() { self->buildList(); });
}

void PickerPopup::onDelete(CCObject* sender) {
    int tag = sender->getTag();
    if (tag < 0 || tag >= (int)m_ids.size()) return;
    auto id = m_ids[tag];
    Ref<PickerPopup> self = this;
    createQuickPopup("Delete template", "Delete this template for good?", "Cancel", "Delete", [self, id](FLAlertLayer*, bool yes) {
        if (!yes) return;
        TemplateStore::get().deleteCustom(id);
        self->buildList();
    });
}

void PickerPopup::onDensity(CCObject*) {
    m_set.density = (m_set.density + 1) % 3;
    m_set.save();
    if (auto spr = typeinfo_cast<ButtonSprite*>(m_densityBtn->getNormalImage())) spr->setString(densityName(m_set.density));
}

void PickerPopup::onToggle(CCObject* sender) {
    auto t = static_cast<CCMenuItemToggler*>(sender);
    bool on = !t->isToggled(); // the toggler flips after the callback
    switch (t->getTag()) {
        case 1: m_set.gradient = on; break;
        case 2: m_set.mirror = on; break;
        case 3: m_set.animate = on; break;
        case 4: m_set.keepBlocks = on; break;
    }
    m_set.save();
}

void PickerPopup::onMatch(CCObject*) {
    auto lel = m_ui->m_editorLayer;
    auto rec = TemplateStore::get().nearest("level", m_sel.grid);
    if (!rec) {
        FLAlertLayer::create("Match nearby", "Nothing decorated in this level yet. Decorate a structure first, then use <cy>Match nearby</c> on the next one.", "OK")->show();
        return;
    }
    auto t = TemplateStore::get().find(rec->templateId);
    if (!t) {
        FLAlertLayer::create("Match nearby", "The template used nearby doesn't exist anymore.", "OK")->show();
        return;
    }
    start(*t, rec->seed);
}

void PickerPopup::onSaveTemplate(CCObject*) {
    if (m_sel.grid.empty() || m_sel.decoObjects.empty()) {
        FLAlertLayer::create("Save as template",
            "Decorate a small structure by hand, then select <cy>the blocks and their decoration</c> together and tap this again.\n"
            "The mod learns what goes on tops, sides, corners and inside.", "OK")->show();
        return;
    }
    Ref<PickerPopup> self = this;
    auto popup = NamePopup::create([self](std::string name) {
        deco::Template t;
        if (!bridge::learnFromSelection(self->m_ui->m_editorLayer, self->m_sel, name, t)) {
            Notification::create("Couldn't learn from this selection", NotificationIcon::Error)->show();
            return;
        }
        if (TemplateStore::get().saveCustom(t)) {
            Notification::create(fmt::format("Saved template \"{}\"", name), NotificationIcon::Success)->show();
            TemplateStore::get().setFavorite(t.id, true);
            self->buildList();
        } else {
            Notification::create("Couldn't save the template", NotificationIcon::Error)->show();
        }
    });
    if (popup) popup->show();
}

// =====================================================================
// name popup
// =====================================================================
NamePopup* NamePopup::create(std::function<void(std::string)> done) {
    auto r = new NamePopup();
    if (r->init(std::move(done))) { r->autorelease(); return r; }
    delete r;
    return nullptr;
}

bool NamePopup::init(std::function<void(std::string)> done) {
    if (!Popup::init(260.f, 140.f)) return false;
    m_done = std::move(done);
    this->setTitle("Template name");
    m_input = TextInput::create(200.f, "My style");
    m_input->setMaxCharCount(24);
    m_input->setPosition({ 130.f, 74.f });
    m_mainLayer->addChild(m_input);
    auto menu = CCMenu::create();
    menu->setPosition({ 0, 0 });
    auto ok = textButton("Save", this, menu_selector(NamePopup::onOk), "GJ_button_01.png", 0.7f);
    ok->setPosition({ 130.f, 28.f });
    menu->addChild(ok);
    m_mainLayer->addChild(menu);
    return true;
}

void NamePopup::onOk(CCObject*) {
    std::string name = m_input->getString();
    while (!name.empty() && name.back() == ' ') name.pop_back();
    if (name.empty()) name = "My style";
    auto done = m_done;
    this->onClose(nullptr);
    if (done) done(name);
}

// =====================================================================
void openDecorator(EditorUI* ui) {
    if (!ui) return;
    if (PreviewPanel::current(ui)) {
        Notification::create("Apply or cancel the current preview first", NotificationIcon::Info)->show();
        return;
    }
    auto sel = bridge::readSelection(ui->m_editorLayer, ui->getSelectedObjects());
    if (sel.grid.size() > 4000) {
        FLAlertLayer::create("Auto Decorator", "That selection is very big. Decorate it in smaller parts (up to about 4000 blocks).", "OK")->show();
        return;
    }
    if (auto p = PickerPopup::create(ui, std::move(sel))) p->show();
}
