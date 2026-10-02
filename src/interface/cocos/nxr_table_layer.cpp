#include "nxr_table_layer.hpp"
#include "nxr_window_popup.hpp"
#include "nxr_hack_settings_popup.hpp"
#include "nxr_hacks_layer.hpp"
#include "nxr_bot_popups.hpp"
#include "nxr_text_style.hpp"
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_theme.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_keybinds.hpp"
#include "../../core/nxr_ui_mode.hpp"
#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace {
    constexpr float kRowH = 20.f;
    constexpr float kTitleH = 22.f;
    constexpr float kWinW = 150.f;
    constexpr float kGap = 6.f;
    constexpr float kDragThreshold = 6.f;
    constexpr float kLongPress = 0.6f;
    constexpr float kLabelScale = 0.42f;
    constexpr float kWheelDir = 1.f; // flip to -1 if the mouse wheel feels inverted

    const ccColor3B kOnColor = {255, 255, 255};
    const ccColor3B kOffColor = {150, 150, 155};
    const ccColor3B kCheatOn = {255, 140, 140};

    std::string key(const char* what, const std::string& name) {
        return std::string("nxr.table.") + what + "." + name;
    }

    std::string layoutName(int v) { return v == NXR::Ui::Table ? "Table" : "Panel"; }

    std::string themeName(int v) {
        switch (v) {
            case NXR::Theme::Basic: return "Basic";
            case NXR::Theme::Medium: return "Medium";
            case NXR::Theme::Pro: return "Pro";
            default: return "Normal";
        }
    }

    ccColor3B rowColor(bool enabled, bool cheating) {
        if (!enabled) return kOffColor;
        return NXR::Ui::textColor(cheating ? kCheatOn : kOnColor);
    }
}

NXRTableLayer* NXRTableLayer::s_instance = nullptr;

NXRTableLayer::~NXRTableLayer() {
    if (s_instance == this) s_instance = nullptr;
}

NXRTableLayer* NXRTableLayer::get() { return s_instance; }
bool NXRTableLayer::isOpened() { return s_instance != nullptr; }

NXRTableLayer* NXRTableLayer::create() {
    auto* ret = new NXRTableLayer();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

void NXRTableLayer::open() {
    if (s_instance) return;
    auto* layer = create();
    if (!layer) return;
    s_instance = layer;
    OverlayManager::get()->addChild(layer, 900);
}

void NXRTableLayer::close() {
    if (!s_instance) return;
    auto* layer = s_instance;
    layer->savePositions();
    NXRConfig::get().save(getFileDataPath());
    s_instance = nullptr;
    layer->removeFromParent();
}

bool NXRTableLayer::init() {
    if (!CCLayer::init()) return false;

    this->setTouchEnabled(true);
#ifdef GEODE_IS_DESKTOP
    this->setMouseEnabled(true);
#endif
    this->setZOrder(900);
    this->rebuild();
    this->scheduleUpdate();
    return true;
}

void NXRTableLayer::registerWithTouchDispatcher() {
    // Above the game and its menus (-128), below popups (-500) and the floating button (-1000)
    CCTouchDispatcher::get()->addTargetedDelegate(this, -300, true);
}

void NXRTableLayer::update(float dt) {
    m_refreshTimer += dt;
    if (m_refreshTimer >= 0.2f) {
        m_refreshTimer = 0.f;
        this->refreshRows();
    }
}

// ---------------------------------------------------------------- building

std::vector<NXRTableLayer::Row> NXRTableLayer::settingsRows() {
    std::vector<Row> rows;

    {
        Row r;
        r.text = [] { return "Layout: " + layoutName(NXR::Ui::layout()); };
        r.onTap = [] {
            NXRConfig::get().set<int>(NXR::Ui::kLayoutKey, NXR::Ui::layout() == NXR::Ui::Table ? NXR::Ui::Panel : NXR::Ui::Table);
            NXR::Ui::reopenMenu();
        };
        rows.push_back(std::move(r));
    }
    {
        Row r;
        r.text = [] { return "Theme: " + themeName(NXR::Theme::current()); };
        r.onTap = [] {
            int next = NXR::Theme::current() + 1;
            if (next > NXR::Theme::Pro) next = NXR::Theme::Basic;
            NXRConfig::get().set<int>(NXR::Theme::kKey, next);
            if (auto* layer = NXRTableLayer::get()) layer->rebuild();
        };
        rows.push_back(std::move(r));
    }
    {
        Row r;
        r.text = [] { return fmt::format("UI Scale: {:.2f}", NXR::Ui::tableScale()); };
        r.onTap = [] {
            constexpr float steps[] = {0.7f, 0.85f, 1.f, 1.15f, 1.3f, 1.5f};
            const float current = NXR::Ui::tableScale();
            float next = steps[0];
            for (float step : steps) {
                if (step > current + 0.01f) { next = step; break; }
            }
            NXRConfig::get().set<float>(NXR::Ui::kTableScaleKey, next);
            if (auto* layer = NXRTableLayer::get()) layer->rebuild();
        };
        rows.push_back(std::move(r));
    }
    {
        Row r;
        r.text = [] { return std::string("Toggle Style: ") + (NXR::Ui::toggleStyle() == NXR::Ui::Checkbox ? "Checkbox" : "Switch"); };
        r.onTap = [] {
            NXRConfig::get().set<int>(NXR::Ui::kToggleStyleKey, NXR::Ui::toggleStyle() == NXR::Ui::Checkbox ? NXR::Ui::Switch : NXR::Ui::Checkbox);
            NXR::Ui::reopenMenu();
        };
        rows.push_back(std::move(r));
    }
    {
        Row r;
        r.label = "Open Menu Key / Font...";
        r.onTap = [] {
            if (auto* popup = NXRWindowPopup::create("Settings", [](NXRHacksTab* tab) { nxrBuildSettingsTab(tab); })) popup->show();
        };
        rows.push_back(std::move(r));
    }
    {
        Row r;
        r.label = "Reset Window Positions";
        r.onTap = [] {
            auto& config = NXRConfig::get();
            for (auto& window : NXR::Gui::get().getWindows()) {
                config.set<float>(key("x", window.getName()), -1.f);
                config.set<float>(key("y", window.getName()), -1.f);
                config.set<bool>(key("collapsed", window.getName()), false);
            }
            config.set<float>(key("x", "Settings"), -1.f);
            config.set<float>(key("y", "Settings"), -1.f);
            config.set<bool>(key("collapsed", "Settings"), false);
            if (auto* layer = NXRTableLayer::get()) layer->rebuild(false);
        };
        rows.push_back(std::move(r));
    }

    return rows;
}

void NXRTableLayer::rebuild(bool save) {
    if (save) this->savePositions();

    for (auto& w : m_wins) {
        if (w.node) w.node->removeFromParent();
    }
    m_wins.clear();
    m_grab = Grab::None;
    m_active = -1;
    m_row = -1;
    this->unschedule(schedule_selector(NXRTableLayer::onLongPress));

    struct Pending {
        std::string name;
        std::vector<Row> rows;
    };
    std::vector<Pending> pending;

    for (auto& window : NXR::Gui::get().getWindows()) {
        const std::string name = window.getName();
        if (name == "Settings") continue;

        Pending p;
        p.name = name;

        for (auto& hack : window.getHacks()) {
            Row r;
            r.label = hack.getName();
            r.hackId = hack.getID();
            r.desc = hack.getDesc();
            r.cheating = hack.isCheating();
            r.disabled = hack.getDisabled();
            if (hack.avaibleCustomWindowCocos()) {
                const std::string id = hack.getID();
                r.onSettings = [id] {
                    if (auto* h = NXR::Gui::get().findHackByIDGlobal(id)) {
                        if (auto* popup = NXRHackSettingsPopup::create(*h)) popup->show();
                    }
                };
            }
            p.rows.push_back(std::move(r));
        }

        if (window.avaibleCustomWindowCocos()) {
            Row r;
            r.label = name + " Panel...";
            r.onTap = [name] {
                auto* popup = NXRWindowPopup::create(name, [name](NXRHacksTab* tab) {
                    tab->addPadding(2.5f);
                    NXR::Gui::get().getWindow(name).callCustomWindowCocos(tab);
                });
                if (popup) popup->show();
            };
            // Custom controls (record, playback...) go first, they are what people reach for
            p.rows.insert(p.rows.begin(), std::move(r));
        }

        if (!p.rows.empty()) pending.push_back(std::move(p));
    }

    pending.push_back({"Settings", this->settingsRows()});

    const int total = static_cast<int>(pending.size());
    int index = 0;
    for (auto& p : pending) {
        this->buildWindow(p.name, std::move(p.rows), index++, total);
    }

    this->refreshRows();
}

void NXRTableLayer::drawArrow(Win& w) {
    if (!w.arrow) return;
    w.arrow->clear();

    const ccColor4F color = {1.f, 1.f, 1.f, 0.95f};
    const float cx = 11.f;
    const float cy = -kTitleH / 2.f;

    if (w.collapsed) {
        CCPoint tri[3] = {{cx - 3.f, cy + 4.f}, {cx - 3.f, cy - 4.f}, {cx + 4.f, cy}};
        w.arrow->drawPolygon(tri, 3, color, 0.f, color);
    } else {
        CCPoint tri[3] = {{cx - 4.f, cy + 3.f}, {cx + 4.f, cy + 3.f}, {cx, cy - 4.f}};
        w.arrow->drawPolygon(tri, 3, color, 0.f, color);
    }
}

void NXRTableLayer::buildWindow(const std::string& name, std::vector<Row> rows, int index, int total) {
    auto& config = NXRConfig::get();
    const float scale = NXR::Ui::tableScale();
    const auto winSize = CCDirector::sharedDirector()->getWinSize();

    m_wins.emplace_back();
    Win& w = m_wins.back();
    w.name = name;
    w.rows = std::move(rows);
    w.contentH = static_cast<float>(w.rows.size()) * kRowH;

    const float maxBody = std::max(kRowH * 3.f, (winSize.height - 12.f) / scale - kTitleH);
    w.bodyH = std::min(w.contentH, maxBody);

    // Not enough room for every window side by side: start collapsed so the title bars wrap into a grid
    const bool crowded = static_cast<float>(total) * (kWinW * scale + kGap) + kGap > winSize.width;
    w.collapsed = config.get<bool>(key("collapsed", name), crowded);

    w.node = CCNode::create();
    w.node->setScale(scale);
    this->addChild(w.node);

    // Title bar
    auto* title = CCLayerColor::create(ccc4(NXR::Theme::accent().r, NXR::Theme::accent().g, NXR::Theme::accent().b, 245), kWinW, kTitleH);
    title->setPosition({0.f, -kTitleH});
    w.node->addChild(title);

    auto* titleLabel = geode::Label::create(name, "GoogleSans.fnt"_spr);
    titleLabel->setAnchorPoint({0.f, 0.5f});
    titleLabel->setScale(0.5f * NXR::Ui::fontScale());
    titleLabel->setPosition({22.f, -kTitleH / 2.f});
    titleLabel->setColor({255, 255, 255});
    w.node->addChild(titleLabel);

    w.arrow = CCDrawNode::create();
    w.node->addChild(w.arrow);
    this->drawArrow(w);

    // Body
    w.body = CCLayerColor::create(ccc4(32, 32, 36, 238), kWinW, w.bodyH);
    w.body->setPosition({0.f, -kTitleH - w.bodyH});
    w.node->addChild(w.body);

    w.scroll = geode::ScrollLayer::create({kWinW, w.bodyH});
    w.scroll->setPosition({0.f, -kTitleH - w.bodyH});
    w.scroll->setTouchEnabled(false);
#ifdef GEODE_IS_DESKTOP
    w.scroll->setMouseEnabled(false);
#endif
    w.scroll->m_contentLayer->setContentSize({kWinW, w.contentH});
    w.scroll->m_contentLayer->setPositionY(w.bodyH - w.contentH);
    w.node->addChild(w.scroll);

    for (size_t i = 0; i < w.rows.size(); i++) {
        Row& r = w.rows[i];
        const float y = w.contentH - static_cast<float>(i + 1) * kRowH;

        const bool checkbox = !r.hackId.empty() && NXR::Ui::toggleStyle() == NXR::Ui::Checkbox;
        const float labelX = checkbox ? 24.f : 7.f;

        r.labelNode = geode::Label::create(r.text ? r.text() : r.label, "GoogleSans.fnt"_spr);
        r.labelNode->setAnchorPoint({0.f, 0.5f});
        r.labelNode->setPosition({labelX, y + kRowH / 2.f});
        r.labelNode->setScale(kLabelScale * NXR::Ui::fontScale());
        const float maxWidth = kWinW - labelX - (r.onSettings ? 20.f : 12.f);
        if (r.labelNode->getScaledContentWidth() > maxWidth) {
            r.labelNode->setScale(r.labelNode->getScale() * (maxWidth / r.labelNode->getScaledContentWidth()));
        }
        w.scroll->m_contentLayer->addChild(r.labelNode);

        if (checkbox) {
            r.checkOn = CCSprite::createWithSpriteFrameName(NXR::Ui::kCheckOnFrame);
            r.checkOff = CCSprite::createWithSpriteFrameName(NXR::Ui::kCheckOffFrame);
            for (auto* sprite : {r.checkOn, r.checkOff}) {
                sprite->setScale(0.4f);
                sprite->setPosition({13.f, y + kRowH / 2.f});
                w.scroll->m_contentLayer->addChild(sprite);
            }
        } else if (!r.hackId.empty()) {
            r.bar = CCLayerColor::create(ccc4(255, 255, 255, 235), 3.f, kRowH - 8.f);
            r.bar->setPosition({kWinW - 6.f, y + 4.f});
            w.scroll->m_contentLayer->addChild(r.bar);
        }

        if (r.onSettings) {
            auto* corner = CCDrawNode::create();
            CCPoint tri[3] = {{kWinW - 12.f, y + 2.f}, {kWinW - 4.f, y + 2.f}, {kWinW - 4.f, y + 10.f}};
            const ccColor4F color = {0.65f, 0.65f, 0.7f, 1.f};
            corner->drawPolygon(tri, 3, color, 0.f, color);
            w.scroll->m_contentLayer->addChild(corner);
        }
    }

    w.body->setVisible(!w.collapsed);
    w.scroll->setVisible(!w.collapsed);

    // Position: remembered, otherwise a left to right flow that wraps
    float x = config.get<float>(key("x", name), -1.f);
    float y = config.get<float>(key("y", name), -1.f);
    if (x < 0.f || y < 0.f) {
        const float cell = kWinW * scale + kGap;
        const int perRow = std::max(1, static_cast<int>((winSize.width - kGap) / cell));
        const int col = index % perRow;
        const int line = index / perRow;
        const float lineH = w.collapsed ? (kTitleH * scale + kGap) : (winSize.height - 12.f);
        x = kGap + static_cast<float>(col) * cell;
        y = winSize.height - kGap - static_cast<float>(line) * lineH;
    }
    w.node->setPosition(this->clampTopLeft(w, {x, y}));
}

// ---------------------------------------------------------------- state

void NXRTableLayer::refreshRow(Row& row) {
    if (!row.labelNode) return;

    bool enabled = false;
    if (!row.hackId.empty()) {
        if (auto* hack = NXR::Gui::get().findHackByIDGlobal(row.hackId)) enabled = hack->getEnabled();
    } else if (row.text) {
        const std::string text = row.text();
        if (text != row.label) {
            row.label = text;
            const float scale = kLabelScale * NXR::Ui::fontScale();
            row.labelNode->setString(text.c_str());
            row.labelNode->setScale(scale);
            const float maxWidth = kWinW - 7.f - 12.f;
            if (row.labelNode->getScaledContentWidth() > maxWidth) {
                row.labelNode->setScale(scale * (maxWidth / row.labelNode->getScaledContentWidth()));
            }
        }
        enabled = true;
    } else {
        enabled = true;
    }

    row.labelNode->setColor(row.hackId.empty() ? NXR::Ui::textColor(kOnColor) : rowColor(enabled, row.cheating));
    if (row.disabled) row.labelNode->setOpacity(100);
    if (row.bar) row.bar->setVisible(enabled && !row.hackId.empty());
    if (row.checkOn) row.checkOn->setVisible(enabled);
    if (row.checkOff) row.checkOff->setVisible(!enabled);
}

void NXRTableLayer::refreshRows() {
    for (auto& w : m_wins) {
        if (w.collapsed) continue;
        for (auto& row : w.rows) this->refreshRow(row);
    }
}

void NXRTableLayer::savePositions() {
    auto& config = NXRConfig::get();
    for (auto& w : m_wins) {
        if (!w.node) continue;
        config.set<float>(key("x", w.name), w.node->getPositionX());
        config.set<float>(key("y", w.name), w.node->getPositionY());
        config.set<bool>(key("collapsed", w.name), w.collapsed);
    }
}

float NXRTableLayer::windowHeight(const Win& w) const {
    return kTitleH + (w.collapsed ? 0.f : w.bodyH);
}

CCPoint NXRTableLayer::clampTopLeft(const Win& w, CCPoint p) const {
    const auto winSize = CCDirector::sharedDirector()->getWinSize();
    const float scale = w.node ? w.node->getScale() : 1.f;
    const float width = kWinW * scale;
    const float height = windowHeight(w) * scale;

    p.x = std::clamp(p.x, 0.f, std::max(0.f, winSize.width - width));
    p.y = std::clamp(p.y, std::min(height, winSize.height), winSize.height);
    return p;
}

void NXRTableLayer::bringToFront(int index) {
    if (index < 0 || index >= static_cast<int>(m_wins.size())) return;
    int top = 0;
    for (auto& w : m_wins) top = std::max(top, w.node->getZOrder());
    m_wins[index].node->setZOrder(top + 1);
}

// ---------------------------------------------------------------- hit testing

int NXRTableLayer::windowAt(const CCPoint& world) const {
    int best = -1;
    int bestZ = -1;
    for (size_t i = 0; i < m_wins.size(); i++) {
        const Win& w = m_wins[i];
        if (!w.node) continue;
        const auto p = w.node->convertToNodeSpace(world);
        if (p.x < 0.f || p.x > kWinW) continue;
        if (p.y > 0.f || p.y < -windowHeight(w)) continue;
        if (w.node->getZOrder() >= bestZ) {
            bestZ = w.node->getZOrder();
            best = static_cast<int>(i);
        }
    }
    return best;
}

int NXRTableLayer::rowAt(Win& w, const CCPoint& world) const {
    if (w.collapsed || !w.scroll) return -1;
    const auto p = w.scroll->m_contentLayer->convertToNodeSpace(world);
    if (p.x < 0.f || p.x > kWinW) return -1;
    const int index = static_cast<int>(std::floor((w.contentH - p.y) / kRowH));
    if (index < 0 || index >= static_cast<int>(w.rows.size())) return -1;
    return index;
}

void NXRTableLayer::scrollBy(Win& w, float dy, bool clamp) {
    if (!w.scroll || w.contentH <= w.bodyH) return;
    auto* content = w.scroll->m_contentLayer;
    const float minY = w.bodyH - w.contentH;
    const float slack = clamp ? 0.f : 12.f;
    content->setPositionY(std::clamp(content->getPositionY() + dy, minY - slack, slack));
}

// ---------------------------------------------------------------- input

bool NXRTableLayer::ccTouchBegan(CCTouch* touch, CCEvent*) {
    const auto world = touch->getLocation();
    const int index = this->windowAt(world);
    if (index < 0) return false; // clicks outside every window go to the game

    Win& w = m_wins[index];
    this->bringToFront(index);

    m_active = index;
    m_moved = false;
    m_longPressed = false;
    m_start = world;
    m_lastLocal = w.node->convertToNodeSpace(world);
    m_row = -1;

    const auto local = m_lastLocal;
    if (local.y > -kTitleH) {
        m_grab = Grab::Title;
        m_grabOffset = w.node->getPosition() - this->convertToNodeSpace(world);
    } else {
        m_grab = Grab::Body;
        m_row = this->rowAt(w, world);
        if (m_row >= 0 && !w.rows[m_row].desc.empty()) {
            this->scheduleOnce(schedule_selector(NXRTableLayer::onLongPress), kLongPress);
        }
    }
    return true;
}

void NXRTableLayer::onLongPress(float) {
    if (m_grab != Grab::Body || m_moved || m_active < 0 || m_row < 0) return;
    if (m_active >= static_cast<int>(m_wins.size())) return;

    auto& row = m_wins[m_active].rows[m_row];
    if (row.desc.empty()) return;

    m_longPressed = true;
    if (auto* popup = NXRInfoPopup::create(row.label, row.desc)) popup->show();
}

void NXRTableLayer::ccTouchMoved(CCTouch* touch, CCEvent*) {
    if (m_active < 0 || m_active >= static_cast<int>(m_wins.size())) return;
    Win& w = m_wins[m_active];
    const auto world = touch->getLocation();

    if (!m_moved && ccpDistance(world, m_start) < kDragThreshold) return;
    if (!m_moved) {
        m_moved = true;
        this->unschedule(schedule_selector(NXRTableLayer::onLongPress));
    }

    if (m_grab == Grab::Title) {
        w.node->setPosition(this->clampTopLeft(w, this->convertToNodeSpace(world) + m_grabOffset));
    } else if (m_grab == Grab::Body) {
        const auto local = w.node->convertToNodeSpace(world);
        this->scrollBy(w, local.y - m_lastLocal.y, false);
        m_lastLocal = local;
    }
}

void NXRTableLayer::ccTouchEnded(CCTouch* touch, CCEvent*) {
    // A tap can close or rebuild this layer (layout switch), keep it alive until we are done with it
    this->retain();
    this->unschedule(schedule_selector(NXRTableLayer::onLongPress));

    if (m_active >= 0 && m_active < static_cast<int>(m_wins.size())) {
        Win& w = m_wins[m_active];

        if (m_grab == Grab::Title) {
            if (!m_moved) {
                w.collapsed = !w.collapsed;
                w.body->setVisible(!w.collapsed);
                w.scroll->setVisible(!w.collapsed);
                this->drawArrow(w);
                w.node->setPosition(this->clampTopLeft(w, w.node->getPosition()));
                this->refreshRows();
            }
            this->savePositions();
        } else if (m_grab == Grab::Body) {
            if (m_moved) {
                this->scrollBy(w, 0.f, true);
            } else if (!m_longPressed && m_row >= 0 && m_row < static_cast<int>(w.rows.size())) {
                Row& row = w.rows[m_row];
                const auto cp = w.scroll->m_contentLayer->convertToNodeSpace(touch->getLocation());
                const bool onCorner = row.onSettings && cp.x > kWinW - 24.f;

                if (onCorner) {
                    auto fn = row.onSettings;
                    fn();
                } else if (!row.hackId.empty()) {
                    if (!row.disabled) {
                        if (auto* hack = NXR::Gui::get().findHackByIDGlobal(row.hackId)) hack->toggle();
                        this->refreshRow(row);
                    }
                } else if (row.onTap) {
                    // The callback may rebuild this layer, so copy it and touch nothing afterwards
                    auto fn = row.onTap;
                    fn();
                    if (NXRTableLayer::get() == this && m_active < static_cast<int>(m_wins.size())) this->refreshRows();
                }
            }
        }
    }

    m_grab = Grab::None;
    m_active = -1;
    m_row = -1;
    m_moved = false;
    this->release();
}

void NXRTableLayer::ccTouchCancelled(CCTouch* touch, CCEvent* event) {
    this->unschedule(schedule_selector(NXRTableLayer::onLongPress));
    if (m_grab == Grab::Body && m_active >= 0 && m_active < static_cast<int>(m_wins.size())) {
        this->scrollBy(m_wins[m_active], 0.f, true);
    }
    m_grab = Grab::None;
    m_active = -1;
    m_row = -1;
    m_moved = false;
}

#ifdef GEODE_IS_DESKTOP
void NXRTableLayer::scrollWheel(float y, float) {
    const int index = this->windowAt(geode::cocos::getMousePos());
    if (index < 0) return;
    Win& w = m_wins[index];
    if (w.collapsed) return;
    this->scrollBy(w, y * kWheelDir, true);
}
#endif

// ---------------------------------------------------------------- menu keybind

$execute {
    NXR::Keybinds::get().registerAction("nxr.menu::toggle", "Open Menu", geode::Keybind(cocos2d::KEY_Tab, geode::KeyboardModifier::None), [](bool repeat) {
        if (!repeat) NXR::Ui::toggleMenu();
    });
}
