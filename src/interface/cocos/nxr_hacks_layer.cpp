#include "nxr_hacks_layer.hpp"
#include "nxr_menu.hpp"
#include "nxr_overlay_button.hpp"
#include "../../core/nxr_theme.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_keybinds.hpp"
#include "../../core/nxr_ui_mode.hpp"
#include "../../core/nxr_utils.hpp"
#include "../../core/nxr_bot.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace geode::prelude;
using namespace NXR::Kit;

namespace {
    constexpr const char* kOpenMenuBind = "nxr.menu::toggle";
    constexpr const char* kSourceUrl = "https://github.com/noxoraid/NoxoraUtils";

    std::vector<NXR::Window*> orderedWindows() {
        auto& windows = NXR::Gui::get().getWindows();
        static const char* preferred[] = {"Global", "Player", "Level", "Bot", "Utils", "Creator"};
        std::vector<NXR::Window*> out;
        for (const char* name : preferred) {
            for (auto& win : windows) {
                if (win.getName() == name) out.push_back(&win);
            }
        }
        for (auto& win : windows) {
            const std::string& name = win.getName();
            if (name == "Settings" || name == "About") continue;
            bool known = false;
            for (const char* p : preferred) {
                if (name == p) known = true;
            }
            if (!known) out.push_back(&win);
        }
        return out;
    }

    std::filesystem::path exportPath() {
        return getFolderMacroPath().parent_path() / "nxr_config_export.json";
    }

    void notify(const std::string& text, NotificationIcon icon) {
        geode::Notification::create(text, icon)->show();
    }

    std::string cleanMarkdown(std::string text) {
        auto strip = [&text](const std::string& token) {
            size_t pos = 0;
            while ((pos = text.find(token, pos)) != std::string::npos) text.erase(pos, token.size());
        };
        strip("**");
        strip("`");
        return text;
    }

    std::string trim(const std::string& text) {
        size_t a = 0;
        size_t b = text.size();
        while (a < b && (text[a] == ' ' || text[a] == '\t' || text[a] == '\r')) a++;
        while (b > a && (text[b - 1] == ' ' || text[b - 1] == '\t' || text[b - 1] == '\r')) b--;
        return text.substr(a, b - a);
    }

    std::string readChangelog() {
        std::ifstream in(geode::Mod::get()->getResourcesDir() / "changelog.md");
        if (!in.is_open()) return std::string();
        std::ostringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }

    void exportConfig() {
        auto& config = NXRConfig::get();
        config.save(getFileDataPath());
        const auto path = exportPath();
        config.save(path);
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) notify("Config exported", NotificationIcon::Success);
        else notify("Could not export config", NotificationIcon::Error);
    }

    void importConfig() {
        const auto path = exportPath();
        std::ifstream in(path);
        if (!in.is_open()) {
            notify("No exported config found", NotificationIcon::Warning);
            return;
        }
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        nlohmann::json json = nlohmann::json::parse(content, nullptr, false);
        if (json.is_discarded() || !json.is_object()) {
            notify("Exported config is not valid", NotificationIcon::Error);
            return;
        }

        auto& config = NXRConfig::get();
        std::vector<std::pair<std::string, bool>> before;
        for (auto& win : NXR::Gui::get().getWindows()) {
            for (auto& hack : win.getHacks()) before.emplace_back(hack.getID(), hack.getEnabled());
        }

        config.loadFromJson(json);

        for (auto& [id, was] : before) {
            auto* hack = NXR::Gui::get().findHackByIDGlobal(id);
            if (!hack) continue;
            const bool now = hack->getEnabled();
            if (now != was) {
                config.set<bool>(id, was);
                hack->setEnabled(now);
            }
        }

        config.save(getFileDataPath());
        notify("Config imported", NotificationIcon::Success);
        geode::queueInMainThread([] { NXR::Ui::reopenMenu(); });
    }

    void resetConfig() {
        auto& config = NXRConfig::get();
        for (auto& win : NXR::Gui::get().getWindows()) {
            for (auto& hack : win.getHacks()) {
                if (hack.getEnabled()) hack.disable();
            }
        }
        config.clear();
        config.save(getFileDataPath());
        notify("Config reset", NotificationIcon::Success);
        geode::queueInMainThread([] { NXR::Ui::reopenMenu(); });
    }

    class NXRSearchPopup : public geode::Popup {
    public:
        static NXRSearchPopup* create(const std::string& initial) {
            auto* ret = new NXRSearchPopup();
            if (ret->init(initial)) {
                ret->autorelease();
                return ret;
            }
            delete ret;
            return nullptr;
        }

    protected:
        bool init(const std::string& initial) {
            if (!geode::Popup::init(300.f, 120.f, NXR::Theme::square())) return false;
            auto size = m_mainLayer->getContentSize();

            auto titleLabel = geode::Label::create("Search hacks", "GoogleSans.fnt"_spr);
            titleLabel->setPosition({size.width / 2.f, size.height - 20.f});
            titleLabel->setScale(0.6f);
            m_mainLayer->addChild(titleLabel);

            auto closeSprite = cocos2d::CCSprite::create("NXR_closeBtn.png"_spr);
            closeSprite->setScale(0.75f);
            m_closeBtn->setSprite(closeSprite);

            auto input = geode::TextInput::create(220.f, "Type a hack name", "GoogleSans.fnt"_spr);
            if (auto* bg = input->getChildByType<geode::NineSlice>(0)) {
                bg->setColor({34, 33, 46});
                bg->setOpacity(255);
            }
            input->setPosition({size.width / 2.f, size.height / 2.f - 2.f});
            input->setMaxCharCount(24);
            input->setCallback([](const std::string& text) {
                NXRHacksLayer::applyQuery(text);
            });
            if (!initial.empty()) input->setString(initial);
            m_mainLayer->addChild(input);

            auto clearSprite = ButtonSprite::create("Clear", 80, true, "GoogleSans.fnt"_spr, NXR::Theme::button(), 24.f, 0.6f);
            auto clearButton = geode::cocos::CCMenuItemExt::createSpriteExtra(clearSprite, [input](CCMenuItemSpriteExtra*) {
                input->setString("");
                NXRHacksLayer::applyQuery("");
            });
            auto menu = NXRMenu::create();
            menu->setPosition({0.f, 0.f});
            clearButton->setPosition({size.width / 2.f, 24.f});
            menu->addChild(clearButton);
            m_mainLayer->addChild(menu);
            return true;
        }
    };
}

NXRHacksLayer* NXRHacksLayer::instance = nullptr;

NXRHacksLayer::~NXRHacksLayer() {
    *m_alive = false;
    if (instance == this) instance = nullptr;
}

NXRHacksLayer* NXRHacksLayer::create() {
    auto* ret = new NXRHacksLayer();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

NXRHacksLayer* NXRHacksLayer::get() {
    if (!instance) instance = create();
    return instance;
}

bool NXRHacksLayer::isOpened() {
    return instance != nullptr;
}

void NXRHacksLayer::applyQuery(const std::string& text) {
    if (instance) instance->setQuery(text);
}

void NXRHacksLayer::openHack(NXR::Hack& hack) {
    const bool wasOpen = isOpened();
    auto* layer = get();
    if (!layer) return;
    if (!wasOpen) {
        layer->setSheetOnly(true);
        layer->show();
    }
    layer->openHackSheet(hack);
}

void NXRHacksLayer::defer(std::function<void()> fn) {
    geode::queueInMainThread([fn = std::move(fn), alive = m_alive]() mutable {
        if (*alive && fn) fn();
    });
}

std::function<void()> NXRHacksLayer::guarded(std::function<void()> fn) {
    auto alive = m_alive;
    return [fn = std::move(fn), alive]() {
        if (*alive && fn) fn();
    };
}

bool NXRHacksLayer::init() {
    if (!CCLayer::init()) return false;

    this->setTouchEnabled(true);
    this->setKeypadEnabled(true);

    m_tab = NXRConfig::get().get<std::string>(NXR::Ui::kTabKey, "Global");
    m_tabBuilder = std::make_unique<PageBuilder>(this, [this] {
        this->defer([this] { this->refreshBody(true); });
    });

    buildUi();
    this->scheduleUpdate();
    return true;
}

void NXRHacksLayer::registerWithTouchDispatcher() {
    CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, -350, true);
}

void NXRHacksLayer::show() {
    if (this->getParent()) return;
    auto* scene = CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return;
    scene->addChild(this, 104);
}

void NXRHacksLayer::onClose(CCObject*) {
    if (m_closed) return;
    m_closed = true;
    *m_alive = false;

    auto& config = NXRConfig::get();
    config.set<std::string>(NXR::Ui::kTabKey, m_tab);
    config.save(getFileDataPath());

    if (instance == this) instance = nullptr;
    this->removeFromParentAndCleanup(true);
}

void NXRHacksLayer::closeLater() {
    defer([this] { this->onClose(nullptr); });
}

void NXRHacksLayer::keyBackClicked() {
    if (m_sheet && !m_sheetClosing) {
        if (m_pages.size() > 1) popPage();
        else closeSheet();
        return;
    }
    onClose(nullptr);
}

CCRect NXRHacksLayer::panelRect() const {
    return CCRect(m_px, m_py, m_pw, m_ph);
}

float NXRHacksLayer::sheetWidth() const {
    return std::min(dp(390.f), m_pw - dp(72.f) - dp(20.f));
}

CCPoint NXRHacksLayer::sheetTarget() const {
    return CCPoint(m_px + m_pw - sheetWidth(), m_py);
}

CCRect NXRHacksLayer::sheetRect() const {
    const CCPoint target = sheetTarget();
    return CCRect(target.x, target.y, sheetWidth(), m_ph);
}

int NXRHacksLayer::zoneAt(const std::vector<Zone>& zones, const CCPoint& p) const {
    for (size_t i = 0; i < zones.size(); i++) {
        if (zones[i].rect.containsPoint(p)) return static_cast<int>(i);
    }
    return -1;
}

void NXRHacksLayer::redrawBackground() {
    if (!m_bgDraw) return;
    m_bgDraw->clear();
    const float opacity = NXR::Ui::panelOpacity();
    const float radius = dp(18.f);
    const float rail = dp(72.f);
    drawRound(m_bgDraw, 0.f, 0.f, m_pw, m_ph, radius, fillColor(13, 18, 27, opacity));
    drawRound(m_bgDraw, 0.f, 0.f, rail, m_ph, radius, fillColor(8, 12, 20, opacity));
    drawRound(m_bgDraw, rail - radius, 0.f, radius, m_ph, 0.f, fillColor(8, 12, 20, opacity));
    drawRound(m_bgDraw, rail, dp(10.f), 1.f, m_ph - dp(20.f), 0.f, fillColor(32, 44, 64, opacity));
}

void NXRHacksLayer::rebuildUiLater() {
    defer([this] { this->buildUi(); });
}

void NXRHacksLayer::setSheetOnly(bool value) {
    m_sheetOnly = value;
    if (m_panel) m_panel->setVisible(!value);
}

void NXRHacksLayer::buildUi() {
    if (m_root) m_root->removeFromParentAndCleanup(true);
    m_root = nullptr;
    m_panel = nullptr;
    m_bgDraw = nullptr;
    m_header = nullptr;
    m_rail = nullptr;
    m_body = nullptr;
    m_sheet = nullptr;
    m_sheetContent = nullptr;
    m_sheetList = nullptr;
    m_sheetClosing = false;
    m_pages.clear();
    m_zones.clear();
    m_sheetZones.clear();
    m_touch = nullptr;
    m_target = Target::None;

    const CCSize win = CCDirector::sharedDirector()->getWinSize();
    m_unit = std::max(0.4f, win.height / 390.f) * NXR::Ui::panelScale();
    m_pw = std::min(win.width - 8.f, dp(844.f));
    m_ph = std::min(win.height - 8.f, dp(390.f));

    auto& config = NXRConfig::get();
    const float offX = static_cast<float>(config.get<int>(NXR::Ui::kPanelXKey, 0)) * m_unit;
    const float offY = static_cast<float>(config.get<int>(NXR::Ui::kPanelYKey, 0)) * m_unit;
    m_px = std::clamp((win.width - m_pw) * 0.5f + offX, 4.f, std::max(4.f, win.width - m_pw - 4.f));
    m_py = std::clamp((win.height - m_ph) * 0.5f + offY, 4.f, std::max(4.f, win.height - m_ph - 4.f));

    m_root = CCNode::create();
    this->addChild(m_root);

    m_panel = CCNode::create();
    m_panel->setPosition(CCPoint(m_px, m_py));
    m_root->addChild(m_panel);

    m_bgDraw = CCDrawNode::create();
    m_panel->addChild(m_bgDraw, -1);
    redrawBackground();

    const float rail = dp(72.f);
    const float headerH = dp(56.f);

    auto* logo = CCSprite::create("NXR_logo.png"_spr);
    if (logo) {
        const float base = std::max(logo->getContentWidth(), logo->getContentHeight());
        if (base > 0.f) logo->setScale(dp(34.f) / base);
        logo->setPosition(CCPoint(rail * 0.5f, m_ph - dp(30.f)));
        m_panel->addChild(logo, 2);
    }

    m_rail = PanelList::create(CCSize(rail, m_ph - dp(60.f)));
    m_rail->setPosition(CCPoint(0.f, 0.f));
    m_rail->setSlop(dp(8.f));
    m_panel->addChild(m_rail, 1);

    m_header = CCNode::create();
    m_header->setPosition(CCPoint(rail, m_ph - headerH));
    m_header->setContentSize(CCSize(m_pw - rail, headerH));
    m_panel->addChild(m_header, 2);

    m_body = PanelList::create(CCSize(m_pw - rail, m_ph - headerH));
    m_body->setPosition(CCPoint(rail, 0.f));
    m_body->setSlop(dp(8.f));
    m_panel->addChild(m_body, 1);

    const auto names = tabNames();
    if (std::find(names.begin(), names.end(), m_tab) == names.end()) m_tab = names.size() > 1 ? names[1] : names[0];

    buildRail();
    buildHeader();
    refreshBody(false);

    if (m_sheetOnly) m_panel->setVisible(false);
}

std::vector<std::string> NXRHacksLayer::tabNames() {
    std::vector<std::string> names;
    names.push_back("Favorites");
    for (auto* win : orderedWindows()) names.push_back(win->getName());
    names.push_back("Settings");
    names.push_back("About");
    return names;
}

void NXRHacksLayer::buildRail() {
    if (!m_rail) return;
    std::vector<ControlPtr> items;
    for (const auto& name : tabNames()) {
        Icon icon = iconForWindow(name);
        if (name == "Favorites") icon = Icon::StarOn;
        else if (name == "Settings") icon = Icon::Settings;
        else if (name == "About") icon = Icon::About;
        items.push_back(makeRailTab(this, name, icon, name == m_tab, guarded([this, name] { this->switchTab(name); })));
    }
    const float scroll = m_rail->getScroll();
    m_rail->setItems(std::move(items), dp(4.f), dp(8.f));
    m_rail->setScroll(scroll);
}

void NXRHacksLayer::switchTab(const std::string& name) {
    m_tab = name;
    m_query.clear();
    NXRConfig::get().set<std::string>(NXR::Ui::kTabKey, m_tab);
    buildRail();
    buildHeader();
    refreshBody(false);
}

void NXRHacksLayer::setQuery(const std::string& text) {
    m_query = text;
    buildHeader();
    refreshBody(false);
}

std::vector<std::string> NXRHacksLayer::statusChips() {
    std::vector<std::string> chips;
    const auto mode = NXR::Bot::State::get().mode;
    if (mode == NXR::Bot::Mode::Recording) chips.push_back("Bot: Record");
    else if (mode == NXR::Bot::Mode::Playing) chips.push_back("Bot: Playback");
    auto* safe = NXR::Gui::get().findHackByIDGlobal("nxr.global.safe_mode");
    if (safe && safe->getEnabled()) chips.push_back("Safe Mode ON");
    return chips;
}

void NXRHacksLayer::buildHeader() {
    if (!m_header) return;
    m_header->removeAllChildrenWithCleanup(true);
    m_zones.clear();

    const float rail = dp(72.f);
    const float width = m_pw - rail;
    const float height = dp(56.f);
    const float baseX = m_px + rail;
    const float baseY = m_py + m_ph - height;

    auto* line = CCDrawNode::create();
    drawRound(line, dp(8.f), 0.f, width - dp(16.f), 1.f, 0.f, fillColor(32, 44, 64));
    m_header->addChild(line);

    const float closeW = dp(52.f);
    auto* closeIcon = makeIcon(Icon::Close, dp(18.f), Pal::muted());
    closeIcon->setPosition(CCPoint(width - closeW * 0.5f, height * 0.5f));
    m_header->addChild(closeIcon, 2);
    m_zones.push_back({CCRect(baseX + width - closeW, baseY, closeW, height), [this] { this->closeLater(); }});

    float right = width - closeW - dp(4.f);
    const float left = dp(12.f);
    const auto chips = statusChips();
    m_lastChips = chips;

    for (const auto& text : chips) {
        auto* label = makeLabel(text, dp(13.f), Pal::accent(), CCPoint(0.5f, 0.5f));
        const float w = label->getScaledContentWidth() + dp(22.f);
        const float h = dp(26.f);
        if (right - w - dp(8.f) - left < dp(130.f)) break;
        auto* bg = CCDrawNode::create();
        drawRound(bg, right - w, height * 0.5f - h * 0.5f, w, h, h * 0.5f, fromColor(Pal::accent(), 0.16f));
        m_header->addChild(bg, 1);
        label->setPosition(CCPoint(right - w * 0.5f, height * 0.5f));
        m_header->addChild(label, 2);
        right -= w + dp(8.f);
    }

    const float pillW = std::max(dp(130.f), right - left - dp(4.f));
    const float pillH = dp(38.f);
    const float pillY = height * 0.5f - pillH * 0.5f;
    auto* pill = CCDrawNode::create();
    drawRound(pill, left, pillY, pillW, pillH, pillH * 0.5f, fillColor(24, 32, 47));
    m_header->addChild(pill, 1);

    auto* searchIcon = makeIcon(Icon::Search, dp(18.f), Pal::muted());
    searchIcon->setPosition(CCPoint(left + dp(20.f), height * 0.5f));
    m_header->addChild(searchIcon, 2);

    const bool hasQuery = !m_query.empty();
    auto* text = makeLabel(hasQuery ? m_query : std::string("Search hacks"), dp(16.f), hasQuery ? Pal::text() : Pal::muted(), CCPoint(0.f, 0.5f));
    text->setPosition(CCPoint(left + dp(38.f), height * 0.5f));
    fitLabel(text, pillW - dp(38.f) - (hasQuery ? dp(48.f) : dp(14.f)));
    m_header->addChild(text, 2);

    if (hasQuery) {
        auto* clear = makeIcon(Icon::Close, dp(14.f), Pal::muted());
        clear->setPosition(CCPoint(left + pillW - dp(22.f), height * 0.5f));
        m_header->addChild(clear, 2);
        m_zones.push_back({CCRect(baseX + left + pillW - dp(46.f), baseY, dp(46.f), height), [this] { this->setQuery(""); }});
        m_zones.push_back({CCRect(baseX + left, baseY, pillW - dp(46.f), height), [this] { NXR::Ui::showPopup(NXRSearchPopup::create(m_query), "Search"); }});
    } else {
        m_zones.push_back({CCRect(baseX + left, baseY, pillW, height), [this] { NXR::Ui::showPopup(NXRSearchPopup::create(m_query), "Search"); }});
    }
}

void NXRHacksLayer::refreshBody(bool keepScroll) {
    if (!m_body) return;
    const float scroll = keepScroll ? m_body->getScroll() : 0.f;
    m_body->setItems(tabItems(), dp(6.f), dp(16.f));
    if (keepScroll) m_body->setScroll(scroll);
}

std::vector<ControlPtr> NXRHacksLayer::tabItems() {
    m_tabBuilder->clear();
    auto& b = *m_tabBuilder;

    if (!m_query.empty()) {
        const std::string query = NXR::Utils::String::toLowerCase(m_query);
        bool any = false;
        for (auto* win : orderedWindows()) {
            std::vector<std::string> ids;
            for (auto& hack : win->getHacks()) {
                if (NXR::Utils::String::toLowerCase(hack.getName()).find(query) != std::string::npos) ids.push_back(hack.getID());
            }
            if (ids.empty()) continue;
            any = true;
            b.addSection(win->getName());
            for (const auto& id : ids) b.addControl(makeHackRow(this, id, ""));
        }
        if (!any) b.addControl(makeText(this, "No hacks match \"" + m_query + "\"", 15.f, true, false));
        return b.items();
    }

    if (m_tab == "Favorites") {
        bool any = false;
        for (const auto& id : NXR::Ui::favorites()) {
            std::string windowName;
            for (auto& win : NXR::Gui::get().getWindows()) {
                for (auto& hack : win.getHacks()) {
                    if (hack.getID() == id) windowName = win.getName();
                }
            }
            if (windowName.empty()) continue;
            any = true;
            b.addControl(makeHackRow(this, id, windowName));
        }
        if (!any) b.addControl(makeText(this, "Tap the star on any hack to pin it here.", 15.f, true, false));
        return b.items();
    }

    if (m_tab == "Settings") {
        fillSettings(b);
        return b.items();
    }

    if (m_tab == "About") {
        fillAbout(b);
        return b.items();
    }

    NXR::Window* target = nullptr;
    for (auto* win : orderedWindows()) {
        if (win->getName() == m_tab) target = win;
    }
    if (!target) return b.items();

    b.addPadding(2.f);
    if (target->avaibleCustomWindowCocos()) {
        target->callCustomWindowCocos(b);
        b.addSection("Hacks");
    }
    for (auto& hack : target->getHacks()) b.addControl(makeHackRow(this, hack.getID(), ""));
    return b.items();
}

void NXRHacksLayer::favoritesChanged() {
    defer([this] {
        if (m_tab == "Favorites" && m_query.empty()) this->refreshBody(true);
    });
}

void NXRHacksLayer::statusChanged() {
    m_chipTimer = 1.f;
}

void NXRHacksLayer::update(float dt) {
    m_chipTimer += dt;
    if (m_chipTimer < 0.3f) return;
    m_chipTimer = 0.f;
    if (m_closed || !m_header) return;
    if (statusChips() != m_lastChips) buildHeader();
}

void NXRHacksLayer::fillSettings(PageBuilder& b) {
    b.addSection("Interface");
    b.addControl(makeChoice(this, "Menu Layout", {"Panel", "Table"},
        [] { return NXR::Ui::layout(); },
        [](int index) {
            NXRConfig::get().set<int>(NXR::Ui::kLayoutKey, index);
            geode::queueInMainThread([] { NXR::Ui::reopenMenu(); });
            return index;
        }));

    SliderSpec size;
    size.label = "UI Size";
    size.key = NXR::Ui::kPanelScaleKey;
    size.suffix = "x";
    size.min = 0.9f;
    size.max = 1.2f;
    size.def = 1.f;
    size.step = 0.05f;
    size.presets = {{"0.9x", 0.9f}, {"1x", 1.f}, {"1.1x", 1.1f}, {"1.2x", 1.2f}};
    size.onCommit = [this](float) { this->rebuildUiLater(); };
    b.addControl(makeSlider(this, size));

    SliderSpec opacity;
    opacity.label = "Panel Opacity";
    opacity.key = NXR::Ui::kPanelOpacityKey;
    opacity.min = 0.35f;
    opacity.max = 1.f;
    opacity.def = 0.96f;
    opacity.step = 0.01f;
    opacity.callback = [this](float) { this->redrawBackground(); };
    b.addControl(makeSlider(this, opacity));

    SliderSpec posX;
    posX.label = "Panel X";
    posX.key = NXR::Ui::kPanelXKey;
    posX.min = -300.f;
    posX.max = 300.f;
    posX.def = 0.f;
    posX.step = 1.f;
    posX.integer = true;
    posX.onCommit = [this](float) { this->rebuildUiLater(); };
    b.addControl(makeSlider(this, posX));

    SliderSpec posY = posX;
    posY.label = "Panel Y";
    posY.key = NXR::Ui::kPanelYKey;
    b.addControl(makeSlider(this, posY));

    b.addButtons({{"Center Panel", [this] {
        NXRConfig::get().set<int>(NXR::Ui::kPanelXKey, 0);
        NXRConfig::get().set<int>(NXR::Ui::kPanelYKey, 0);
        this->rebuildUiLater();
    }}});

    b.addSection("Logo Button");
    SliderSpec logo;
    logo.label = "Logo Size";
    logo.min = 0.3f;
    logo.max = 1.f;
    logo.def = 0.5f;
    logo.step = 0.05f;
    logo.getter = [] { return NXRConfig::get().get<float>(NXR::Ui::kLogoScaleKey, 0.5f); };
    logo.setter = [](float value) { NXROverlayButton::get()->setSizeScale(value); };
    b.addControl(makeSlider(this, logo));

    SliderSpec logoX;
    logoX.label = "Logo X";
    logoX.suffix = "%";
    logoX.min = 0.f;
    logoX.max = 100.f;
    logoX.def = 10.f;
    logoX.step = 1.f;
    logoX.integer = true;
    logoX.getter = [] {
        const auto win = CCDirector::sharedDirector()->getWinSize();
        return NXROverlayButton::get()->getTarget().x / win.width * 100.f;
    };
    logoX.setter = [](float value) {
        const auto win = CCDirector::sharedDirector()->getWinSize();
        auto* button = NXROverlayButton::get();
        button->moveTo(win.width * value / 100.f, button->getTarget().y);
    };
    b.addControl(makeSlider(this, logoX));

    SliderSpec logoY = logoX;
    logoY.label = "Logo Y";
    logoY.getter = [] {
        const auto win = CCDirector::sharedDirector()->getWinSize();
        return NXROverlayButton::get()->getTarget().y / win.height * 100.f;
    };
    logoY.setter = [](float value) {
        const auto win = CCDirector::sharedDirector()->getWinSize();
        auto* button = NXROverlayButton::get();
        button->moveTo(button->getTarget().x, win.height * value / 100.f);
    };
    b.addControl(makeSlider(this, logoY));

    b.addButtons({
        {"Bottom Left", [] { NXROverlayButton::get()->moveToCorner(0); }},
        {"Bottom Right", [] { NXROverlayButton::get()->moveToCorner(1); }}
    });
    b.addButtons({
        {"Top Left", [] { NXROverlayButton::get()->moveToCorner(2); }},
        {"Top Right", [] { NXROverlayButton::get()->moveToCorner(3); }}
    });

    b.addToggle("Hide logo in levels",
        [] { return NXRConfig::get().get<bool>(NXR::Ui::kLogoHideGameKey, true); },
        [](bool value) { NXRConfig::get().set<bool>(NXR::Ui::kLogoHideGameKey, value); });
    b.addToggle("Hide logo in editor",
        [] { return NXRConfig::get().get<bool>(NXR::Ui::kLogoHideEditorKey, false); },
        [](bool value) { NXRConfig::get().set<bool>(NXR::Ui::kLogoHideEditorKey, value); });

    b.addSection("Controls");
    b.addKeybind("Open Menu Key",
        [] {
            auto& keybinds = NXR::Keybinds::get();
            if (keybinds.isRecordingCustom(kOpenMenuBind)) return std::string("Press a key...");
            const auto bind = keybinds.getBind(kOpenMenuBind);
            return bind.key == cocos2d::KEY_None ? std::string("None") : bind.toString();
        },
        [] { NXR::Keybinds::get().startRecordingCustom(kOpenMenuBind); },
        [] {
            NXR::Keybinds::get().stopRecording();
            NXR::Keybinds::get().clearCustomBind(kOpenMenuBind);
        });
    b.addLink("All Keybinds", nullptr, [this] { this->openKeybinds(); });

    b.addSection("Theme");
    b.addControl(makeChoice(this, "Popup Theme", {"Basic", "Normal", "Medium", "Pro"},
        [] { return NXR::Theme::current() - 1; },
        [](int index) {
            NXRConfig::get().set<int>(NXR::Theme::kKey, index + 1);
            return index;
        }));

    b.addSection("Config");
    b.addButtons({
        {"Export Config", [] { exportConfig(); }},
        {"Import Config", [] { importConfig(); }}
    });
    b.addText("Export writes nxr_config_export.json next to the macro folder. Import reads the same file.");
    b.addButtons({{"Reset Config", [] {
        NXR::Ui::showChoice("Reset config", {"All NXR settings go back to default.", "Every hack is turned off."}, {
            {"Cancel", [] {}},
            {"Reset", [] { resetConfig(); }}
        });
    }}}, true);
    b.addPadding(8.f);
}

void NXRHacksLayer::fillAbout(PageBuilder& b) {
    b.addSection("NoxoraUtils");
    b.addControl(makeText(this, fmt::format("Version {}", geode::Mod::get()->getVersion().toVString()), 17.f, false, true));
    b.addText("Macro bot, noclip, hitboxes, trajectory and editor tools for Geometry Dash.");

    b.addSection("Links");
    b.addLink("Source code", [] { return std::string(kSourceUrl); }, [] {
        CCApplication::sharedApplication()->openURL(kSourceUrl);
    });

    b.addSection("Build");
    size_t total = 0;
    for (auto& win : NXR::Gui::get().getWindows()) total += win.getHacks().size();
    b.addText(fmt::format("Geode {}\nGeometry Dash {}\nPlatform {}\nBuilt {} {}\nHacks loaded {}",
        geode::Loader::get()->getVersion().toVString(), GEODE_GD_VERSION_STRING, GEODE_PLATFORM_NAME, __DATE__, __TIME__, total));

    b.addSection("Changelog");
    const std::string changelog = readChangelog();
    if (changelog.empty()) {
        b.addText("changelog.md was not found in the bundled resources.");
    } else {
        std::istringstream stream(changelog);
        std::string line;
        while (std::getline(stream, line)) {
            line = trim(line);
            if (line.empty()) continue;
            if (line.rfind("# ", 0) == 0) {
                b.addSection(cleanMarkdown(line.substr(2)));
            } else if (line.rfind("- ", 0) == 0) {
                b.addText(std::string("- ") + cleanMarkdown(line.substr(2)));
            } else {
                b.addText(cleanMarkdown(line));
            }
        }
    }
    b.addPadding(8.f);
}

void NXRHacksLayer::fillKeybinds(PageBuilder& b) {
    auto& keybinds = NXR::Keybinds::get();
    b.addText("Tap a key button, then press a key. That key toggles the feature without opening the menu. Esc or Clear removes it.");

    for (auto& window : NXR::Gui::get().getWindows()) {
        if (window.getHacks().empty()) continue;
        const std::string windowName = window.getName();
        b.addSection(windowName);

        for (auto& hack : window.getHacks()) {
            const std::string hackName = hack.getName();
            const std::string hackId = hack.getID();
            b.addKeybind(hackName,
                [windowName, hackName, hackId] {
                    auto& kb = NXR::Keybinds::get();
                    if (kb.isRecording(windowName, hackName)) return std::string("Press a key...");
                    auto* target = NXR::Gui::get().findHackByIDGlobal(hackId);
                    if (!target) return std::string("None");
                    const auto bind = target->getKeybind();
                    return bind.key == cocos2d::KEY_None ? std::string("None") : bind.toString();
                },
                [windowName, hackName] { NXR::Keybinds::get().startRecording(windowName, hackName); },
                [windowName, hackName] {
                    NXR::Keybinds::get().stopRecording();
                    NXR::Keybinds::get().clearHackBind(windowName, hackName);
                });
        }
    }

#ifdef GEODE_IS_DESKTOP
    const auto actions = keybinds.customActions();
    if (!actions.empty()) {
        b.addSection("Actions");
        for (const auto& [id, label] : actions) {
            const std::string actionId = id;
            b.addKeybind(label,
                [actionId] {
                    auto& kb = NXR::Keybinds::get();
                    if (kb.isRecordingCustom(actionId)) return std::string("Press a key...");
                    const auto bind = kb.getBind(actionId);
                    return bind.key == cocos2d::KEY_None ? std::string("None") : bind.toString();
                },
                [actionId] { NXR::Keybinds::get().startRecordingCustom(actionId); },
                [actionId] {
                    NXR::Keybinds::get().stopRecording();
                    NXR::Keybinds::get().clearCustomBind(actionId);
                });
        }
    }
#else
    (void)keybinds;
#endif
    b.addPadding(8.f);
}

std::shared_ptr<NXRHacksLayer::Page> NXRHacksLayer::makeHackPage(NXR::Hack& hack, bool withForm) {
    auto page = std::make_shared<Page>();
    page->id = m_nextPageId++;
    page->title = hack.getName();
    page->hackId = hack.getID();
    page->hack = &hack;
    const int id = page->id;
    page->builder = std::make_unique<PageBuilder>(this, [this, id] {
        this->defer([this, id] { this->rebuildPage(id); });
    });
    page->build = [this, withForm](Page& pg) {
        NXR::Hack* hack = NXR::Gui::get().findHackByIDGlobal(pg.hackId);
        const bool listed = hack != nullptr;
        if (!hack) hack = pg.hack;
        pg.builder->clear();
        if (!hack) return pg.builder->items();

        if (listed && !hack->getDisabled()) {
            const std::string hackId = pg.hackId;
            pg.builder->addToggle("Enabled",
                [hackId] {
                    auto* target = NXR::Gui::get().findHackByIDGlobal(hackId);
                    return target && target->getEnabled();
                },
                [hackId](bool value) {
                    if (auto* target = NXR::Gui::get().findHackByIDGlobal(hackId)) target->setEnabled(value);
                });
        }

        if (!hack->getDesc().empty()) pg.builder->addControl(makeText(this, hack->getDesc(), 14.f, false, false));
        if (withForm && hack->hasForm()) {
            pg.builder->addSeparator();
            hack->callForm(*pg.builder);
        }
        return pg.builder->items();
    };
    return page;
}

void NXRHacksLayer::openHackSheet(NXR::Hack& hack) {
    NXR::Hack* ptr = &hack;
    defer([this, ptr] {
        this->m_pages.clear();
        this->pushPage(this->makeHackPage(*ptr, true));
    });
}

void NXRHacksLayer::openInfoSheet(NXR::Hack& hack) {
    NXR::Hack* ptr = &hack;
    defer([this, ptr] {
        this->m_pages.clear();
        this->pushPage(this->makeHackPage(*ptr, false));
    });
}

void NXRHacksLayer::openKeybinds() {
    defer([this] {
        auto page = std::make_shared<Page>();
        page->id = m_nextPageId++;
        page->title = "Keybinds";
        const int id = page->id;
        page->builder = std::make_unique<PageBuilder>(this, [this, id] {
            this->defer([this, id] { this->rebuildPage(id); });
        });
        page->build = [this](Page& pg) {
            pg.builder->clear();
            this->fillKeybinds(*pg.builder);
            return pg.builder->items();
        };
        this->m_pages.clear();
        this->pushPage(page);
    });
}

void NXRHacksLayer::openColorPicker(const ColorSpec& spec, std::function<void()> onDone) {
    defer([this, spec, onDone = std::move(onDone)]() mutable {
        auto page = std::make_shared<Page>();
        page->id = m_nextPageId++;
        page->title = spec.label;
        auto state = std::make_shared<ColorState>();
        loadColorState(*state, spec);
        page->build = [this, state, spec](Page&) {
            return makePickerItems(this, state, spec.defaultHex, !spec.rainbowKey.empty());
        };
        page->footer.emplace_back("Cancel", this->guarded([this] { this->popPage(); }));
        page->footer.emplace_back("Apply", this->guarded([this, state, spec, onDone] {
            saveColorState(*state, spec);
            if (onDone) onDone();
            this->popPage();
        }));
        this->pushPage(page);
    });
}

void NXRHacksLayer::openNumberInput(const std::string& title, const std::string& hint, const std::string& current, NumberKind kind, std::function<void(const std::string&)> onApply) {
    defer([this, title, hint, current, kind, onApply = std::move(onApply)]() mutable {
        auto page = std::make_shared<Page>();
        page->id = m_nextPageId++;
        page->title = title;
        auto text = std::make_shared<std::string>(current);
        page->build = [this, kind, text, hint](Page&) {
            std::vector<ControlPtr> items;
            if (!hint.empty()) items.push_back(makeText(this, hint, 14.f, true, false));
            items.push_back(makeKeypad(this, kind, text));
            return items;
        };
        page->footer.emplace_back("Cancel", this->guarded([this] { this->popPage(); }));
        page->footer.emplace_back("Apply", this->guarded([this, text, onApply] {
            const std::string value = *text;
            if (onApply && !value.empty()) onApply(value);
            this->popPage();
        }));
        this->pushPage(page);
    });
}

void NXRHacksLayer::pushPage(std::shared_ptr<Page> page) {
    if (m_sheetClosing) {
        if (m_sheet) {
            m_sheet->stopAllActions();
            m_sheet->removeFromParentAndCleanup(true);
        }
        m_sheet = nullptr;
        m_sheetContent = nullptr;
        m_sheetList = nullptr;
        m_sheetClosing = false;
    }

    if (m_sheetList && !m_pages.empty()) m_pages.back()->scroll = m_sheetList->getScroll();
    m_pages.push_back(std::move(page));

    bool fresh = false;
    if (!m_sheet) {
        fresh = true;
        m_sheet = CCNode::create();
        m_root->addChild(m_sheet, 10);

        const float sw = sheetWidth();
        auto* bg = CCDrawNode::create();
        drawRound(bg, 0.f, 0.f, sw, m_ph, dp(18.f), fillColor(16, 22, 34));
        drawRound(bg, 0.f, dp(14.f), dp(1.5f), m_ph - dp(28.f), 0.f, fromColor(Pal::accent(), 0.45f));
        m_sheet->addChild(bg, -2);

        if (!m_sheetOnly) {
            auto* scrim = CCDrawNode::create();
            drawRound(scrim, -(m_pw - sw), 0.f, m_pw - sw, m_ph, 0.f, fillColor(0, 0, 0, 0.38f));
            m_sheet->addChild(scrim, -3);
        }

        const CCPoint target = sheetTarget();
        m_sheet->setPosition(CCPoint(target.x + sw + dp(24.f), target.y));
        m_sheet->runAction(CCEaseOut::create(CCMoveTo::create(0.2f, target), 2.f));
    }

    renderSheet();
    (void)fresh;
}

void NXRHacksLayer::popPage() {
    if (!m_sheet) return;
    if (m_pages.size() <= 1) {
        closeSheet();
        return;
    }
    m_pages.pop_back();
    renderSheet();
}

void NXRHacksLayer::rebuildPage(int id) {
    if (m_pages.empty() || m_pages.back()->id != id) return;
    if (m_sheetList) m_pages.back()->scroll = m_sheetList->getScroll();
    renderSheet();
}

void NXRHacksLayer::renderSheet() {
    if (!m_sheet || m_pages.empty()) return;
    auto page = m_pages.back();

    if (m_sheetContent) m_sheetContent->removeFromParentAndCleanup(true);
    m_sheetContent = nullptr;
    m_sheetList = nullptr;
    m_sheetZones.clear();

    m_sheetContent = CCNode::create();
    m_sheet->addChild(m_sheetContent, 1);

    const float sw = sheetWidth();
    const float headerH = dp(54.f);
    const bool hasFooter = !page->footer.empty();
    const float footerH = hasFooter ? dp(68.f) : 0.f;
    const CCPoint target = sheetTarget();

    auto* head = CCDrawNode::create();
    drawRound(head, dp(10.f), m_ph - headerH, sw - dp(20.f), 1.f, 0.f, fillColor(36, 48, 68));
    m_sheetContent->addChild(head);

    float titleX = dp(18.f);
    if (m_pages.size() > 1) {
        auto* back = makeIcon(Icon::Back, dp(18.f), Pal::text());
        back->setPosition(CCPoint(dp(26.f), m_ph - headerH * 0.5f));
        m_sheetContent->addChild(back, 2);
        m_sheetZones.push_back({CCRect(target.x, target.y + m_ph - headerH, dp(52.f), headerH), guarded([this] { this->popPage(); })});
        titleX = dp(52.f);
    }

    auto* title = makeLabel(page->title, dp(20.f), Pal::text(), CCPoint(0.f, 0.5f));
    title->setPosition(CCPoint(titleX, m_ph - headerH * 0.5f));
    fitLabel(title, sw - titleX - dp(60.f));
    m_sheetContent->addChild(title, 2);

    auto* close = makeIcon(Icon::Close, dp(18.f), Pal::muted());
    close->setPosition(CCPoint(sw - dp(26.f), m_ph - headerH * 0.5f));
    m_sheetContent->addChild(close, 2);
    m_sheetZones.push_back({CCRect(target.x + sw - dp(52.f), target.y + m_ph - headerH, dp(52.f), headerH), guarded([this] { this->closeSheet(); })});

    m_sheetList = PanelList::create(CCSize(sw, m_ph - headerH - footerH));
    m_sheetList->setPosition(CCPoint(0.f, footerH));
    m_sheetList->setSlop(dp(8.f));
    m_sheetContent->addChild(m_sheetList);
    m_sheetList->setItems(page->build(*page), dp(6.f), dp(16.f));
    m_sheetList->setScroll(page->scroll);

    if (hasFooter) {
        auto* footer = CCDrawNode::create();
        drawRound(footer, dp(10.f), footerH - 1.f, sw - dp(20.f), 1.f, 0.f, fillColor(36, 48, 68));
        m_sheetContent->addChild(footer);

        const float gap = dp(10.f);
        const float count = static_cast<float>(page->footer.size());
        const float bw = (sw - dp(24.f) - gap * (count - 1.f)) / count;
        for (size_t i = 0; i < page->footer.size(); i++) {
            const float x = dp(12.f) + (bw + gap) * static_cast<float>(i);
            const bool primary = i + 1 == page->footer.size();
            auto* button = CCDrawNode::create();
            drawRound(button, x, dp(10.f), bw, dp(48.f), dp(12.f), primary ? fromColor(Pal::accent()) : fillColor(30, 41, 60));
            m_sheetContent->addChild(button);
            auto* label = makeLabel(page->footer[i].first, dp(17.f), primary ? Pal::onAccent() : Pal::text(), CCPoint(0.5f, 0.5f));
            label->setPosition(CCPoint(x + bw * 0.5f, dp(34.f)));
            fitLabel(label, bw - dp(12.f));
            m_sheetContent->addChild(label, 2);
            m_sheetZones.push_back({CCRect(target.x + x, target.y + dp(8.f), bw, dp(52.f)), page->footer[i].second});
        }
    }
}

void NXRHacksLayer::closeSheet() {
    if (!m_sheet || m_sheetClosing) return;
    m_sheetClosing = true;
    m_pages.clear();
    m_sheetList = nullptr;
    m_sheetZones.clear();
    m_touch = nullptr;
    m_target = Target::None;

    const CCPoint target = sheetTarget();
    m_sheet->stopAllActions();
    m_sheet->runAction(CCSequence::create(
        CCEaseIn::create(CCMoveTo::create(0.16f, CCPoint(target.x + sheetWidth() + dp(24.f), target.y)), 2.f),
        CCCallFunc::create(this, callfunc_selector(NXRHacksLayer::finishSheetClose)),
        nullptr
    ));
}

void NXRHacksLayer::finishSheetClose() {
    defer([this] {
        if (!m_sheetClosing) return;
        m_sheetClosing = false;
        if (m_sheet) m_sheet->removeFromParentAndCleanup(true);
        m_sheet = nullptr;
        m_sheetContent = nullptr;
        m_sheetList = nullptr;
        if (m_sheetOnly) {
            this->onClose(nullptr);
            return;
        }
        this->refreshBody(true);
    });
}

bool NXRHacksLayer::ccTouchBegan(CCTouch* touch, CCEvent*) {
    if (m_touch) return true;
    m_touch = touch;
    m_target = Target::None;
    m_moved = false;
    m_zoneIndex = -1;
    m_zoneInSheet = false;

    const CCPoint pt = touch->getLocation();
    m_start = pt;

    const bool inSheet = m_sheet && !m_sheetClosing && sheetRect().containsPoint(pt);
    const bool inPanel = !m_sheetOnly && panelRect().containsPoint(pt);

    if (!inSheet && !inPanel) {
        m_target = Target::Outside;
        return true;
    }

    if (inSheet) {
        const int zone = zoneAt(m_sheetZones, pt);
        if (zone >= 0) {
            m_target = Target::Zone;
            m_zoneIndex = zone;
            m_zoneInSheet = true;
            return true;
        }
        if (m_sheetList && m_sheetList->containsWorld(pt)) {
            m_target = Target::Sheet;
            m_sheetList->onTouchBegan(pt);
        }
        return true;
    }

    if (m_sheet) {
        m_target = Target::Scrim;
        return true;
    }

    const int zone = zoneAt(m_zones, pt);
    if (zone >= 0) {
        m_target = Target::Zone;
        m_zoneIndex = zone;
        return true;
    }

    if (m_rail && m_rail->containsWorld(pt)) {
        m_target = Target::Rail;
        m_rail->onTouchBegan(pt);
    } else if (m_body && m_body->containsWorld(pt)) {
        m_target = Target::Body;
        m_body->onTouchBegan(pt);
    }
    return true;
}

void NXRHacksLayer::ccTouchMoved(CCTouch* touch, CCEvent*) {
    if (touch != m_touch) return;
    const CCPoint pt = touch->getLocation();
    if (ccpDistance(pt, m_start) > dp(8.f)) m_moved = true;

    if (m_target == Target::Rail && m_rail) m_rail->onTouchMoved(pt);
    else if (m_target == Target::Body && m_body) m_body->onTouchMoved(pt);
    else if (m_target == Target::Sheet && m_sheetList) m_sheetList->onTouchMoved(pt);
}

void NXRHacksLayer::ccTouchEnded(CCTouch* touch, CCEvent*) {
    if (touch != m_touch) return;
    m_touch = nullptr;
    const CCPoint pt = touch->getLocation();
    const Target target = m_target;
    m_target = Target::None;

    switch (target) {
        case Target::Outside:
            if (!m_moved) closeLater();
            break;
        case Target::Scrim:
            if (!m_moved) closeSheet();
            break;
        case Target::Zone: {
            const auto& zones = m_zoneInSheet ? m_sheetZones : m_zones;
            if (!m_moved && m_zoneIndex >= 0 && m_zoneIndex < static_cast<int>(zones.size()) && zones[static_cast<size_t>(m_zoneIndex)].rect.containsPoint(pt)) {
                auto action = zones[static_cast<size_t>(m_zoneIndex)].action;
                if (action) action();
            }
            break;
        }
        case Target::Rail:
            if (m_rail) m_rail->onTouchEnded(pt);
            break;
        case Target::Body:
            if (m_body) m_body->onTouchEnded(pt);
            break;
        case Target::Sheet:
            if (m_sheetList) m_sheetList->onTouchEnded(pt);
            break;
        default:
            break;
    }
}

void NXRHacksLayer::ccTouchCancelled(CCTouch* touch, CCEvent*) {
    if (touch != m_touch) return;
    m_touch = nullptr;
    const Target target = m_target;
    m_target = Target::None;

    if (target == Target::Rail && m_rail) m_rail->onTouchCancelled();
    else if (target == Target::Body && m_body) m_body->onTouchCancelled();
    else if (target == Target::Sheet && m_sheetList) m_sheetList->onTouchCancelled();
}
