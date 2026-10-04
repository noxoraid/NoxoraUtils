#include "../../core/nxr_theme.hpp"
#include "nxr_menu.hpp"
#include "nxr_bot_popups.hpp"
#include "../../core/nxr_ui_mode.hpp"
#include <algorithm>
#include <filesystem>
#include <limits>
#include <vector>
#include "../../core/nxr_bot.hpp"
#include "../../core/nxr_level_stats.hpp"
#include "nxr_text_style.hpp"
#include "nxr_modal.hpp"
#include "nxr_ui_kit.hpp"

NXRNamePopup* NXRNamePopup::create(const std::string& title, geode::Function<void(const std::string&)> onConfirm, const std::string& initial) {
    auto ret = new NXRNamePopup();
    if (ret->init(title, std::move(onConfirm), initial)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool NXRNamePopup::init(const std::string& title, geode::Function<void(const std::string&)> onConfirm, const std::string& initial) {
    if (!geode::Popup::init(300.f, 170.f, NXR::Theme::square())) return false;

    m_onConfirm = std::move(onConfirm);
    auto size = m_mainLayer->getContentSize();
    NXR::Modal::skin({m_mainLayer, m_bgSprite, m_closeBtn, m_buttonMenu}, title, size.width, size.height);

    auto input = geode::TextInput::create(250.f, "File name", "GoogleSans.fnt"_spr);
    if (auto* bg = input->getChildByType<geode::NineSlice>(0)) {
        bg->setColor({11, 16, 26});
        bg->setOpacity(255);
    }
    input->setPosition({size.width / 2.f, size.height / 2.f + 8.f});
    input->setMaxCharCount(40);
    input->setFilter("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-. ");
    input->setCallback([this](const std::string& str) {
        m_text = str;
    });
    if (!initial.empty()) {
        input->setString(initial);
        m_text = initial;
    }
    m_mainLayer->addChild(input);

    auto cancelButton = NXR::Modal::button("Cancel", 112.f, 36.f, false, [this] { this->onClose(nullptr); });
    auto okButton = NXR::Modal::button("OK", 112.f, 36.f, true, [this] {
        std::string name;
        for (char c : m_text) {
            if (c >= 32 && std::string("\\/:*?\"<>|").find(c) == std::string::npos) name.push_back(c);
        }
        while (!name.empty() && name.back() == ' ') name.pop_back();
        while (!name.empty() && name.front() == ' ') name.erase(name.begin());

        if (name.empty()) {
            geode::Notification::create("Enter a file name", geode::NotificationIcon::Warning)->show();
            return;
        }

        if (m_onConfirm) m_onConfirm(name);
        this->onClose(nullptr);
    });
    auto menu = NXRMenu::create();
    menu->setPosition({0.f, 0.f});
    cancelButton->setPosition({size.width / 2.f - 62.f, 34.f});
    okButton->setPosition({size.width / 2.f + 62.f, 34.f});
    menu->addChild(cancelButton);
    menu->addChild(okButton);
    m_mainLayer->addChild(menu);
    return true;
}

NXRReplayInfoPopup* NXRReplayInfoPopup::create(const std::string& name, const NXR::Bot::Macro& macro) {
    auto ret = new NXRReplayInfoPopup();
    if (ret->init(name, macro)) { ret->autorelease(); return ret; }
    delete ret;
    return nullptr;
}

bool NXRReplayInfoPopup::init(const std::string& name, const NXR::Bot::Macro& macro) {
    if (!geode::Popup::init(330.f, 300.f, NXR::Theme::square())) return false;
    auto size = m_mainLayer->getContentSize();
    NXR::Modal::skin({m_mainLayer, m_bgSprite, m_closeBtn, m_buttonMenu}, name, size.width, size.height);

    const float tps = macro.tps > 0.f ? macro.tps : 240.f;
    const double seconds = static_cast<double>(macro.endFrame()) / static_cast<double>(tps);
    const int minutes = static_cast<int>(seconds) / 60;

    size_t clicksP1 = 0;
    size_t clicksP2 = 0;
    size_t releases = 0;
    size_t jumps = 0;
    size_t moves = 0;
    bool platformer = false;
    std::vector<uint64_t> pressFrames;
    pressFrames.reserve(macro.events.size() / 2 + 1);
    uint64_t pressedAt[2][4] = {};
    bool pressedNow[2][4] = {};
    uint64_t longestHold = 0;
    uint64_t shortestHold = std::numeric_limits<uint64_t>::max();
    size_t spamTaps = 0;
    const uint64_t spamLimit = std::max<uint64_t>(1, static_cast<uint64_t>(tps * 0.02f));

    for (const auto& event : macro.events) {
        const int slot = event.player() == 2 ? 1 : 0;
        const int button = std::clamp<int>(event.button(), 0, 3);

        if (event.button() != 1) platformer = true;

        if (!event.down()) {
            ++releases;
            if (pressedNow[slot][button]) {
                const uint64_t held = event.frame() - pressedAt[slot][button];
                longestHold = std::max(longestHold, held);
                shortestHold = std::min(shortestHold, held);
                if (held <= spamLimit) ++spamTaps;
                pressedNow[slot][button] = false;
            }
            continue;
        }

        (slot == 1 ? clicksP2 : clicksP1)++;
        pressFrames.push_back(event.frame());
        pressedAt[slot][button] = event.frame();
        pressedNow[slot][button] = true;
        if (event.button() == 1) ++jumps;
        else ++moves;
    }

    double maxCps = 0.0;
    {
        const uint64_t window = static_cast<uint64_t>(tps);
        size_t left = 0;
        for (size_t right = 0; right < pressFrames.size(); right++) {
            while (pressFrames[right] - pressFrames[left] >= window) ++left;
            maxCps = std::max(maxCps, static_cast<double>(right - left + 1));
        }
    }
    const double avgCps = seconds > 0.0 ? static_cast<double>(pressFrames.size()) / seconds : 0.0;

    std::error_code sizeError;
    const auto macroPath = NXR::Bot::macroPathFor(name);
    const auto bytes = std::filesystem::file_size(macroPath, sizeError);

    NXR::Stats::LevelStats level = macro.stats;
    const bool hasLevel = macro.hasStats || (macro.levelId != 0 && NXR::Stats::cachedFor(macro.levelId, level));
    auto levelValue = [&](uint32_t value) { return hasLevel ? std::to_string(value) : std::string("N/A"); };

    auto holdText = [&](uint64_t frames) {
        return fmt::format("{} f  ({:.3f}s)", frames, static_cast<double>(frames) / static_cast<double>(tps));
    };

    const std::vector<std::pair<std::string, std::string>> rows = {
        {"Level Name", macro.levelName.empty() ? "Unknown" : macro.levelName},
        {"Level ID", macro.levelId > 0 ? std::to_string(macro.levelId) : "N/A"},
        {"Mode", platformer ? "Platformer" : "Normal"},
        {"Version", macro.version.empty() ? "N/A" : macro.version},
        {"Total Jumps", std::to_string(jumps)},
        {"Move Inputs (L / R)", platformer ? std::to_string(moves) : std::string("N/A")},
        {"Total Actions", std::to_string(macro.events.size())},
        {"Total Clicks", fmt::format("{}  (P1 {} / P2 {})", clicksP1 + clicksP2, clicksP1, clicksP2)},
        {"Total Releases", std::to_string(releases)},
        {"Max CPS", fmt::format("{:.0f}", maxCps)},
        {"Average CPS", fmt::format("{:.2f}", avgCps)},
        {"Longest Hold", pressFrames.empty() ? std::string("N/A") : holdText(longestHold)},
        {"Shortest Hold", shortestHold == std::numeric_limits<uint64_t>::max() ? std::string("N/A") : holdText(shortestHold)},
        {"Spam Taps (<= 0.02s)", std::to_string(spamTaps)},
        {"Total Objects", levelValue(level.objects)},
        {"Interactables (Orb+Pad+Portal)", levelValue(level.interactables())},
        {"Orbs", levelValue(level.orbs)},
        {"Pads", levelValue(level.pads)},
        {"Portals (All)", levelValue(level.portals)},
        {"Gravity Portals", levelValue(level.gravityPortals)},
        {"Gamemode Portals", levelValue(level.modePortals)},
        {"Speed Portals", levelValue(level.speedPortals)},
        {"Size Portals", levelValue(level.sizePortals)},
        {"Mirror Portals", levelValue(level.mirrorPortals)},
        {"Dual / Solo Portals", levelValue(level.dualPortals)},
        {"Hazards", levelValue(level.hazards)},
        {"Coins", levelValue(level.coins)},
        {"Triggers", levelValue(level.triggers)},
        {"Start Positions", levelValue(level.startPositions)},
        {"Total Frames", std::to_string(macro.totalFrames)},
        {"Recorded Frames", std::to_string(macro.frames.size())},
        {"Super Frames", std::to_string(macro.supers.size())},
        {"Last Frame", std::to_string(macro.endFrame())},
        {"FPS / TPS", fmt::format("{:.1f} / {:.1f}", macro.fps > 0.f ? macro.fps : 60.f, tps)},
        {"Duration", fmt::format("{}:{:04.1f}", minutes, seconds - static_cast<double>(minutes) * 60.0)},
        {"File Size", sizeError ? std::string("N/A") : fmt::format("{:.1f} KB", static_cast<double>(bytes) / 1024.0)},
    };

    constexpr float kRowWidth = 290.f;
    constexpr float kRowHeight = 20.f;

    auto* scroll = geode::prelude::ScrollLayer::create({kRowWidth, size.height - 58.f});
    scroll->setPosition({(size.width - kRowWidth) / 2.f - 4.f, 14.f});
    scroll->m_contentLayer->setLayout(
        geode::ColumnLayout::create()
            ->setAutoScale(false)
            ->setAxisReverse(true)
            ->setAutoGrowAxis(true)
            ->setGap(0.f)
    );

    for (size_t i = 0; i < rows.size(); i++) {
        auto* row = cocos2d::CCLayerColor::create({255, 255, 255, static_cast<GLubyte>(i % 2 == 0 ? 12 : 0)});
        row->setContentSize({kRowWidth, kRowHeight});

        auto* key = geode::Label::create(rows[i].first, "GoogleSans.fnt"_spr);
        key->setAnchorPoint({0.f, 0.5f});
        key->setScale(0.46f);
        key->setColor(NXR::Kit::Pal::muted());
        key->setUserObject(NXR::Ui::kStyledMarker, cocos2d::CCString::create("1"));
        key->setPosition({10.f, kRowHeight / 2.f});
        row->addChild(key);

        auto* value = geode::Label::create(rows[i].second, "GoogleSans.fnt"_spr);
        value->setAnchorPoint({1.f, 0.5f});
        value->setScale(0.46f);
        value->setColor(NXR::Kit::Pal::text());
        value->setUserObject(NXR::Ui::kStyledMarker, cocos2d::CCString::create("1"));
        const float room = kRowWidth - 28.f - key->getScaledContentWidth();
        if (value->getScaledContentWidth() > room) value->setScale(value->getScale() * room / value->getScaledContentWidth());
        value->setPosition({kRowWidth - 10.f, kRowHeight / 2.f});
        row->addChild(value);

        scroll->m_contentLayer->addChild(row);
    }

    scroll->m_contentLayer->updateLayout();
    scroll->moveToTop();
    m_mainLayer->addChild(scroll);

    m_mainLayer->addChild(NXR::Kit::ScrollGrip::create(scroll, size.width - 26.f, 14.f, size.height - 62.f, 22.f), 6);

    return true;
}

NXRInfoPopup* NXRInfoPopup::create(const std::string& title, const std::string& body) {
    auto ret = new NXRInfoPopup();
    if (ret->init(title, body)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool NXRInfoPopup::init(const std::string& title, const std::string& body) {
    if (!geode::Popup::init(300.f, 230.f, NXR::Theme::square())) return false;
    auto size = m_mainLayer->getContentSize();
    NXR::Modal::skin({m_mainLayer, m_bgSprite, m_closeBtn, m_buttonMenu}, title, size.width, size.height);

    auto* scroll = geode::prelude::ScrollLayer::create({size.width - 56.f, size.height - 62.f});
    scroll->setPosition({14.f, 16.f});
    scroll->m_contentLayer->setLayout(
        geode::ColumnLayout::create()
            ->setAutoScale(false)
            ->setAxisReverse(true)
            ->setAutoGrowAxis(true)
            ->setGap(0.f)
    );

    const size_t wrapAt = std::max<size_t>(16, static_cast<size_t>((size.width - 72.f) / 5.2f));
    std::string wrapped;
    for (const auto& line : NXR::Modal::wrap(body, wrapAt)) wrapped += line + "\n";

    auto text = cocos2d::CCLabelBMFont::create(wrapped.c_str(), "chatFont.fnt");
    text->setScale(0.55f);
    text->setColor(NXR::Kit::Pal::text());
    text->setUserObject(NXR::Ui::kStyledMarker, cocos2d::CCString::create("1"));
    scroll->m_contentLayer->addChild(text);
    scroll->m_contentLayer->updateLayout();
    scroll->moveToTop();
    m_mainLayer->addChild(scroll);

    m_mainLayer->addChild(NXR::Kit::ScrollGrip::create(scroll, size.width - 34.f, 16.f, size.height - 62.f, 22.f), 6);

    return true;
}

void NXRInfoPopup::registerWithTouchDispatcher() {

    cocos2d::CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, -200, true);
}

bool NXRInfoPopup::ccTouchBegan(cocos2d::CCTouch*, cocos2d::CCEvent*) {
    this->onClose(nullptr);
    return true;
}

NXRReplayPickerPopup* NXRReplayPickerPopup::create(const std::string& title, const std::string& actionLabel, geode::Function<void(const std::string&)> onPick, bool allowClear) {
    auto ret = new NXRReplayPickerPopup();
    if (ret->init(title, actionLabel, std::move(onPick), allowClear)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

void NXRReplayPickerPopup::refreshSelection() {
    for (size_t i = 0; i < m_items.size(); i++) {
        if (auto* row = geode::cast::typeinfo_cast<NXR::Modal::Row*>(m_items[i]->getNormalImage())) {
            row->setSelected(m_names[i] == m_selected);
        }
    }
}

bool NXRReplayPickerPopup::init(const std::string& title, const std::string& actionLabel, geode::Function<void(const std::string&)> onPick, bool allowClear) {
    if (!geode::Popup::init(320.f, 300.f, NXR::Theme::square())) return false;

    m_onPick = std::move(onPick);
    m_allowClear = allowClear;
    auto size = m_mainLayer->getContentSize();
    NXR::Modal::skin({m_mainLayer, m_bgSprite, m_closeBtn, m_buttonMenu}, title, size.width, size.height);

    if (m_allowClear) {
        auto hint = geode::Label::create("Tap the selected replay again to deselect", "GoogleSans.fnt"_spr);
        hint->setScale(0.38f);
        hint->setColor(NXR::Kit::Pal::muted());
        hint->setUserObject(NXR::Ui::kStyledMarker, cocos2d::CCString::create("1"));
        hint->setPosition({size.width / 2.f, size.height - 55.f});
        m_mainLayer->addChild(hint);
    }

    auto* scroll = geode::prelude::ScrollLayer::create({286.f, 172.f});
    scroll->setPosition({10.f, 62.f});
    scroll->m_contentLayer->setLayout(
        geode::ColumnLayout::create()
            ->setAutoScale(false)
            ->setAxisReverse(true)
            ->setAutoGrowAxis(true)
            ->setGap(6.f)
    );
    m_mainLayer->addChild(scroll);
    m_mainLayer->addChild(NXR::Kit::ScrollGrip::create(scroll, size.width - 26.f, 62.f, 172.f, 22.f), 6);

    m_names = NXR::Bot::listMacros();
    std::sort(m_names.begin(), m_names.end());

    if (m_names.empty()) {
        auto empty = geode::Label::create("No replays found", "GoogleSans.fnt"_spr);
        empty->setScale(0.5f);
        empty->setColor(NXR::Kit::Pal::muted());
        empty->setUserObject(NXR::Ui::kStyledMarker, cocos2d::CCString::create("1"));
        empty->setContentHeight(30.f);
        scroll->m_contentLayer->addChild(empty);
    }

    for (size_t i = 0; i < m_names.size(); i++) {
        std::string name = m_names[i];

        auto* row = NXRMenu::create();
        row->setContentSize({280.f, 38.f});
        row->setLayout(
            geode::RowLayout::create()
                ->setGap(8.f)
                ->setAxisAlignment(geode::AxisAlignment::Center)
                ->setCrossAxisAlignment(geode::AxisAlignment::Center)
                ->setAutoScale(false)
        );

        auto infoButton = geode::cocos::CCMenuItemExt::createSpriteExtra(NXR::Modal::iconNode(24.f), [name](CCMenuItemSpriteExtra*) {
            NXR::Bot::Macro macro;
            if (!NXR::Bot::loadMacro(macro, NXR::Bot::macroPathFor(name))) return;
            NXR::Ui::showPopup(NXRReplayInfoPopup::create(name, macro), "Info: " + name);
        });
        row->addChild(infoButton);

        geode::Ref<CCMenuItemSpriteExtra> button = geode::cocos::CCMenuItemExt::createSpriteExtra(NXR::Modal::Row::create(name, 236.f, 36.f, false), [this, name](CCMenuItemSpriteExtra*) {
            m_selected = (m_allowClear && m_selected == name) ? std::string() : name;
            refreshSelection();
        });
        row->addChild(button);
        m_items.push_back(button);

        row->updateLayout();
        scroll->m_contentLayer->addChild(row);
    }

    scroll->m_contentLayer->updateLayout();
    scroll->moveToTop();
    if (m_allowClear) {
        const auto& current = NXR::Bot::State::get().selectedReplay;
        if (std::find(m_names.begin(), m_names.end(), current) != m_names.end()) m_selected = current;
    } else if (!m_names.empty()) {
        m_selected = m_names.front();
    }
    refreshSelection();

    auto actionButton = NXR::Modal::button(actionLabel, 160.f, 38.f, true, [this] {
        if (m_selected.empty() && !m_allowClear) {
            geode::Notification::create("Pick a replay first", geode::NotificationIcon::Warning)->show();
            return;
        }
        if (m_onPick) m_onPick(m_selected);
        this->onClose(nullptr);
    });
    auto menu = NXRMenu::create();
    menu->setPosition({0.f, 0.f});
    actionButton->setPosition({size.width / 2.f, 30.f});
    menu->addChild(actionButton);
    m_mainLayer->addChild(menu);

    return true;
}

NXRReplayBrowserPopup* NXRReplayBrowserPopup::create(const std::string& title, geode::Function<void(const std::filesystem::path&)> onPick) {
    auto ret = new NXRReplayBrowserPopup();
    if (ret->init(title, std::move(onPick))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

void NXRReplayBrowserPopup::refreshSelection() {
    for (size_t i = 0; i < m_items.size(); i++) {
        if (auto* row = geode::cast::typeinfo_cast<NXR::Modal::Row*>(m_items[i]->getNormalImage())) {
            row->setSelected(i == m_selected);
        }
    }
}

bool NXRReplayBrowserPopup::init(const std::string& title, geode::Function<void(const std::filesystem::path&)> onPick) {
    if (!geode::Popup::init(330.f, 300.f, NXR::Theme::square())) return false;

    m_onPick = std::move(onPick);
    auto size = m_mainLayer->getContentSize();
    NXR::Modal::skin({m_mainLayer, m_bgSprite, m_closeBtn, m_buttonMenu}, title, size.width, size.height);

    auto* scroll = geode::prelude::ScrollLayer::create({296.f, 188.f});
    scroll->setPosition({8.f, 62.f});
    scroll->m_contentLayer->setLayout(
        geode::ColumnLayout::create()
            ->setAutoScale(false)
            ->setAxisReverse(true)
            ->setAutoGrowAxis(true)
            ->setGap(6.f)
    );
    m_mainLayer->addChild(scroll);
    m_mainLayer->addChild(NXR::Kit::ScrollGrip::create(scroll, size.width - 26.f, 62.f, 188.f, 22.f), 6);

    m_files = NXR::Bot::scanReplayFiles();

    if (m_files.empty()) {
        auto empty = geode::Label::create("No replays found", "GoogleSans.fnt"_spr);
        empty->setScale(0.5f);
        empty->setColor(NXR::Kit::Pal::muted());
        empty->setUserObject(NXR::Ui::kStyledMarker, cocos2d::CCString::create("1"));
        empty->setContentHeight(30.f);
        scroll->m_contentLayer->addChild(empty);
    }

    for (size_t i = 0; i < m_files.size(); i++) {
        std::string name = m_files[i].label;
        if (name.size() > 28) name = name.substr(0, 25) + "...";
        name += " [" + m_files[i].ext + "]";

        auto* row = NXRMenu::create();
        row->setContentSize({290.f, 38.f});
        row->setLayout(
            geode::RowLayout::create()
                ->setGap(8.f)
                ->setAxisAlignment(geode::AxisAlignment::Center)
                ->setCrossAxisAlignment(geode::AxisAlignment::Center)
                ->setAutoScale(false)
        );

        geode::Ref<CCMenuItemSpriteExtra> button = geode::cocos::CCMenuItemExt::createSpriteExtra(NXR::Modal::Row::create(name, 280.f, 36.f, false), [this, i](CCMenuItemSpriteExtra*) {
            m_selected = i;
            refreshSelection();
        });
        row->addChild(button);
        m_items.push_back(button);

        row->updateLayout();
        scroll->m_contentLayer->addChild(row);
    }

    scroll->m_contentLayer->updateLayout();
    scroll->moveToTop();
    if (!m_files.empty()) m_selected = 0;
    refreshSelection();

    auto actionButton = NXR::Modal::button("Load", 160.f, 38.f, true, [this] {
        if (m_selected >= m_files.size()) {
            geode::Notification::create("Pick a replay first", geode::NotificationIcon::Warning)->show();
            return;
        }
        const auto path = m_files[m_selected].path;
        if (m_onPick) m_onPick(path);
        this->onClose(nullptr);
    });
    auto menu = NXRMenu::create();
    menu->setPosition({0.f, 0.f});
    actionButton->setPosition({size.width / 2.f, 30.f});
    menu->addChild(actionButton);
    m_mainLayer->addChild(menu);

    return true;
}

NXRChoicePopup* NXRChoicePopup::create(const std::string& title, const std::string& body, std::vector<std::pair<std::string, std::function<void()>>> choices) {
    auto ret = new NXRChoicePopup();
    if (ret->init(title, NXR::Modal::wrap(body, 44), std::move(choices))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool NXRChoicePopup::init(const std::string& title, const std::vector<std::string>& lines, std::vector<std::pair<std::string, std::function<void()>>> choices) {
    if (choices.size() < 2) return false;
    const float height = std::clamp(112.f + 15.f * static_cast<float>(lines.size()), 150.f, 280.f);
    if (!geode::Popup::init(320.f, height, NXR::Theme::square())) return false;
    auto size = m_mainLayer->getContentSize();
    NXR::Modal::skin({m_mainLayer, m_bgSprite, m_closeBtn, m_buttonMenu}, title, size.width, size.height);

    float y = size.height - 62.f;
    for (const auto& line : lines) {
        if (line.empty()) {
            y -= 8.f;
            continue;
        }
        auto* label = geode::Label::create(line, "GoogleSans.fnt"_spr);
        label->setScale(0.44f);
        label->setColor(NXR::Kit::Pal::text());
        label->setUserObject(NXR::Ui::kStyledMarker, cocos2d::CCString::create("1"));
        label->setPosition({size.width / 2.f, y});
        m_mainLayer->addChild(label);
        y -= 15.f;
    }

    auto first = choices[0].second;
    auto second = choices[1].second;
    auto firstButton = NXR::Modal::button(choices[0].first, 124.f, 38.f, false, [this, first] {
        this->onClose(nullptr);
        if (first) first();
    });
    auto secondButton = NXR::Modal::button(choices[1].first, 124.f, 38.f, true, [this, second] {
        this->onClose(nullptr);
        if (second) second();
    });
    auto menu = NXRMenu::create();
    menu->setPosition({0.f, 0.f});
    firstButton->setPosition({size.width / 2.f - 70.f, 32.f});
    secondButton->setPosition({size.width / 2.f + 70.f, 32.f});
    menu->addChild(firstButton);
    menu->addChild(secondButton);
    m_mainLayer->addChild(menu);
    return true;
}
