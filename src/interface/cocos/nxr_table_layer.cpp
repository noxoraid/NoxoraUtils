#include "nxr_table_layer.hpp"
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
#include <map>

using namespace geode::prelude;

namespace {
    constexpr float kRowH = 20.f;
    constexpr float kTitleH = 22.f;
    constexpr float kWinW = 180.f;
    constexpr float kGap = 6.f;
    constexpr float kWideW = kWinW * 2.f + kGap;
    constexpr float kDragThreshold = 6.f;
    constexpr float kLongPress = 0.6f;
    constexpr float kLabelScale = 0.42f;
    constexpr float kWheelDir = 1.f;
    constexpr int kTablePriority = -900;
    constexpr int kHostPriority = -899;

    const ccColor3B kOnColor = {255, 255, 255};
    const ccColor3B kCheatColor = {255, 120, 140};
    const ccColor4B kBodyColor = {34, 32, 62, 245};
    const ccColor4B kTitleColor = {44, 42, 84, 252};
    const ccColor4B kButtonColor = {72, 68, 142, 255};

    std::string key(const char* what, const std::string& name) {
        return std::string("nxr.table2.") + what + "." + name;
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

    CCSprite* fitSprite(const char* file, float targetWidth) {
        auto* sprite = CCSprite::create(file);
        if (!sprite) return nullptr;
        const float width = sprite->getContentSize().width;
        if (width > 0.f) sprite->setScale(targetWidth / width);
        return sprite;
    }

    void keepTouchScroll(geode::ScrollLayer* scroll) {
        if (!scroll) return;
#ifdef GEODE_IS_DESKTOP
        scroll->setMouseEnabled(false);
#endif
    }

    void disableScrollInput(geode::ScrollLayer* scroll) {
        if (!scroll) return;
        scroll->setTouchEnabled(false);
#ifdef GEODE_IS_DESKTOP
        scroll->setMouseEnabled(false);
#endif
    }
}

NXRTableLayer* NXRTableLayer::s_instance = nullptr;

NXRTableLayer::~NXRTableLayer() {
    if (s_instance == this) s_instance = nullptr;
}

NXRTableLayer* NXRTableLayer::get() { return s_instance; }
bool NXRTableLayer::isOpened() { return s_instance != nullptr; }

bool NXRTableLayer::popupBlocking() {
    auto isAlert = [](CCNode* parent) {
        if (!parent) return false;
        auto* children = parent->getChildren();
        if (!children) return false;
        for (auto* child : CCArrayExt<CCNode*>(children)) {
            if (child == s_instance) continue;
            if (child->isVisible() && typeinfo_cast<FLAlertLayer*>(child)) return true;
        }
        return false;
    };
    if (isAlert(CCDirector::sharedDirector()->getRunningScene())) return true;
    return isAlert(OverlayManager::get());
}

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

void NXRTableLayer::openHackSettings(NXR::Hack& hack, const std::string& origin) {
    if (!s_instance) {
        if (auto* popup = NXRHackSettingsPopup::create(hack)) popup->show();
        return;
    }
    Spec spec;
    spec.name = hack.getName() + " Settings";
    spec.id = "settings:" + hack.getID() + ":" + hack.getName();
    spec.kind = Kind::HackSettings;
    spec.hack = &hack;
    s_instance->openExtra(std::move(spec), origin);
}

bool NXRTableLayer::hostPopup(geode::Popup* popup, const std::string& title) {
    if (!s_instance || !popup) return false;
    Spec spec;
    spec.name = title;
    spec.id = "popup:" + title;
    spec.kind = Kind::Embedded;
    spec.popup = popup;
    s_instance->openExtra(std::move(spec), "");
    return true;
}

void NXRTableLayer::openChoice(const std::string& title, const std::vector<std::string>& notes, const std::vector<std::pair<std::string, std::function<void()>>>& choices) {
    if (!s_instance) return;
    Spec spec;
    spec.name = title;
    spec.id = "choice:" + title;
    spec.kind = Kind::Rows;
    for (auto& note : notes) {
        Row r;
        r.label = note;
        spec.rows.push_back(std::move(r));
    }
    for (auto& choice : choices) {
        Row r;
        r.label = choice.first;
        const std::string id = spec.id;
        const std::function<void()> action = choice.second;
        r.onTap = [id, action] {
            if (auto* layer = NXRTableLayer::get()) {
                const int index = layer->findById(id);
                if (index >= 0) layer->closeWindow(index);
            }
            if (action) action();
        };
        spec.rows.push_back(std::move(r));
    }
    s_instance->openExtra(std::move(spec), "");
}

void NXRTableLayer::openUiSettings(const std::string& origin) {
    if (!s_instance) return;
    Spec spec;
    spec.name = "UI Settings";
    spec.id = "panel:ui-settings";
    spec.kind = Kind::UiSettings;
    s_instance->openExtra(std::move(spec), origin);
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

void NXRTableLayer::onEnter() {
    CCLayer::onEnter();
    this->applyHostPriority();
}

void NXRTableLayer::registerWithTouchDispatcher() {
    CCTouchDispatcher::get()->addTargetedDelegate(this, kTablePriority, true);
}

void NXRTableLayer::applyHostPriority() {
    for (auto& w : m_wins) {
        if (w.panel() && w.node) geode::cocos::handleTouchPriorityWith(w.node, kHostPriority, true);
    }
}

void NXRTableLayer::update(float dt) {
    for (size_t i = 0; i < m_wins.size(); i++) {
        if (m_wins[i].popup && !m_wins[i].popup->getParent()) {
            this->closeWindow(static_cast<int>(i));
            break;
        }
    }
    m_refreshTimer += dt;
    if (m_refreshTimer >= 0.2f) {
        m_refreshTimer = 0.f;
        this->refreshRows();
        this->applyHostPriority();
    }
}

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
        r.label = "Open Menu Key / Font...";
        r.onTap = [] { NXRTableLayer::openUiSettings("Settings"); };
        rows.push_back(std::move(r));
    }
    {
        Row r;
        r.label = "Reset Window Positions";
        r.onTap = [] {
            auto* layer = NXRTableLayer::get();
            if (!layer) return;
            auto& config = NXRConfig::get();
            for (auto& w : layer->m_wins) {
                if (w.closable) continue;
                config.set<float>(key("x", w.id), -1.f);
                config.set<float>(key("y", w.id), -1.f);
                config.set<bool>(key("collapsed", w.id), false);
            }
            layer->rebuild(false);
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

    std::vector<Spec> pending;

    for (auto& window : NXR::Gui::get().getWindows()) {
        const std::string name = window.getName();
        if (name == "Settings") continue;

        const bool hasPanel = window.avaibleCustomWindowCocos();

        if (hasPanel) {
            Spec panel;
            panel.name = name;
            panel.id = "panel:" + name;
            panel.kind = Kind::BotPanel;
            panel.windowName = name;
            pending.push_back(std::move(panel));
        }

        Spec spec;
        spec.name = hasPanel ? name + " Hacks" : name;
        spec.id = spec.name;

        for (auto& hack : window.getHacks()) {
            Row r;
            r.label = hack.getName();
            r.hackId = hack.getID();
            r.desc = hack.getDesc();
            r.cheating = hack.isCheating();
            r.disabled = hack.getDisabled();
            if (hack.avaibleCustomWindowCocos()) {
                const std::string id = hack.getID();
                const std::string origin = spec.name;
                r.onSettings = [id, origin] {
                    if (auto* h = NXR::Gui::get().findHackByIDGlobal(id)) NXRTableLayer::openHackSettings(*h, origin);
                };
            }
            spec.rows.push_back(std::move(r));
        }

        if (!spec.rows.empty()) pending.push_back(std::move(spec));
    }

    {
        Spec spec;
        spec.name = "Settings";
        spec.id = "Settings";
        spec.rows = this->settingsRows();
        pending.push_back(std::move(spec));
    }

    for (auto& spec : pending) {
        this->buildWindow(std::move(spec));
    }

    auto extras = m_extras;
    for (auto& spec : extras) {
        this->buildWindow(spec);
    }

    this->reflow();
    this->applyHostPriority();
    this->refreshRows();
}

void NXRTableLayer::drawArrow(Win& w) {
    if (!w.arrow) return;
    w.arrow->clear();

    const ccColor4F color = {1.f, 1.f, 1.f, 0.95f};
    const float cx = w.width - 11.f;
    const float cy = -kTitleH / 2.f;

    if (w.collapsed) {
        CCPoint tri[3] = {{cx - 3.f, cy + 4.f}, {cx - 3.f, cy - 4.f}, {cx + 4.f, cy}};
        w.arrow->drawPolygon(tri, 3, color, 0.f, color);
    } else {
        CCPoint tri[3] = {{cx - 4.f, cy + 3.f}, {cx + 4.f, cy + 3.f}, {cx, cy - 4.f}};
        w.arrow->drawPolygon(tri, 3, color, 0.f, color);
    }
}

void NXRTableLayer::buildWindow(Spec spec) {
    auto& config = NXRConfig::get();
    const float scale = NXR::Ui::tableScale();
    const auto winSize = CCDirector::sharedDirector()->getWinSize();
    const float maxBody = std::max(kRowH * 3.f, (winSize.height - 12.f) / scale - kTitleH);

    m_wins.emplace_back();
    Win& w = m_wins.back();
    w.name = spec.name;
    w.id = spec.id;
    w.kind = spec.kind;
    w.hack = spec.hack;
    w.popup = spec.popup;
    w.closable = spec.closable;
    w.origin = spec.origin;

    w.node = CCNode::create();
    w.node->setScale(scale);
    this->addChild(w.node);

    float hostScale = 1.f;
    float hostW = 0.f;
    float hostH = 0.f;
    CCNode* hostNode = nullptr;
    geode::Ref<CCNode> hostKeep;

    if (w.panel()) {
        if (spec.kind == Kind::HackSettings && spec.hack) {
            auto* popup = NXRHackSettingsPopup::create(*spec.hack);
            if (popup) {
                w.owner = popup;
                geode::Ref<geode::ScrollLayer> keep(popup->m_scrollLayer);
                keep->removeFromParent();
                keep->setPosition({0.f, 0.f});
                keepTouchScroll(keep.data());
                w.scroll = keep.data();
                hostNode = keep.data();
                hostW = 240.f;
                hostH = 190.f;
                hostScale = 0.8f;
            }
        } else if (spec.kind == Kind::Embedded) {
            if (spec.popup) {
                auto* popup = spec.popup.data();
                geode::Ref<CCNode> keep(popup->m_mainLayer);
                popup->m_mainLayer->removeFromParent();
                popup->removeFromParent();
                popup->setTouchEnabled(false);
                popup->setKeypadEnabled(false);
                popup->setVisible(false);
                if (popup->m_closeBtn) popup->m_closeBtn->setVisible(false);
                const auto size = keep->getContentSize();
                keep->setAnchorPoint({0.f, 0.f});
                keep->setPosition({0.f, 0.f});
                auto* holder = CCNode::create();
                holder->addChild(keep);
                holder->addChild(popup);
                hostNode = holder;
                hostW = size.width;
                hostH = size.height;
                hostScale = 0.8f;
            }
        } else {
            auto* tab = NXRHacksTab::create();
            if (tab) {
                if (spec.kind == Kind::BotPanel) {
                    tab->addPadding(2.5f);
                    NXR::Gui::get().getWindow(spec.windowName).callCustomWindowCocos(tab);
                } else {
                    nxrBuildSettingsTab(tab);
                }
                tab->m_scrollLayer->m_contentLayer->updateLayout();
                tab->m_scrollLayer->setPosition({0.f, 0.f});
                tab->m_scrollLayer->moveToTop();
                keepTouchScroll(tab->m_scrollLayer);
                w.owner = tab;
                w.scroll = tab->m_scrollLayer;
                hostNode = tab;
                hostW = 360.f;
                hostH = 250.f;
                hostScale = 0.62f;
            }
        }

        if (!hostNode) {
            hostNode = CCNode::create();
            hostW = 120.f;
            hostH = 40.f;
            hostScale = 1.f;
        }

        hostKeep = hostNode;
        if (spec.kind == Kind::BotPanel || spec.kind == Kind::UiSettings) hostScale = kWideW / hostW;
        if (hostH * hostScale > maxBody) hostScale = maxBody / hostH;

        w.width = hostW * hostScale;
        w.contentH = hostH * hostScale;
        w.bodyH = w.contentH;
    } else {
        w.width = kWinW;
        w.rows = std::move(spec.rows);
        w.contentH = static_cast<float>(w.rows.size()) * kRowH;
        w.bodyH = std::min(w.contentH, maxBody);
    }

    w.collapsed = w.closable ? false : config.get<bool>(key("collapsed", w.id), true);

    auto accent = NXR::Theme::accent();

    auto* title = CCLayerColor::create(kTitleColor, w.width, kTitleH);
    title->setPosition({0.f, -kTitleH});
    w.node->addChild(title);

    auto* line = CCLayerColor::create(ccc4(accent.r, accent.g, accent.b, 255), w.width, 1.5f);
    line->setPosition({0.f, -kTitleH});
    w.node->addChild(line);

    auto* titleLabel = geode::Label::create(w.name, "GoogleSans.fnt"_spr);
    titleLabel->setAnchorPoint({0.5f, 0.5f});
    titleLabel->setScale(0.5f * NXR::Ui::fontScale());
    const float titleMax = w.width - (w.closable ? 52.f : 30.f);
    if (titleLabel->getScaledContentWidth() > titleMax) {
        titleLabel->setScale(titleLabel->getScale() * (titleMax / titleLabel->getScaledContentWidth()));
    }
    titleLabel->setPosition({(w.closable ? w.width / 2.f - 8.f : w.width / 2.f - 4.f), -kTitleH / 2.f});
    titleLabel->setColor({255, 255, 255});
    w.node->addChild(titleLabel);

    if (w.closable) {
        if (auto* close = fitSprite("NXR_tableClose.png"_spr, 9.f)) {
            close->setPosition({w.width - 30.f, -kTitleH / 2.f});
            w.node->addChild(close);
        }
    }

    w.arrow = CCDrawNode::create();
    w.node->addChild(w.arrow);
    this->drawArrow(w);

    w.body = CCLayerColor::create(kBodyColor, w.width, w.bodyH);
    w.body->setPosition({0.f, -kTitleH - w.bodyH});
    w.node->addChild(w.body);

    if (w.panel()) {
        w.host = hostNode;
        w.host->setScale(hostScale);
        w.host->setPosition({0.f, -kTitleH - w.bodyH});
        w.node->addChild(w.host);
        w.host->setVisible(!w.collapsed);
        if (w.scroll) {
            w.scroll->m_contentLayer->updateLayout();
            w.scroll->moveToTop();
        }
    } else {
        w.scroll = geode::ScrollLayer::create({w.width, w.bodyH});
        w.scroll->setPosition({0.f, -kTitleH - w.bodyH});
        disableScrollInput(w.scroll);
        w.scroll->m_contentLayer->setContentSize({w.width, w.contentH});
        w.scroll->m_contentLayer->setPositionY(w.bodyH - w.contentH);
        w.node->addChild(w.scroll);

        for (size_t i = 0; i < w.rows.size(); i++) {
            Row& r = w.rows[i];
            r.width = w.width;
            const float y = w.contentH - static_cast<float>(i + 1) * kRowH;
            const bool hackRow = !r.hackId.empty();

            if (!hackRow) {
                auto* bg = CCLayerColor::create(kButtonColor, w.width - 8.f, kRowH - 3.f);
                bg->setPosition({4.f, y + 1.5f});
                w.scroll->m_contentLayer->addChild(bg);
            }

            r.labelNode = geode::Label::create(r.text ? r.text() : r.label, "GoogleSans.fnt"_spr);
            if (hackRow) {
                r.labelNode->setAnchorPoint({0.f, 0.5f});
                r.labelNode->setPosition({32.f, y + kRowH / 2.f});
            } else {
                r.labelNode->setAnchorPoint({0.5f, 0.5f});
                r.labelNode->setPosition({w.width / 2.f, y + kRowH / 2.f});
            }
            r.labelNode->setScale(kLabelScale * NXR::Ui::fontScale());
            const float maxWidth = hackRow ? w.width - 32.f - (r.onSettings ? 24.f : 8.f) : w.width - 16.f;
            if (r.labelNode->getScaledContentWidth() > maxWidth) {
                r.labelNode->setScale(r.labelNode->getScale() * (maxWidth / r.labelNode->getScaledContentWidth()));
            }
            w.scroll->m_contentLayer->addChild(r.labelNode);

            if (hackRow) {
                r.checkOn = fitSprite("NXR_tableCheckOn.png"_spr, 22.f);
                r.checkOff = fitSprite("NXR_tableCheckOff.png"_spr, 22.f);
                for (auto* sprite : {r.checkOn, r.checkOff}) {
                    if (!sprite) continue;
                    sprite->setPosition({18.f, y + kRowH / 2.f});
                    w.scroll->m_contentLayer->addChild(sprite);
                }
            }

            if (r.onSettings) {
                if (auto* arrow = fitSprite("NXR_tableArrow.png"_spr, 13.f)) {
                    arrow->setPosition({w.width - 13.f, y + kRowH / 2.f});
                    w.scroll->m_contentLayer->addChild(arrow);
                }
            }
        }

        w.scroll->setVisible(!w.collapsed);
    }

    w.body->setVisible(!w.collapsed);

    if (w.closable) {
        w.pinned = spec.pinned;
        const float x = spec.x >= 0.f ? spec.x : kGap;
        const float y = spec.y >= 0.f ? spec.y : winSize.height - kGap;
        w.node->setPosition(this->clampTopLeft(w, {x, y}));
        return;
    }

    const float x = config.get<float>(key("x", w.id), -1.f);
    const float y = config.get<float>(key("y", w.id), -1.f);
    w.pinned = x >= 0.f && y >= 0.f;
    if (w.pinned) w.node->setPosition(this->clampTopLeft(w, {x, y}));
    else w.node->setPosition({kGap, winSize.height - kGap});
}

void NXRTableLayer::reflow() {
    const auto winSize = CCDirector::sharedDirector()->getWinSize();
    const float scale = NXR::Ui::tableScale();
    const float top = winSize.height - kGap;

    float colX = kGap;
    float curY = top;
    float colW = 0.f;
    for (auto& w : m_wins) {
        if (!w.node || w.closable || w.pinned) continue;
        const float ws = w.width * scale;
        const float hs = this->windowHeight(w) * scale;
        if (curY < top - 0.5f && curY - hs < kGap) {
            colX += colW + kGap;
            curY = top;
            colW = 0.f;
        }
        w.node->setPosition(this->clampTopLeft(w, {colX, curY}));
        curY -= hs + kGap;
        colW = std::max(colW, ws);
    }

    std::map<std::string, float> stack;
    for (auto& w : m_wins) {
        if (!w.node || !w.closable || w.pinned || w.origin.empty()) continue;
        const int oi = this->findById(w.origin);
        if (oi < 0) continue;
        const Win& o = m_wins[oi];
        const float ow = o.width * o.node->getScale();
        const float ew = w.width * w.node->getScale();
        float x = o.node->getPositionX() + ow + kGap;
        if (x + ew > winSize.width - 0.5f) x = o.node->getPositionX() - ew - kGap;
        auto it = stack.find(w.origin);
        const float y = it == stack.end() ? o.node->getPositionY() : it->second;
        w.node->setPosition(this->clampTopLeft(w, {x, y}));
        stack[w.origin] = w.node->getPositionY() - this->windowHeight(w) * w.node->getScale() - kGap;
    }
}

int NXRTableLayer::findById(const std::string& id) const {
    for (size_t i = 0; i < m_wins.size(); i++) {
        if (m_wins[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

void NXRTableLayer::closeWindow(int index) {
    if (index < 0 || index >= static_cast<int>(m_wins.size())) return;
    const std::string id = m_wins[index].id;
    if (m_wins[index].node) m_wins[index].node->removeFromParent();
    m_wins.erase(m_wins.begin() + index);
    m_extras.erase(std::remove_if(m_extras.begin(), m_extras.end(), [&](const Spec& s) { return s.id == id; }), m_extras.end());
    m_grab = Grab::None;
    m_active = -1;
    m_row = -1;
}

void NXRTableLayer::openExtra(Spec spec, const std::string& origin) {
    const int existing = this->findById(spec.id);
    if (existing >= 0) {
        this->closeWindow(existing);
        if (spec.kind != Kind::Embedded && spec.id.rfind("choice:", 0) != 0) return;
    }

    const float scale = NXR::Ui::tableScale();
    const auto winSize = CCDirector::sharedDirector()->getWinSize();
    const bool floating = spec.kind == Kind::Embedded || spec.id.rfind("choice:", 0) == 0;
    spec.closable = true;
    spec.origin = origin;
    spec.pinned = floating;
    spec.x = kGap;
    spec.y = winSize.height - kGap;

    m_extras.push_back(spec);
    this->buildWindow(spec);

    const int index = static_cast<int>(m_wins.size()) - 1;
    Win& w = m_wins[index];
    if (floating) {
        const float cx = (winSize.width - w.width * scale) / 2.f;
        const float cy = winSize.height / 2.f + this->windowHeight(w) * scale / 2.f;
        w.node->setPosition(this->clampTopLeft(w, {cx, cy}));
    }

    this->reflow();
    this->bringToFront(index);
    this->applyHostPriority();
    this->refreshRows();
}

void NXRTableLayer::refreshRow(Row& row) {
    if (!row.labelNode) return;

    const bool hackRow = !row.hackId.empty();
    bool enabled = true;
    if (hackRow) {
        enabled = false;
        if (auto* hack = NXR::Gui::get().findHackByIDGlobal(row.hackId)) enabled = hack->getEnabled();
    } else if (row.text) {
        const std::string text = row.text();
        if (text != row.label) {
            row.label = text;
            const float scale = kLabelScale * NXR::Ui::fontScale();
            row.labelNode->setString(text.c_str());
            row.labelNode->setScale(scale);
            const float maxWidth = row.width - 16.f;
            if (row.labelNode->getScaledContentWidth() > maxWidth) {
                row.labelNode->setScale(scale * (maxWidth / row.labelNode->getScaledContentWidth()));
            }
        }
    }

    if (hackRow) {
        row.labelNode->setColor(NXR::Ui::textColor(row.cheating ? kCheatColor : kOnColor));
    } else {
        row.labelNode->setColor(NXR::Ui::textColor(kOnColor));
    }
    if (row.disabled) row.labelNode->setOpacity(100);
    if (row.checkOn) row.checkOn->setVisible(enabled);
    if (row.checkOff) row.checkOff->setVisible(!enabled);
}

void NXRTableLayer::refreshRows() {
    for (auto& w : m_wins) {
        if (w.collapsed || w.panel()) continue;
        for (auto& row : w.rows) this->refreshRow(row);
    }
}

void NXRTableLayer::savePositions() {
    auto& config = NXRConfig::get();
    for (auto& w : m_wins) {
        if (!w.node) continue;
        if (w.closable) {
            for (auto& extra : m_extras) {
                if (extra.id == w.id) {
                    extra.x = w.node->getPositionX();
                    extra.y = w.node->getPositionY();
                    extra.pinned = w.pinned;
                }
            }
            continue;
        }
        if (w.pinned) {
            config.set<float>(key("x", w.id), w.node->getPositionX());
            config.set<float>(key("y", w.id), w.node->getPositionY());
        }
        config.set<bool>(key("collapsed", w.id), w.collapsed);
    }
}

float NXRTableLayer::windowHeight(const Win& w) const {
    return kTitleH + (w.collapsed ? 0.f : w.bodyH);
}

CCPoint NXRTableLayer::clampTopLeft(const Win& w, CCPoint p) const {
    const auto winSize = CCDirector::sharedDirector()->getWinSize();
    const float scale = w.node ? w.node->getScale() : 1.f;
    const float width = w.width * scale;
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

int NXRTableLayer::windowAt(const CCPoint& world) const {
    int best = -1;
    int bestZ = -1;
    for (size_t i = 0; i < m_wins.size(); i++) {
        const Win& w = m_wins[i];
        if (!w.node) continue;
        const auto p = w.node->convertToNodeSpace(world);
        if (p.x < 0.f || p.x > w.width) continue;
        if (p.y > 0.f || p.y < -windowHeight(w)) continue;
        if (w.node->getZOrder() >= bestZ) {
            bestZ = w.node->getZOrder();
            best = static_cast<int>(i);
        }
    }
    return best;
}

int NXRTableLayer::rowAt(Win& w, const CCPoint& world) const {
    if (w.collapsed || !w.scroll || w.panel()) return -1;
    const auto p = w.scroll->m_contentLayer->convertToNodeSpace(world);
    if (p.x < 0.f || p.x > w.width) return -1;
    const int index = static_cast<int>(std::floor((w.contentH - p.y) / kRowH));
    if (index < 0 || index >= static_cast<int>(w.rows.size())) return -1;
    return index;
}

void NXRTableLayer::scrollBy(Win& w, float dy, bool clamp) {
    if (!w.scroll || w.panel() || w.contentH <= w.bodyH) return;
    auto* content = w.scroll->m_contentLayer;
    const float minY = w.bodyH - w.contentH;
    const float slack = clamp ? 0.f : 12.f;
    content->setPositionY(std::clamp(content->getPositionY() + dy, minY - slack, slack));
}

bool NXRTableLayer::ccTouchBegan(CCTouch* touch, CCEvent*) {
    if (popupBlocking()) return false;

    const auto world = touch->getLocation();
    const int index = this->windowAt(world);
    if (index < 0) return false;

    Win& w = m_wins[index];
    this->bringToFront(index);

    const auto local = w.node->convertToNodeSpace(world);
    if (w.panel() && local.y <= -kTitleH) {
        this->applyHostPriority();
        return false;
    }

    m_active = index;
    m_moved = false;
    m_longPressed = false;
    m_start = world;
    m_lastLocal = local;
    m_row = -1;

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
    NXR::Ui::showPopup(NXRInfoPopup::create(row.label, row.desc), row.label);
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
        w.pinned = true;
        w.node->setPosition(this->clampTopLeft(w, this->convertToNodeSpace(world) + m_grabOffset));
    } else if (m_grab == Grab::Body) {
        const auto local = w.node->convertToNodeSpace(world);
        this->scrollBy(w, local.y - m_lastLocal.y, false);
        m_lastLocal = local;
    }
}

void NXRTableLayer::ccTouchEnded(CCTouch* touch, CCEvent*) {
    this->retain();
    this->unschedule(schedule_selector(NXRTableLayer::onLongPress));

    int closeIndex = -1;

    if (m_active >= 0 && m_active < static_cast<int>(m_wins.size())) {
        Win& w = m_wins[m_active];

        if (m_grab == Grab::Title) {
            if (!m_moved) {
                const auto startLocal = w.node->convertToNodeSpace(m_start);
                if (w.closable && startLocal.x >= w.width - 42.f && startLocal.x <= w.width - 20.f) {
                    closeIndex = m_active;
                } else {
                    w.collapsed = !w.collapsed;
                    w.body->setVisible(!w.collapsed);
                    if (w.scroll && !w.panel()) w.scroll->setVisible(!w.collapsed);
                    if (w.host) w.host->setVisible(!w.collapsed);
                    this->drawArrow(w);
                    w.node->setPosition(this->clampTopLeft(w, w.node->getPosition()));
                    this->reflow();
                    this->refreshRows();
                }
            }
            this->savePositions();
        } else if (m_grab == Grab::Body) {
            if (m_moved) {
                this->scrollBy(w, 0.f, true);
            } else if (!m_longPressed && m_row >= 0 && m_row < static_cast<int>(w.rows.size())) {
                Row& row = w.rows[m_row];
                const auto cp = w.scroll->m_contentLayer->convertToNodeSpace(touch->getLocation());
                const bool onCorner = row.onSettings && cp.x > w.width - 26.f;

                if (onCorner) {
                    auto fn = row.onSettings;
                    m_grab = Grab::None;
                    m_active = -1;
                    m_row = -1;
                    m_moved = false;
                    fn();
                    this->release();
                    return;
                } else if (!row.hackId.empty()) {
                    if (!row.disabled) {
                        if (auto* hack = NXR::Gui::get().findHackByIDGlobal(row.hackId)) hack->toggle();
                        this->refreshRow(row);
                    }
                } else if (row.onTap) {
                    auto fn = row.onTap;
                    m_grab = Grab::None;
                    m_active = -1;
                    m_row = -1;
                    m_moved = false;
                    fn();
                    if (NXRTableLayer::get() == this) this->refreshRows();
                    this->release();
                    return;
                }
            }
        }
    }

    if (closeIndex >= 0) this->closeWindow(closeIndex);

    m_grab = Grab::None;
    m_active = -1;
    m_row = -1;
    m_moved = false;
    this->release();
}

void NXRTableLayer::ccTouchCancelled(CCTouch*, CCEvent*) {
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
void NXRTableLayer::scrollWheel(float y, float x) {
    if (popupBlocking()) return;
    const int index = this->windowAt(geode::cocos::getMousePos());
    if (index < 0) return;
    Win& w = m_wins[index];
    if (w.collapsed) return;
    if (w.panel()) {
        if (w.scroll) w.scroll->scrollWheel(y, x);
        return;
    }
    this->scrollBy(w, y * kWheelDir, true);
}
#endif

$execute {
    NXR::Keybinds::get().registerAction("nxr.menu::toggle", "Open Menu", geode::Keybind(cocos2d::KEY_Tab, geode::KeyboardModifier::None), [](bool repeat) {
        if (!repeat) NXR::Ui::toggleMenu();
    });
}
