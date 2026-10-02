#include "../../core/nxr_theme.hpp"
#include "nxr_menu.hpp"
#include <Geode/ui/Scrollbar.hpp>
#include "nxr_hack_settings_popup.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_utils.hpp"
#include <memory>
#include <algorithm>
#include "nxr_text_style.hpp"

void updatePopupRowAlignment(cocos2d::CCMenu* row) {
    if (!row) return;
    if (auto layout = static_cast<geode::RowLayout*>(row->getLayout())) {
        row->updateLayout();
    }
}

NXRHackSettingsPopup* NXRHackSettingsPopup::create(NXR::Hack& hack) {
    auto ret = new NXRHackSettingsPopup();
    if (ret->init(hack)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool NXRHackSettingsPopup::init(NXR::Hack& hack) {
    if (!geode::Popup::init(260.f, 230.f, NXR::Theme::square())) return false;

    m_hack = &hack;
    auto winSize = m_mainLayer->getContentSize();

    auto title = geode::Label::create(hack.getName() + " Settings", "GoogleSans.fnt"_spr);
    title->setPosition({winSize.width / 2.f, winSize.height - 18.f});
    title->setScale(0.6f);
    m_mainLayer->addChild(title);
    NXR::Ui::applyTextStyle(title);

    auto closeBtn = cocos2d::CCSprite::create("NXR_closeBtn.png"_spr);
    closeBtn->setScale(0.75f);
    m_closeBtn->setSprite(closeBtn);

    m_scrollLayer = geode::prelude::ScrollLayer::create({240.f, 190.f});
    m_scrollLayer->setPosition({winSize.width / 2.f - 120.f, 10.f});
    m_scrollLayer->m_contentLayer->setLayout(
        geode::ColumnLayout::create()
            ->setAutoScale(false)
            ->setAxisReverse(true)
            ->setAutoGrowAxis(true)
            ->setGap(6.f)
    );
    m_mainLayer->addChild(m_scrollLayer);

    rebuild();

    return true;
}

void NXRHackSettingsPopup::rebuild() {
    m_currentRow = nullptr;
    m_scrollLayer->m_contentLayer->removeAllChildren();
    m_scrollLayer->m_contentLayer->addChild(cocos2d::CCNode::create());

    if (m_hack->avaibleCustomWindowCocos()) {
        m_hack->callCustomWindowCocos(this);
    }

    m_scrollLayer->m_contentLayer->addChild(cocos2d::CCNode::create());

    NXR::Ui::applyTextStyle(m_scrollLayer->m_contentLayer);

    m_scrollLayer->m_contentLayer->updateLayout();
    m_scrollLayer->updateLayout();
    m_scrollLayer->moveToTop();
}

void NXRHackSettingsPopup::prepareNewRow() {
    m_currentRow = NXRMenu::create();
    m_currentRow->setContentSize({230.f, 32.f});
    m_currentRow->setLayout(
        geode::RowLayout::create()
            ->setGap(8.f)
            ->setAxisAlignment(geode::AxisAlignment::Center)
            ->setCrossAxisAlignment(geode::AxisAlignment::Center)
            ->setAutoScale(false)
    );
    m_scrollLayer->m_contentLayer->addChild(m_currentRow);
}

void NXRHackSettingsPopup::addConfigToggle(const std::string& labelText, const std::string& key, bool defaultValue, geode::Function<void(bool)> callback) {
    auto& config = NXRConfig::get();
    prepareNewRow();

    auto toggle = CCMenuItemExt::createTogglerWithFilename("NXR_togglerOn.png"_spr, "NXR_togglerOff.png"_spr, 0.8f, [key, callback = std::move(callback)](CCMenuItemToggler* sender) mutable {
        bool newValue = !sender->isOn();
        NXRConfig::get().set<bool>(key, newValue);
        if (callback) {
            callback(newValue);
        }
    });
    toggle->toggle(config.get<bool>(key, defaultValue));
    m_currentRow->addChild(toggle);

    auto label = geode::Label::create(labelText, "GoogleSans.fnt"_spr);
    label->setScale(0.55f);
    if (label->getScaledContentSize().width > 160.f) {
        label->setScale(160.f / label->getContentSize().width);
    }
    m_currentRow->addChild(label);

    m_currentRow->updateLayout();
    updatePopupRowAlignment(m_currentRow);
}

NXROptionPopup* NXROptionPopup::create(const std::string& title, const std::vector<std::pair<std::string, int>>& options, int current, geode::Function<void(int)> onPick) {
    auto ret = new NXROptionPopup();
    if (ret->init(title, options, current, std::move(onPick))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool NXROptionPopup::init(const std::string& title, const std::vector<std::pair<std::string, int>>& options, int current, geode::Function<void(int)> onPick) {
    const float rowHeight = 34.f;
    const float height = std::clamp(70.f + rowHeight * static_cast<float>(options.size()), 120.f, 250.f);
    if (!geode::Popup::init(240.f, height, NXR::Theme::square())) return false;

    m_options = options;
    m_current = current;
    m_onPick = std::move(onPick);
    auto size = m_mainLayer->getContentSize();

    auto titleLabel = geode::Label::create(title, "GoogleSans.fnt"_spr);
    titleLabel->setPosition({size.width / 2.f, size.height - 18.f});
    titleLabel->setScale(0.6f);
    if (titleLabel->getScaledContentSize().width > 190.f) titleLabel->setScale(190.f / titleLabel->getContentSize().width);
    m_mainLayer->addChild(titleLabel);

    auto closeSprite = cocos2d::CCSprite::create("NXR_closeBtn.png"_spr);
    closeSprite->setScale(0.75f);
    m_closeBtn->setSprite(closeSprite);

    auto* scroll = geode::prelude::ScrollLayer::create({220.f, height - 50.f});
    scroll->setPosition({size.width / 2.f - 110.f, 12.f});
    scroll->m_contentLayer->setLayout(
        geode::ColumnLayout::create()
            ->setAutoScale(false)
            ->setAxisReverse(true)
            ->setAutoGrowAxis(true)
            ->setGap(6.f)
    );
    m_mainLayer->addChild(scroll);

    for (auto const& [text, value] : m_options) {
        auto* row = NXRMenu::create();
        row->setContentSize({210.f, 30.f});
        row->setLayout(
            geode::RowLayout::create()
                ->setAxisAlignment(geode::AxisAlignment::Center)
                ->setCrossAxisAlignment(geode::AxisAlignment::Center)
                ->setAutoScale(false)
        );

        const bool selected = value == m_current;
        auto sprite = ButtonSprite::create(text.c_str(), 190, true, "GoogleSans.fnt"_spr, selected ? NXR::Theme::buttonOn() : NXR::Theme::button(), 26.f, 0.6f);
        sprite->m_label->setColor(NXR::Ui::textColor(selected ? cocos2d::ccColor3B({255, 236, 179}) : cocos2d::ccColor3B({255, 255, 255})));
        auto button = geode::cocos::CCMenuItemExt::createSpriteExtra(sprite, [this, value](CCMenuItemSpriteExtra*) {
            if (m_onPick) m_onPick(value);
            this->onClose(nullptr);
        });
        row->addChild(button);
        row->updateLayout();
        scroll->m_contentLayer->addChild(row);
    }

    scroll->m_contentLayer->updateLayout();
    scroll->moveToTop();
    NXR::Ui::applyTextStyle(m_mainLayer);
    return true;
}

void NXRHackSettingsPopup::addConfigSelect(const std::string& labelText, const std::string& key, const std::vector<std::pair<std::string, int>>& options, int defaultValue, geode::Function<void(int)> callback) {
    prepareNewRow();
    m_currentRow->setContentSize({230.f, 46.f});

    auto nameOf = [options](int value) {
        for (auto const& [text, v] : options) if (v == value) return text;
        return options.empty() ? std::string() : options.front().first;
    };

    const int current = NXRConfig::get().get<int>(key, defaultValue);

    auto label = geode::Label::create(fmt::format("{}: {}", labelText, nameOf(current)), "GoogleSans.fnt"_spr);
    label->setScale(0.55f);
    if (label->getScaledContentSize().width > 140.f) label->setScale(140.f / label->getContentSize().width);
    geode::Ref<geode::Label> labelRef(label);

    auto arrowIcon = cocos2d::CCSprite::create("NXR_arrowRight.png"_spr);
    arrowIcon->setScale(0.42f);
    auto arrow = cocos2d::CCNode::create();
    arrow->setContentSize({54.f, 46.f});
    arrowIcon->setPosition({27.f, 23.f});
    arrow->addChild(arrowIcon);

    auto shared = std::make_shared<geode::Function<void(int)>>(std::move(callback));
    auto button = geode::cocos::CCMenuItemExt::createSpriteExtra(arrow, [this, labelText, key, defaultValue, options, nameOf, labelRef, shared](CCMenuItemSpriteExtra*) {
        const int now = NXRConfig::get().get<int>(key, defaultValue);
        NXROptionPopup::create(labelText, options, now, [key, labelText, nameOf, labelRef, shared](int value) {
            NXRConfig::get().set<int>(key, value);
            if (auto* text = labelRef.data()) {
                text->setString(fmt::format("{}: {}", labelText, nameOf(value)).c_str());
                text->setScale(0.55f);
                if (text->getScaledContentSize().width > 140.f) text->setScale(140.f / text->getContentSize().width);
            }
            if (*shared) (*shared)(value);
        })->show();
    });

    m_currentRow->addChild(label);
    m_currentRow->addChild(button);
    m_currentRow->updateLayout();
    updatePopupRowAlignment(m_currentRow);
}

void NXRHackSettingsPopup::addConfigModeToggle(const std::string& key, const std::string& offText, const std::string& onText, int defaultValue, geode::Function<void(int)> callback) {
    addConfigSelect("Mode", key, {{offText, 1}, {onText, 2}}, defaultValue, std::move(callback));
}

void NXRHackSettingsPopup::addConfigRadio(const std::string& labelText, const std::string& key, const std::vector<std::pair<std::string, int>>& options, int defaultValue, geode::Function<void(int)> callback) {
    addConfigSelect(labelText, key, options, defaultValue, std::move(callback));
}

void NXRHackSettingsPopup::addConfigIntInput(const std::string& labelText, const std::string& key, int min, int max, int defaultValue, geode::Function<void(int)> callback) {
    auto& config = NXRConfig::get();
    prepareNewRow();

    auto input = geode::TextInput::create(50.f, "Val", "GoogleSans.fnt"_spr);
    if (auto* bg = input->getChildByType<geode::NineSlice>(0)) {
        bg->setColor({34, 33, 46});
        bg->setOpacity(255);
    }
    input->setScale(0.65f);
    input->setString(std::to_string(config.get<int>(key, defaultValue)));
    input->setFilter("0123456789-");
    input->setMaxCharCount(8);

    input->setCallback([key, min, max, callback = std::move(callback)](const std::string& str) mutable {
        auto value = geode::utils::numFromString<int>(str);
        if (!value.isErr()) {
            int val = std::clamp(value.unwrap(), min, max);
            NXRConfig::get().set<int>(key, val);

            if (callback) {
                callback(val);
            }
        }
    });
    m_currentRow->addChild(input);

    auto label = geode::Label::create(labelText, "GoogleSans.fnt"_spr);
    label->setScale(0.55f);
    if (label->getScaledContentSize().width > 160.f) {
        label->setScale(160.f / label->getContentSize().width);
    }
    m_currentRow->addChild(label);

    m_currentRow->updateLayout();
    updatePopupRowAlignment(m_currentRow);
}

void NXRHackSettingsPopup::addConfigFloatInput(const std::string& labelText, const std::string& key, float min, float max, float defaultValue, geode::Function<void(float)> callback) {
    auto& config = NXRConfig::get();
    prepareNewRow();

    auto input = geode::TextInput::create(50.f, "Val", "GoogleSans.fnt"_spr);
    if (auto* bg = input->getChildByType<geode::NineSlice>(0)) {
        bg->setColor({34, 33, 46});
        bg->setOpacity(255);
    }
    input->setScale(0.65f);
    input->setString(fmt::format("{:.2f}", config.get<float>(key, defaultValue)));
    input->setFilter("0123456789-.");
    input->setMaxCharCount(6);

    input->setCallback([key, min, max, callback = std::move(callback)](const std::string& str) mutable {
        auto value = geode::utils::numFromString<float>(str);
        if (!value.isErr()) {
            float val = std::clamp(value.unwrap(), min, max);
            NXRConfig::get().set<float>(key, val);

            if (callback) {
                callback(val);
            }
        }
    });
    m_currentRow->addChild(input);

    auto label = geode::Label::create(labelText, "GoogleSans.fnt"_spr);
    label->setScale(0.55f);
    if (label->getScaledContentSize().width > 160.f) {
        label->setScale(160.f / label->getContentSize().width);
    }
    m_currentRow->addChild(label);

    m_currentRow->updateLayout();
    updatePopupRowAlignment(m_currentRow);
}

void NXRHackSettingsPopup::addConfigColor3Hex(const std::string& labelText, const std::string& key, const std::string& defaultHex) {
    auto& config = NXRConfig::get();
    prepareNewRow();

    std::string currentHex = config.get<std::string>(key, defaultHex);
    cocos2d::ccColor3B color = NXR::Utils::hexToColor(currentHex);

    auto colorSprite = ColorChannelSprite::create();
    colorSprite->setScale(0.6f);
    colorSprite->setColor(color);

    auto colorBtn = CCMenuItemExt::createSpriteExtra(colorSprite, [this, key, defaultHex, colorSprite](CCMenuItemSpriteExtra*) {
        std::string curHex = NXRConfig::get().get<std::string>(key, defaultHex);
        cocos2d::ccColor3B curColor = NXR::Utils::hexToColor(curHex);

        cocos2d::ccColor4B popupColor = { curColor.r, curColor.g, curColor.b, 255 };

        auto popup = NXRColorPopup::create(popupColor, false);
        popup->setCallback([weakPopup = geode::WeakRef(this), key, colorSprite](cocos2d::ccColor4B const& formatColor) {
            if (!weakPopup.lock()) return;

            std::string newHex = fmt::format("{:02X}{:02X}{:02X}", formatColor.r, formatColor.g, formatColor.b);
            NXRConfig::get().set<std::string>(key, newHex);
            colorSprite->setColor(geode::cocos::to3B(formatColor));
        });
        popup->show();
    });
    m_currentRow->addChild(colorBtn);

    auto label = geode::Label::create(labelText, "GoogleSans.fnt"_spr);
    label->setScale(0.55f);
    if (label->getScaledContentSize().width > 120.f) {
        label->setScale(120.f / label->getContentSize().width);
    }
    m_currentRow->addChild(label);

    auto resetBtnSprite = ButtonSprite::create("Reset", 40, true, "GoogleSans.fnt"_spr, NXR::Theme::button(), 25.f, 0.4f);
    auto resetBtn = CCMenuItemExt::createSpriteExtra(resetBtnSprite, [key, defaultHex, colorSprite](CCMenuItemSpriteExtra*) {
        NXRConfig::get().set<std::string>(key, defaultHex);
        cocos2d::ccColor3B defColor = NXR::Utils::hexToColor(defaultHex);
        colorSprite->setColor(defColor);
    });
    m_currentRow->addChild(resetBtn);

    m_currentRow->updateLayout();
    updatePopupRowAlignment(m_currentRow);
}

void NXRHackSettingsPopup::addConfigColor4Hex(const std::string& labelText, const std::string& key, const std::string& defaultHex) {
    auto& config = NXRConfig::get();
    prepareNewRow();

    std::string currentHex = config.get<std::string>(key, defaultHex);
    cocos2d::ccColor4F color4F = NXR::Utils::hexToColor4F(currentHex);

    cocos2d::ccColor3B color3B = {
        static_cast<GLubyte>(color4F.r * 255.f),
        static_cast<GLubyte>(color4F.g * 255.f),
        static_cast<GLubyte>(color4F.b * 255.f)
    };
    GLubyte alphaByte = static_cast<GLubyte>(color4F.a * 255.f);

    auto colorSprite = ColorChannelSprite::create();
    colorSprite->setScale(0.6f);
    colorSprite->setColor(color3B);
    colorSprite->setOpacity(alphaByte);

    auto colorBtn = CCMenuItemExt::createSpriteExtra(colorSprite, [this, key, defaultHex, colorSprite](CCMenuItemSpriteExtra*) {
        std::string curHex = NXRConfig::get().get<std::string>(key, defaultHex);
        cocos2d::ccColor4F cur4F = NXR::Utils::hexToColor4F(curHex);

        cocos2d::ccColor4B popupColor = {
            static_cast<GLubyte>(cur4F.r * 255.f),
            static_cast<GLubyte>(cur4F.g * 255.f),
            static_cast<GLubyte>(cur4F.b * 255.f),
            static_cast<GLubyte>(cur4F.a * 255.f)
        };

        auto popup = NXRColorPopup::create(popupColor, true);
        popup->setCallback([weakPopup = geode::WeakRef(this), key, colorSprite](cocos2d::ccColor4B const& formatColor) {
            if (!weakPopup.lock()) return;

            std::string newHex = fmt::format("{:02X}{:02X}{:02X}{:02X}", formatColor.r, formatColor.g, formatColor.b, formatColor.a);
            NXRConfig::get().set<std::string>(key, newHex);

            colorSprite->setColor(geode::cocos::to3B(formatColor));
            colorSprite->setOpacity(formatColor.a);
        });
        popup->show();
    });
    m_currentRow->addChild(colorBtn);

    auto label = geode::Label::create(labelText, "GoogleSans.fnt"_spr);
    label->setScale(0.55f);
    if (label->getScaledContentSize().width > 120.f) {
        label->setScale(120.f / label->getContentSize().width);
    }
    m_currentRow->addChild(label);

    auto resetBtnSprite = ButtonSprite::create("Reset", 40, true, "GoogleSans.fnt"_spr, NXR::Theme::button(), 25.f, 0.4f);
    auto resetBtn = CCMenuItemExt::createSpriteExtra(resetBtnSprite, [key, defaultHex, colorSprite](CCMenuItemSpriteExtra*) {
        NXRConfig::get().set<std::string>(key, defaultHex);
        cocos2d::ccColor4F def4F = NXR::Utils::hexToColor4F(defaultHex);

        colorSprite->setColor({
            static_cast<GLubyte>(def4F.r * 255.f),
            static_cast<GLubyte>(def4F.g * 255.f),
            static_cast<GLubyte>(def4F.b * 255.f)
        });
        colorSprite->setOpacity(static_cast<GLubyte>(def4F.a * 255.f));
    });
    m_currentRow->addChild(resetBtn);

    m_currentRow->updateLayout();
    updatePopupRowAlignment(m_currentRow);
}

void NXRHackSettingsPopup::addSeparator(float height) {
    m_currentRow = nullptr;
    auto node = cocos2d::CCLayerColor::create({181, 105, 56, 80});
    node->setContentSize({230.f, height});
    m_scrollLayer->m_contentLayer->addChild(node);
}
