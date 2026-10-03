#include "../../core/nxr_theme.hpp"
#include "nxr_menu.hpp"
#include "nxr_hacks_layer.hpp"
#include <Geode/ui/Scrollbar.hpp>
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_gui.hpp"
#include "nxr_text_style.hpp"
#include "../../core/nxr_ui_mode.hpp"
#include "../../core/nxr_keybinds.hpp"

namespace {
    constexpr const char* kOpenMenuBind = "nxr.menu::toggle";
}

void nxrBuildKeybindTab(NXRHacksTab* tab) {
    auto& keybinds = NXR::Keybinds::get();

    tab->addPadding(4.f);
    tab->addText("Tap a key button, then press a key. That key toggles the feature without opening the menu. Esc or Clear removes it", 0.42f);

    for (auto& window : NXR::Gui::get().getWindows()) {
        if (window.getHacks().empty()) continue;
        const std::string windowName = window.getName();

        tab->addSeparator();
        tab->addText(windowName, 0.6f);

        for (auto& hack : window.getHacks()) {
            const std::string hackName = hack.getName();
            tab->addKeybindRow(hackName,
                [windowName, hackName] {
                    auto& kb = NXR::Keybinds::get();
                    if (kb.isRecording(windowName, hackName)) return std::string("Press a key...");
                    const auto bind = NXR::Gui::get().getWindow(windowName).findHackByName(hackName).getKeybind();
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
        tab->addSeparator();
        tab->addText("Actions", 0.6f);
        for (const auto& [id, label] : actions) {
            const std::string actionId = id;
            tab->addKeybindRow(label,
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
#endif
    tab->addPadding(4.f);
}

void nxrBuildSettingsTab(NXRHacksTab* tab) {
    tab->addPadding(4.f);
    tab->addText("Menu Layout", 0.6f);
    tab->addRadioRow({"Panel", "Table"},
        [] { return NXR::Ui::layout(); },
        [](int index) {
            NXRConfig::get().set<int>(NXR::Ui::kLayoutKey, index);
            geode::queueInMainThread([] { NXR::Ui::reopenMenu(); });
            return index;
        });
    tab->addText("Panel: tabs in one popup. Table: draggable ImGui windows", 0.42f);
    tab->addSeparator();
    tab->addText("Open Menu Key", 0.6f);
    tab->addKeybindRow("Open Menu",
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
    tab->addText("Tap the key button, then press a key. Esc or Clear removes it", 0.42f);
    tab->addSeparator();
    tab->addText("Theme", 0.6f);
    tab->addRadioRow({"Basic", "Normal", "Medium", "Pro"},
        [] { return NXR::Theme::current() - 1; },
        [](int index) {
            NXRConfig::get().set<int>(NXR::Theme::kKey, index + 1);
            return index;
        });
    tab->addSeparator();
    tab->addText("Toggle Style", 0.6f);
    tab->addRadioRow({"Switch", "Check"},
        [] { return NXR::Ui::toggleStyle(); },
        [](int index) {
            NXRConfig::get().set<int>(NXR::Ui::kToggleStyleKey, index);
            geode::queueInMainThread([] { NXR::Ui::reopenMenu(); });
            return index;
        });
    tab->addText("Switch: slider toggle. Check: checkbox on every toggle", 0.42f);
    tab->addSeparator();
    tab->addText("Font", 0.6f);
    tab->addConfigFloatInput("Font Size (0.6 - 1.6)", NXR::Ui::kFontScaleKey, 1.f, 0.6f, 1.6f);
    tab->addConfigToggle("Custom Font Color", NXR::Ui::kFontColorOnKey, true);
    tab->addConfigColor3Hex("Font Color", NXR::Ui::kFontColorKey, "FFFFFF");
    tab->addSeparator();
    tab->addText("Table Layout (ImGui)", 0.6f);
    tab->addConfigFloatInput("Table Scale (0.6 - 1.6)", NXR::Ui::kTableScaleKey, 1.f, 0.6f, 1.6f);
    tab->addSeparator();
    tab->addConfigButton("Apply (reopen menu)", [] { NXR::Ui::reopenMenu(); });
    tab->addText("Changes show after the menu is reopened", 0.42f);
    tab->addPadding(4.f);
}

NXRHacksLayer* NXRHacksLayer::instance = nullptr;

NXRHacksLayer::~NXRHacksLayer() {
    instance = nullptr;
}

bool NXRHacksLayer::init() {
    if (!Popup::init(460.f, 260.f, NXR::Theme::square()))
        return false;

    auto& gui = NXR::Gui::get();
    auto& config = NXRConfig::get();

    m_index = config.get<int>("nxr.gui_mobile.index", 1);
    m_lastIndexScroll = config.get<int>("nxr.gui_mobile.lastIndexScroll", -1);

    auto& windows = gui.getWindows();
    int shownWindows = 0;
    for (auto& win : windows) {
        if (win.getName() != "Settings") shownWindows++;
    }
    m_index = std::clamp(m_index, 0, shownWindows + 1);

    m_closeBtn->setVisible(false);

    auto version = geode::Label::create(geode::Mod::get()->getVersion().toVString(), "GoogleSans.fnt"_spr);
    version->setAnchorPoint({1.f, 0.f});
    version->setScale(0.5f);
    version->setPosition({CCDirector::get()->getScreenRight() - 6.f, 10.f});
    version->setOpacity(0);
    version->runAction(CCFadeTo::create(0.15f, 175));
    addChild(version);

    auto logo = cocos2d::CCSprite::create("NXR_logo.png"_spr);
    logo->setAnchorPoint({1.f, 0.f});
    logo->setScale(0.3f);
    logo->setPosition({CCDirector::get()->getScreenRight() - 6.f - version->getScaledContentWidth(), 6.f});
    logo->setOpacity(0);
    logo->runAction(CCFadeTo::create(0.15f, 200));
    addChild(logo);

    auto panel = geode::NineSlice::create(NXR::Theme::panel());
    panel->setAnchorPoint({0, 0});
    panel->setColor({255, 255, 255});
    panel->setPosition({5.f, 5.f});
    panel->setContentSize({100.f, 250.f});
    m_mainLayer->addChild(panel);

    m_tabsScrollLayer = geode::ScrollLayer::create({200.f, 250.f});
    m_tabsScrollLayer->setPosition({-95.f, 5.f});

    auto tabsMenu = NXRMenu::create();
    tabsMenu->setAnchorPoint({0.f, 0.f});
    tabsMenu->setContentSize({96.f, 0.f});

    int i = 0;
    for (auto& win : windows) {
        auto winName = win.getName();
        if (winName == "Settings")
            continue;

        auto button = ButtonSprite::create(winName.c_str(), 90, true, "GoogleSans.fnt"_spr, (i == m_index) ? NXR::Theme::buttonOn() : NXR::Theme::button(), 30.f, 0.7f);
        button->m_label->setColor(NXR::Ui::textColor((i == m_index) ? ccColor3B({255, 236, 179}) : ccColor3B({255, 255, 255})));
        button->setScale(0.8f);

        auto buttonClick = CCMenuItemExt::createSpriteExtra(button, [this, i](CCMenuItemSpriteExtra* sender) {
            switchTab(i);
        });
        m_buttonTabs.push_back(buttonClick);
        tabsMenu->addChild(buttonClick);

        auto tab = NXRHacksTab::create();
        tab->setVisible(i == m_index);
        tab->setID(fmt::format("{}"_spr, winName));
        m_mainLayer->addChild(tab);

        for (auto& hack : win.getHacks()) {
            tab->addToggle(hack);
        }

        if (win.avaibleCustomWindowCocos()) win.callCustomWindowCocos(tab);
        else tab->addPadding(2.5f);

        tab->m_scrollLayer->m_contentLayer->updateLayout();
        if (i == m_index && m_lastIndexScroll != -1)
            tab->m_scrollLayer->m_contentLayer->setPositionY(static_cast<float>(m_lastIndexScroll));
        else
            tab->m_scrollLayer->moveToTop();

        m_tabs.push_back(tab);

        i++;
    }


    auto addSpecialTab = [&](const std::string& tabName, const std::function<void(NXRHacksTab*)>& build) {
        const int tabIndex = i;
        auto button = ButtonSprite::create(tabName.c_str(), 90, true, "GoogleSans.fnt"_spr, (tabIndex == m_index) ? NXR::Theme::buttonOn() : NXR::Theme::button(), 30.f, 0.7f);
        button->m_label->setColor(NXR::Ui::textColor((tabIndex == m_index) ? ccColor3B({255, 236, 179}) : ccColor3B({255, 255, 255})));
        button->setScale(0.8f);

        auto buttonClick = CCMenuItemExt::createSpriteExtra(button, [this, tabIndex](CCMenuItemSpriteExtra*) {
            switchTab(tabIndex);
        });
        m_buttonTabs.push_back(buttonClick);
        tabsMenu->addChild(buttonClick);

        auto tab = NXRHacksTab::create();
        tab->setVisible(tabIndex == m_index);
        tab->setID(fmt::format("{}"_spr, tabName));
        m_mainLayer->addChild(tab);

        build(tab);

        tab->m_scrollLayer->m_contentLayer->updateLayout();
        if (tabIndex == m_index && m_lastIndexScroll != -1)
            tab->m_scrollLayer->m_contentLayer->setPositionY(static_cast<float>(m_lastIndexScroll));
        else
            tab->m_scrollLayer->moveToTop();

        m_tabs.push_back(tab);
        i++;
    };

    addSpecialTab("Keybind", [](NXRHacksTab* tab) { nxrBuildKeybindTab(tab); });
    addSpecialTab("Settings", [](NXRHacksTab* tab) { nxrBuildSettingsTab(tab); });

    tabsMenu->setLayout(
        geode::ColumnLayout::create()
            ->setGap(5.f)
            ->setAxisReverse(true)
            ->setAxisAlignment(geode::AxisAlignment::End)
            ->setCrossAxisAlignment(geode::AxisAlignment::Center)
            ->setAutoGrowAxis(true)
    );
    tabsMenu->updateLayout();

    float contentHeight = std::max(240.f, tabsMenu->getContentHeight() + 10.f);
    m_tabsScrollLayer->m_contentLayer->setContentSize({96.f, contentHeight});
    tabsMenu->setPosition({107.f, contentHeight - tabsMenu->getContentHeight() - 5.f});

    m_tabsScrollLayer->m_contentLayer->addChild(tabsMenu);
    m_tabsScrollLayer->moveToTop();
    m_mainLayer->addChild(m_tabsScrollLayer);

    NXR::Ui::applyTextStyle(m_mainLayer);

    geode::queueInMainThread([&config] {
        if (config.get<bool>("nxr.ui.need_update", false) && config.get<bool>("nxr.ui.notify_updates", true)) {
            NXR::MaterialLayer(FLAlertLayer::create("A new update is available!", "Please open the Geode menu and download the latest NXR update to get new features and bug fixes", "OK"))->show();
            config.set<bool>("nxr.ui.need_update", false);
        }
    });

    return true;
}

void NXRHacksLayer::switchTab(int newIndex) {
    if (newIndex < 0 || newIndex >= m_tabs.size()) return;

    auto& config = NXRConfig::get();
    m_index = newIndex;
    config.set<int>("nxr.gui_mobile.index", m_index);

    for (size_t i = 0; i < m_tabs.size(); i++) {
        m_tabs[i]->setVisible(i == m_index);
    }

    for (size_t i = 0; i < m_buttonTabs.size(); i++) {
        auto* button = m_buttonTabs[i];
        if (button) {
            auto* btnSprite = static_cast<ButtonSprite*>(button->getChildren()->objectAtIndex(0));
            if (btnSprite) {
                btnSprite->updateBGImage(i == m_index ? NXR::Theme::buttonOn() : NXR::Theme::button());
                btnSprite->m_label->setColor(NXR::Ui::textColor((i == m_index) ? ccColor3B({255, 236, 179}) : ccColor3B({255, 255, 255})));
            }
        }
    }
}

NXRHacksLayer* NXRHacksLayer::create() {
    auto* ret = new NXRHacksLayer();
    if (ret->init()) { ret->autorelease(); return ret; }
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

void NXRHacksLayer::onClose(CCObject* object) {
    auto& config = NXRConfig::get();
    config.set<int>("nxr.gui_mobile.lastIndexScroll", static_cast<int>(m_tabs[m_index]->m_scrollLayer->m_contentLayer->getPositionY()));
    config.save(getFileDataPath());

    Popup::onClose(object);
    instance = nullptr;
}
