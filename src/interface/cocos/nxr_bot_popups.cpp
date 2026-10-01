#include "nxr_menu.hpp"
#include "nxr_bot_popups.hpp"
#include <algorithm>
#include <Geode/ui/Scrollbar.hpp>
#include "../../core/nxr_bot.hpp"

NXRNamePopup* NXRNamePopup::create(const std::string& title, geode::Function<void(const std::string&)> onConfirm) {
    auto ret = new NXRNamePopup();
    if (ret->init(title, std::move(onConfirm))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool NXRNamePopup::init(const std::string& title, geode::Function<void(const std::string&)> onConfirm) {
    if (!geode::Popup::init(280.f, 150.f, "NXR_square.png"_spr)) return false;

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
    m_mainLayer->addChild(input);

    auto okSprite = ButtonSprite::create("OK", 90, true, "GoogleSans.fnt"_spr, "NXR_button_01.png"_spr, 26.f, 0.7f);
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

    return true;
}

NXRReplayInfoPopup* NXRReplayInfoPopup::create(const std::string& name, size_t actions, size_t clicks, uint64_t frames, float fps) {
    auto ret = new NXRReplayInfoPopup();
    if (ret->init(name, actions, clicks, frames, fps)) { ret->autorelease(); return ret; }
    delete ret;
    return nullptr;
}

bool NXRReplayInfoPopup::init(const std::string& name, size_t actions, size_t clicks, uint64_t frames, float fps) {
    if (!geode::Popup::init(240.f, 160.f, "NXR_square.png"_spr)) return false;
    auto size = m_mainLayer->getContentSize();

    auto closeSprite = cocos2d::CCSprite::create("NXR_closeBtn.png"_spr);
    closeSprite->setScale(0.75f);
    m_closeBtn->setSprite(closeSprite);

    auto title = geode::Label::create(name, "GoogleSans.fnt"_spr);
    title->setScale(0.55f);
    title->setPosition({size.width / 2.f, size.height - 22.f});
    m_mainLayer->addChild(title);

    auto text = cocos2d::CCLabelBMFont::create(
        fmt::format(
            "Total Actions: {}\nTotal Clicks: {}\nTotal Frames: {}\nFPS / DPS: {:.1f} / {:.1f}",
            actions, clicks, frames, fps, fps
        ).c_str(),
        "bigFont.fnt"
    );

    text->setScale(0.4f);
    text->setAnchorPoint({0.5f, 0.5f});
    text->setPosition({size.width / 2.f, (size.height - 40.f) / 2.f});
    m_mainLayer->addChild(text);
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

    if (!geode::Popup::init(280.f, 220.f, "NXR_square.png"_spr)) return false;
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
        sprite->updateBGImage(selected ? "NXR_button_02.png"_spr : "NXR_button_01.png"_spr);
        sprite->m_label->setColor(selected ? cocos2d::ccColor3B({240, 238, 252}) : cocos2d::ccColor3B({255, 255, 255}));
    }
}

bool NXRReplayPickerPopup::init(const std::string& title, const std::string& actionLabel, geode::Function<void(const std::string&)> onPick, bool allowClear) {
    if (!geode::Popup::init(260.f, 230.f, "NXR_square.png"_spr)) return false;

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
            size_t clicks = 0;
            for (const auto& event : macro.events) if (event.down()) ++clicks;
            NXRReplayInfoPopup::create(name, macro.events.size(), clicks, macro.totalFrames, 60.f)->show();
        });
        row->addChild(infoButton);

        auto sprite = ButtonSprite::create(name.c_str(), 150, true, "GoogleSans.fnt"_spr, "NXR_button_01.png"_spr, 26.f, 0.55f);
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

    auto actionSprite = ButtonSprite::create(actionLabel.c_str(), 110, true, "GoogleSans.fnt"_spr, "NXR_button_02.png"_spr, 26.f, 0.7f);
    actionSprite->m_label->setColor({240, 238, 252});
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
        sprite->updateBGImage(selected ? "NXR_button_02.png"_spr : "NXR_button_01.png"_spr);
        sprite->m_label->setColor(selected ? cocos2d::ccColor3B({240, 238, 252}) : cocos2d::ccColor3B({255, 255, 255}));
    }
}

bool NXRReplayBrowserPopup::init(const std::string& title, geode::Function<void(const std::filesystem::path&)> onPick) {
    if (!geode::Popup::init(300.f, 240.f, "NXR_square.png"_spr)) return false;

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

        auto sprite = ButtonSprite::create(name.c_str(), 230, true, "GoogleSans.fnt"_spr, "NXR_button_01.png"_spr, 26.f, 0.55f);
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

    auto actionSprite = ButtonSprite::create("Load", 110, true, "GoogleSans.fnt"_spr, "NXR_button_02.png"_spr, 26.f, 0.7f);
    actionSprite->m_label->setColor({240, 238, 252});
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

    return true;
}
