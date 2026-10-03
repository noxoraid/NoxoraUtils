#include "../../core/nxr_theme.hpp"
#include "nxr_menu.hpp"
#include "nxr_bot_popups.hpp"
#include "../../core/nxr_ui_mode.hpp"
#include <algorithm>
#include <filesystem>
#include <limits>
#include <vector>
#include <Geode/ui/Scrollbar.hpp>
#include "../../core/nxr_bot.hpp"
#include "../../core/nxr_level_stats.hpp"
#include "nxr_text_style.hpp"

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
    if (!geode::Popup::init(280.f, 150.f, NXR::Theme::square())) return false;

    m_onConfirm = std::move(onConfirm);
    auto size = m_mainLayer->getContentSize();

    auto titleLabel = geode::Label::create(title, "GoogleSans.fnt"_spr);
    titleLabel->setPosition({size.width / 2.f, size.height - 20.f});
    titleLabel->setScale(0.65f);
    m_mainLayer->addChild(titleLabel);

    auto closeSprite = cocos2d::CCSprite::create("NXR_closeBtn.png"_spr);
    closeSprite->setScale(0.75f);
    m_closeBtn->setSprite(closeSprite);

    auto input = geode::TextInput::create(210.f, "File name", "GoogleSans.fnt"_spr);
    if (auto* bg = input->getChildByType<geode::NineSlice>(0)) {
        bg->setColor({34, 33, 46});
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

    auto okSprite = ButtonSprite::create("OK", 90, true, "GoogleSans.fnt"_spr, NXR::Theme::button(), 26.f, 0.7f);
    auto okButton = geode::cocos::CCMenuItemExt::createSpriteExtra(okSprite, [this](CCMenuItemSpriteExtra*) {
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
    okButton->setPosition({size.width / 2.f, 28.f});
    menu->addChild(okButton);
    m_mainLayer->addChild(menu);

    NXR::Ui::applyTextStyle(m_mainLayer);
    return true;
}

NXRReplayInfoPopup* NXRReplayInfoPopup::create(const std::string& name, const NXR::Bot::Macro& macro) {
    auto ret = new NXRReplayInfoPopup();
    if (ret->init(name, macro)) { ret->autorelease(); return ret; }
    delete ret;
    return nullptr;
}

bool NXRReplayInfoPopup::init(const std::string& name, const NXR::Bot::Macro& macro) {
    if (!geode::Popup::init(300.f, 250.f, NXR::Theme::square())) return false;
    auto size = m_mainLayer->getContentSize();

    auto closeSprite = cocos2d::CCSprite::create("NXR_closeBtn.png"_spr);
    closeSprite->setScale(0.75f);
    m_closeBtn->setSprite(closeSprite);

    auto title = geode::Label::create(name, "GoogleSans.fnt"_spr);
    title->setScale(0.6f);
    if (title->getScaledContentSize().width > 220.f) title->setScale(220.f / title->getContentSize().width);
    title->setPosition({size.width / 2.f, size.height - 18.f});
    m_mainLayer->addChild(title);

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

    constexpr float kRowWidth = 270.f;
    constexpr float kRowHeight = 17.f;

    auto* scroll = geode::prelude::ScrollLayer::create({kRowWidth, size.height - 50.f});
    scroll->setPosition({(size.width - kRowWidth) / 2.f - 4.f, 14.f});
    scroll->m_contentLayer->setLayout(
        geode::ColumnLayout::create()
            ->setAutoScale(false)
            ->setAxisReverse(true)
            ->setAutoGrowAxis(true)
            ->setGap(0.f)
    );

    for (size_t i = 0; i < rows.size(); i++) {
        auto* row = cocos2d::CCLayerColor::create({255, 255, 255, static_cast<GLubyte>(i % 2 == 0 ? 14 : 0)});
        row->setContentSize({kRowWidth, kRowHeight});

        auto* key = geode::Label::create(rows[i].first, "GoogleSans.fnt"_spr);
        key->setAnchorPoint({0.f, 0.5f});
        key->setScale(0.5f);
        key->setColor({170, 170, 190});
        key->setPosition({8.f, kRowHeight / 2.f});
        row->addChild(key);

        auto* value = geode::Label::create(rows[i].second, "GoogleSans.fnt"_spr);
        value->setAnchorPoint({1.f, 0.5f});
        value->setScale(0.5f);
        const float room = kRowWidth - 24.f - key->getScaledContentWidth();
        if (value->getScaledContentWidth() > room) value->setScale(value->getScale() * room / value->getScaledContentWidth());
        value->setPosition({kRowWidth - 8.f, kRowHeight / 2.f});
        row->addChild(value);

        scroll->m_contentLayer->addChild(row);
    }

    scroll->m_contentLayer->updateLayout();
    scroll->moveToTop();
    m_mainLayer->addChild(scroll);

    auto* scrollbar = geode::Scrollbar::create(scroll);
    scrollbar->setPosition({size.width - 8.f, size.height / 2.f - 14.f});
    m_mainLayer->addChild(scrollbar);

    NXR::Ui::applyTextStyle(m_mainLayer);
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

    if (!geode::Popup::init(280.f, 220.f, NXR::Theme::square())) return false;
    auto size = m_mainLayer->getContentSize();

    auto closeSprite = cocos2d::CCSprite::create("NXR_closeBtn.png"_spr);
    closeSprite->setScale(0.75f);
    m_closeBtn->setSprite(closeSprite);

    auto titleLabel = geode::Label::create(title, "GoogleSans.fnt"_spr);
    titleLabel->setScale(0.6f);
    titleLabel->setPosition({size.width / 2.f, size.height - 18.f});
    m_mainLayer->addChild(titleLabel);

    auto* scroll = geode::prelude::ScrollLayer::create({size.width - 30.f, size.height - 50.f});
    scroll->setPosition({15.f, 20.f});
    scroll->m_contentLayer->setLayout(
        geode::ColumnLayout::create()
            ->setAutoScale(false)
            ->setAxisReverse(true)
            ->setAutoGrowAxis(true)
            ->setGap(0.f)
    );

    const size_t wrapAt = std::max<size_t>(16, static_cast<size_t>((size.width - 40.f) / 5.2f));
    std::string wrapped;
    {
        std::string line;
        std::string word;
        auto flushWord = [&]() {
            if (word.empty()) return;
            if (!line.empty() && line.size() + 1 + word.size() > wrapAt) {
                wrapped += line + "\n";
                line.clear();
            }
            if (!line.empty()) line += " ";
            line += word;
            word.clear();
        };
        for (char c : body) {
            if (c == '\n') {
                flushWord();
                wrapped += line + "\n";
                line.clear();
            } else if (c == ' ') {
                flushWord();
            } else {
                word.push_back(c);
            }
        }
        flushWord();
        wrapped += line;
    }

    auto text = cocos2d::CCLabelBMFont::create(wrapped.c_str(), "chatFont.fnt");
    text->setScale(0.55f);
    scroll->m_contentLayer->addChild(text);
    scroll->m_contentLayer->updateLayout();
    scroll->moveToTop();
    m_mainLayer->addChild(scroll);

    auto* scrollbar = geode::Scrollbar::create(scroll);
    scrollbar->setPosition({size.width - 2.f, size.height / 2.f});
    m_mainLayer->addChild(scrollbar);

    NXR::Ui::applyTextStyle(m_mainLayer);
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
        auto* sprite = static_cast<ButtonSprite*>(m_items[i]->getNormalImage());
        bool selected = m_names[i] == m_selected;
        sprite->updateBGImage(selected ? NXR::Theme::buttonOn() : NXR::Theme::button());
        sprite->m_label->setColor(NXR::Ui::textColor(selected ? cocos2d::ccColor3B({255, 236, 179}) : cocos2d::ccColor3B({255, 255, 255})));
    }
}

bool NXRReplayPickerPopup::init(const std::string& title, const std::string& actionLabel, geode::Function<void(const std::string&)> onPick, bool allowClear) {
    if (!geode::Popup::init(260.f, 230.f, NXR::Theme::square())) return false;

    m_onPick = std::move(onPick);
    m_allowClear = allowClear;
    auto size = m_mainLayer->getContentSize();

    auto titleLabel = geode::Label::create(title, "GoogleSans.fnt"_spr);
    titleLabel->setPosition({size.width / 2.f, size.height - 18.f});
    titleLabel->setScale(0.6f);
    m_mainLayer->addChild(titleLabel);

    if (m_allowClear) {
        auto hint = geode::Label::create("Tap the selected replay again to deselect", "GoogleSans.fnt"_spr);
        hint->setScale(0.38f);
        hint->setOpacity(160);
        hint->setPosition({size.width / 2.f, size.height - 33.f});
        m_mainLayer->addChild(hint);
    }

    auto closeSprite = cocos2d::CCSprite::create("NXR_closeBtn.png"_spr);
    closeSprite->setScale(0.75f);
    m_closeBtn->setSprite(closeSprite);

    auto* scroll = geode::prelude::ScrollLayer::create({240.f, 150.f});
    scroll->setPosition({size.width / 2.f - 120.f, 42.f});
    scroll->m_contentLayer->setLayout(
        geode::ColumnLayout::create()
            ->setAutoScale(false)
            ->setAxisReverse(true)
            ->setAutoGrowAxis(true)
            ->setGap(6.f)
    );
    m_mainLayer->addChild(scroll);

    m_names = NXR::Bot::listMacros();
    std::sort(m_names.begin(), m_names.end());

    if (m_names.empty()) {
        auto empty = geode::Label::create("No replays found", "GoogleSans.fnt"_spr);
        empty->setScale(0.5f);
        empty->setContentHeight(30.f);
        scroll->m_contentLayer->addChild(empty);
    }

    for (size_t i = 0; i < m_names.size(); i++) {
        std::string name = m_names[i];

        auto* row = NXRMenu::create();
        row->setContentSize({230.f, 32.f});
        row->setLayout(
            geode::RowLayout::create()
                ->setGap(8.f)
                ->setAxisAlignment(geode::AxisAlignment::Center)
                ->setCrossAxisAlignment(geode::AxisAlignment::Center)
                ->setAutoScale(false)
        );

        auto infoSprite = cocos2d::CCSprite::create("NXR_infoIcon.png"_spr);
        infoSprite->setScale(0.6f);
        auto infoButton = geode::cocos::CCMenuItemExt::createSpriteExtra(infoSprite, [name](CCMenuItemSpriteExtra*) {
            NXR::Bot::Macro macro;
            if (!NXR::Bot::loadMacro(macro, NXR::Bot::macroPathFor(name))) return;
            NXR::Ui::showPopup(NXRReplayInfoPopup::create(name, macro), "Info: " + name);
        });
        row->addChild(infoButton);

        auto sprite = ButtonSprite::create(name.c_str(), 150, true, "GoogleSans.fnt"_spr, NXR::Theme::button(), 26.f, 0.55f);
        geode::Ref<CCMenuItemSpriteExtra> button = geode::cocos::CCMenuItemExt::createSpriteExtra(sprite, [this, name](CCMenuItemSpriteExtra*) {
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

    auto actionSprite = ButtonSprite::create(actionLabel.c_str(), 110, true, "GoogleSans.fnt"_spr, NXR::Theme::buttonOn(), 26.f, 0.7f);
    actionSprite->m_label->setColor(NXR::Ui::textColor({255, 236, 179}));
    auto actionButton = geode::cocos::CCMenuItemExt::createSpriteExtra(actionSprite, [this](CCMenuItemSpriteExtra*) {
        if (m_selected.empty() && !m_allowClear) {
            geode::Notification::create("Pick a replay first", geode::NotificationIcon::Warning)->show();
            return;
        }
        if (m_onPick) m_onPick(m_selected);
        this->onClose(nullptr);
    });
    auto menu = NXRMenu::create();
    menu->setPosition({0.f, 0.f});
    actionButton->setPosition({size.width / 2.f, 24.f});
    menu->addChild(actionButton);
    m_mainLayer->addChild(menu);

    NXR::Ui::applyTextStyle(m_mainLayer);
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
        auto* sprite = static_cast<ButtonSprite*>(m_items[i]->getNormalImage());
        bool selected = i == m_selected;
        sprite->updateBGImage(selected ? NXR::Theme::buttonOn() : NXR::Theme::button());
        sprite->m_label->setColor(NXR::Ui::textColor(selected ? cocos2d::ccColor3B({255, 236, 179}) : cocos2d::ccColor3B({255, 255, 255})));
    }
}

bool NXRReplayBrowserPopup::init(const std::string& title, geode::Function<void(const std::filesystem::path&)> onPick) {
    if (!geode::Popup::init(300.f, 240.f, NXR::Theme::square())) return false;

    m_onPick = std::move(onPick);
    auto size = m_mainLayer->getContentSize();

    auto titleLabel = geode::Label::create(title, "GoogleSans.fnt"_spr);
    titleLabel->setPosition({size.width / 2.f, size.height - 18.f});
    titleLabel->setScale(0.6f);
    m_mainLayer->addChild(titleLabel);

    auto closeSprite = cocos2d::CCSprite::create("NXR_closeBtn.png"_spr);
    closeSprite->setScale(0.75f);
    m_closeBtn->setSprite(closeSprite);

    auto* scroll = geode::prelude::ScrollLayer::create({270.f, 160.f});
    scroll->setPosition({size.width / 2.f - 135.f, 44.f});
    scroll->m_contentLayer->setLayout(
        geode::ColumnLayout::create()
            ->setAutoScale(false)
            ->setAxisReverse(true)
            ->setAutoGrowAxis(true)
            ->setGap(6.f)
    );
    m_mainLayer->addChild(scroll);

    m_files = NXR::Bot::scanReplayFiles();

    if (m_files.empty()) {
        auto empty = geode::Label::create("No replays found", "GoogleSans.fnt"_spr);
        empty->setScale(0.5f);
        empty->setContentHeight(30.f);
        scroll->m_contentLayer->addChild(empty);
    }

    for (size_t i = 0; i < m_files.size(); i++) {
        std::string name = m_files[i].label;
        if (name.size() > 26) name = name.substr(0, 23) + "...";
        name += " [" + m_files[i].ext + "]";

        auto* row = NXRMenu::create();
        row->setContentSize({260.f, 32.f});
        row->setLayout(
            geode::RowLayout::create()
                ->setGap(8.f)
                ->setAxisAlignment(geode::AxisAlignment::Center)
                ->setCrossAxisAlignment(geode::AxisAlignment::Center)
                ->setAutoScale(false)
        );

        auto sprite = ButtonSprite::create(name.c_str(), 230, true, "GoogleSans.fnt"_spr, NXR::Theme::button(), 26.f, 0.55f);
        geode::Ref<CCMenuItemSpriteExtra> button = geode::cocos::CCMenuItemExt::createSpriteExtra(sprite, [this, i](CCMenuItemSpriteExtra*) {
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

    auto actionSprite = ButtonSprite::create("Load", 110, true, "GoogleSans.fnt"_spr, NXR::Theme::buttonOn(), 26.f, 0.7f);
    actionSprite->m_label->setColor(NXR::Ui::textColor({255, 236, 179}));
    auto actionButton = geode::cocos::CCMenuItemExt::createSpriteExtra(actionSprite, [this](CCMenuItemSpriteExtra*) {
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
    actionButton->setPosition({size.width / 2.f, 24.f});
    menu->addChild(actionButton);
    m_mainLayer->addChild(menu);

    NXR::Ui::applyTextStyle(m_mainLayer);
    return true;
}
