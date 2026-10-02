#include "../../core/nxr_ui_mode.hpp"
#include "../../core/nxr_theme.hpp"
#include "nxr_menu.hpp"
#include <Geode/ui/Scrollbar.hpp>
#include "nxr_hacks_tab.hpp"
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_config.hpp"
#include "nxr_hack_settings_popup.hpp"
#include "nxr_bot_popups.hpp"
#include "../../core/nxr_utils.hpp"
#include <memory>

namespace {
    constexpr float kColumnWidth = 175.f;
    constexpr float kColumnPadRight = 8.f;

    void fitLabelToColumn(geode::Label* label) {
        if (!label) return;
        const float available = kColumnWidth - label->getPositionX() - kColumnPadRight;
        const float width = label->getScaledContentWidth();
        if (available > 0.f && width > available) {
            label->setScale(label->getScale() * (available / width));
        }
    }
}

NXRHacksTab* NXRHacksTab::create() {
    auto ret = new NXRHacksTab();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

void updateRowAlignment(cocos2d::CCMenu* row) {
    if (!row) return;
    if (auto layout = static_cast<geode::RowLayout*>(row->getLayout())) {

        layout->setAxisAlignment(geode::AxisAlignment::Start);
        row->updateLayout();
    }
}

void NXRHacksTab::addToggle(NXR::Hack& hck) {
    auto& gui = NXR::Gui::get();
    const std::string ID = hck.getID();

    float columnWidth = 175.f;
    bool disabled = hck.getDisabled();

    auto hackNode = NXRMenu::create();
    hackNode->setContentSize({columnWidth, 30.f});
    hackNode->setAnchorPoint({0, 0.5f});

    auto toggle = NXR::Ui::makeToggler(0.8f, [&gui, ID](CCMenuItemToggler* sender) {
        auto* hack = gui.findHackByIDGlobal(ID);
        hack->toggle();
    });
    toggle->setPosition({25.f, 15.f});
    toggle->toggle(hck.getEnabled());

    if (disabled) {
        toggle->setEnabled(false);
        toggle->setOpacity(100);
    }

    hackNode->addChild(toggle);

    std::string name = hck.getName();
    auto label = geode::Label::create(name, "GoogleSans.fnt"_spr);
    label->setAnchorPoint({0.f, 0.5f});
    label->setScale(0.65f);
    label->setPosition({toggle->getPositionX() + 22.f, 15.f});
    if (disabled) label->setOpacity(100);
    if (hck.isCheating()) label->setColor({255, 128, 128});

    float maxLabelWidth = columnWidth - label->getPositionX() - 45.f;
    if (label->getScaledContentWidth() > maxLabelWidth) {
        label->setScale(label->getScale() * (maxLabelWidth / label->getScaledContentWidth()));
    }
    hackNode->addChild(label);

    float iconXOffset = label->getPositionX() + label->getScaledContentWidth() + 13.f;
    if (hck.avaibleCustomWindowCocos()) {
        auto customSettingsBtn = CCSprite::create("NXR_settingsBtn.png"_spr);
        customSettingsBtn->setScale(0.45f);

        auto customSettingsBtnClick = CCMenuItemExt::createSpriteExtra(customSettingsBtn, [&hck](CCMenuItemSpriteExtra* sender) {
            NXRHackSettingsPopup::open(hck);
        });

        customSettingsBtnClick->setPosition({iconXOffset, 15.f});
        hackNode->addChild(customSettingsBtnClick);
        iconXOffset += 18.5f;
    }

    std::string desc = hck.getDesc();
    if (!desc.empty()) {
        auto descSprite = CCSprite::create("NXR_infoIcon.png"_spr);
        descSprite->setScale(0.5f);
        auto descClick = CCMenuItemExt::createSpriteExtra(descSprite, [name, desc](CCMenuItemSpriteExtra* sender) {
            NXR::Ui::showPopup(NXRInfoPopup::create(name, desc), name);
        });

        descClick->setPosition({iconXOffset, 15.f});
        hackNode->addChild(descClick);
    }

    if (!m_currentRow || m_currentRow->getChildrenCount() >= 2) {
        prepareNewRow();
    }

    m_currentRow->addChild(hackNode);
    updateRowAlignment(m_currentRow);
}

bool NXRHacksTab::init() {
    if (!CCMenu::init())
        return false;

    setPosition({0, 0});

    constexpr float kContentWidth = 360.f;
    m_scrollLayer = ScrollLayer::create({kContentWidth, 250.f});
    m_scrollLayer->m_contentLayer->setLayout(
        geode::ColumnLayout::create()
            ->setAutoScale(false)
            ->setAxisReverse(true)
            ->setAutoGrowAxis(false)
            ->setGap(0.f)
    );
    m_scrollLayer->setPosition({107.f, 5.f});
    m_scrollLayer->m_peekLimitTop = 15;
    m_scrollLayer->m_peekLimitBottom = 15;

    addChild(m_scrollLayer);

    return true;
}

void NXRHacksTab::addPadding(float height) {
    m_currentRow = nullptr;
    auto node = cocos2d::CCNode::create();
    node->setContentHeight(height);
    m_scrollLayer->m_contentLayer->addChild(node);
}

void NXRHacksTab::addSeparator(float height) {
    m_currentRow = nullptr;
    auto node = cocos2d::CCLayerColor::create({181, 105, 56, 80});
    node->setContentSize({335.f, height});
    m_scrollLayer->m_contentLayer->addChild(node);
}

void NXRHacksTab::prepareNewRow() {
    float columnWidth = 175.f;
    m_currentRow = NXRMenu::create();
    m_currentRow->setContentSize({columnWidth * 2 + 5.f, 32.f});
    m_currentRow->setLayout(
        geode::RowLayout::create()
            ->setGap(0.f)
            ->setAxisAlignment(geode::AxisAlignment::Start)
            ->setCrossAxisAlignment(geode::AxisAlignment::Center)
            ->setAutoScale(false)
    );
    m_scrollLayer->m_contentLayer->addChild(m_currentRow);
}

void NXRHacksTab::addHackToggle(const std::string& labelText, const std::string& key, bool defaultValue, geode::Function<void(bool)> callback) {
    auto& config = NXRConfig::get();
    float columnWidth = 175.f;

    auto node = NXRMenu::create();
    node->setContentSize({columnWidth, 30.f});
    node->setAnchorPoint({0, 0.5f});

    auto toggle = NXR::Ui::makeToggler(0.8f, [key, callback = std::move(callback)](CCMenuItemToggler* sender) mutable {
        auto& gui = NXR::Gui::get();
        auto* hack = gui.findHackByIDGlobal(key);
        if (hack != nullptr)
            hack->toggle();

        if (callback) {
            callback(hack->getEnabled());
        }
    });
    toggle->setPosition({25.f, 15.f});
    toggle->toggle(config.get<bool>(key, defaultValue));
    node->addChild(toggle);

    auto label = geode::Label::create(labelText, "GoogleSans.fnt"_spr);
    label->setAnchorPoint({0.f, 0.5f});
    label->setScale(0.65f);
    label->setPosition({toggle->getPositionX() + 22.f, 15.f});
    fitLabelToColumn(label);
    node->addChild(label);

    if (!m_currentRow || m_currentRow->getChildrenCount() >= 2) prepareNewRow();
    m_currentRow->addChild(node);
    updateRowAlignment(m_currentRow);
}

void NXRHacksTab::addConfigToggle(
    const std::string& labelText,
    const std::string& key,
    bool defaultValue,
    geode::Function<void(bool)> callback
) {
    auto& config = NXRConfig::get();
    float columnWidth = 175.f;

    auto node = NXRMenu::create();
    node->setContentSize({columnWidth, 30.f});
    node->setAnchorPoint({0, 0.5f});

    auto toggle = NXR::Ui::makeToggler(0.8f,
        [key, callback = std::move(callback)](CCMenuItemToggler* sender) mutable {
            bool newValue = !sender->isOn();
            NXRConfig::get().set<bool>(key, newValue);
            if (callback) {
                callback(newValue);
            }
        }
    );
    toggle->setPosition({25.f, 15.f});
    toggle->toggle(config.get<bool>(key, defaultValue));
    node->addChild(toggle);

    auto label = geode::Label::create(labelText, "GoogleSans.fnt"_spr);
    label->setAnchorPoint({0.f, 0.5f});
    label->setScale(0.65f);
    label->setPosition({toggle->getPositionX() + 22.f, 15.f});
    fitLabelToColumn(label);
    node->addChild(label);

    if (!m_currentRow || m_currentRow->getChildrenCount() >= 2) prepareNewRow();
    m_currentRow->addChild(node);
    updateRowAlignment(m_currentRow);
}

void NXRHacksTab::addConfigIntInput(const std::string& labelText, const std::string& key, int defaultValue, int min, int max, geode::Function<void(int)> callback) {
    auto& config = NXRConfig::get();
    float columnWidth = 175.f;

    auto node = NXRMenu::create();
    node->setContentSize({columnWidth, 30.f});

    auto input = geode::TextInput::create(60.f, "Val", "GoogleSans.fnt"_spr);
    if (auto* bg = input->getChildByType<geode::NineSlice>(0)) {
        bg->setColor({34, 33, 46});
        bg->setOpacity(255);
    }
    input->setScale(0.7f);
    input->setPosition({30.f, 15.f});
    input->setString(std::to_string(config.get<int>(key, defaultValue)));
    input->setFilter("0123456789-");
    input->setMaxCharCount(8);

    input->setCallback([key, callback = std::move(callback), min, max](const std::string& str) mutable {
        auto value = geode::utils::numFromString<int>(str);
        if (!value.isErr()) {
            int val = std::clamp(value.unwrap(), min, max);
            NXRConfig::get().set<int>(key, val);
            if (callback) callback(val);
        }
    });
    node->addChild(input);

    auto label = geode::Label::create(labelText, "GoogleSans.fnt"_spr);
    label->setAnchorPoint({0.f, 0.5f});
    label->setScale(0.55f);
    label->setPosition({60.f, 15.f});
    fitLabelToColumn(label);
    node->addChild(label);

    if (!m_currentRow || m_currentRow->getChildrenCount() >= 2) prepareNewRow();
    m_currentRow->addChild(node);
    updateRowAlignment(m_currentRow);
}

void NXRHacksTab::addConfigFloatInput(const std::string& labelText, const std::string& key, float defaultValue, float min, float max, geode::Function<void(float)> callback) {
    auto& config = NXRConfig::get();
    float columnWidth = 175.f;

    auto node = NXRMenu::create();
    node->setContentSize({columnWidth, 30.f});

    auto input = geode::TextInput::create(60.f, "Val", "GoogleSans.fnt"_spr);
    if (auto* bg = input->getChildByType<geode::NineSlice>(0)) {
        bg->setColor({34, 33, 46});
        bg->setOpacity(255);
    }
    input->setScale(0.7f);
    input->setPosition({30.f, 15.f});
    input->setString(fmt::format("{:.2f}", config.get<float>(key, defaultValue)));
    input->setFilter("0123456789-.");
    input->setMaxCharCount(8);

    input->setCallback([key, callback = std::move(callback), min, max](const std::string& str) mutable {
        auto value = geode::utils::numFromString<float>(str);
        if (!value.isErr()) {
            float val = std::clamp(value.unwrap(), min, max);
            NXRConfig::get().set<float>(key, val);
            if (callback) callback(val);
        }
    });
    node->addChild(input);

    auto label = geode::Label::create(labelText, "GoogleSans.fnt"_spr);
    label->setAnchorPoint({0.f, 0.5f});
    label->setScale(0.55f);
    label->setPosition({60.f, 15.f});
    fitLabelToColumn(label);
    node->addChild(label);

    if (!m_currentRow || m_currentRow->getChildrenCount() >= 2) prepareNewRow();
    m_currentRow->addChild(node);
    updateRowAlignment(m_currentRow);
}

void NXRHacksTab::addConfigColor3Hex(const std::string& labelText, const std::string& key, const std::string& defaultHex) {
    auto& config = NXRConfig::get();
    float columnWidth = 175.f;

    auto node = NXRMenu::create();
    node->setContentSize({columnWidth, 30.f});

    cocos2d::ccColor3B color = NXR::Utils::hexToColor(config.get<std::string>(key, defaultHex));

    auto colorSprite = ColorChannelSprite::create();
    colorSprite->setScale(0.6f);
    colorSprite->setColor(color);

    auto colorBtn = CCMenuItemExt::createSpriteExtra(colorSprite, [key, defaultHex, colorSprite, labelText](CCMenuItemSpriteExtra*) {
        cocos2d::ccColor3B cur = NXR::Utils::hexToColor(NXRConfig::get().get<std::string>(key, defaultHex));
        cocos2d::ccColor4B popupColor = { cur.r, cur.g, cur.b, 255 };

        auto popup = NXRColorPopup::create(popupColor, false);
        popup->setCallback([key, sprite = geode::Ref<ColorChannelSprite>(colorSprite)](cocos2d::ccColor4B const& picked) {
            NXRConfig::get().set<std::string>(key, fmt::format("{:02X}{:02X}{:02X}", picked.r, picked.g, picked.b));
            if (sprite) sprite->setColor(geode::cocos::to3B(picked));
        });
        NXR::Ui::showPopup(popup, labelText);
    });
    colorBtn->setPosition({18.f, 15.f});
    node->addChild(colorBtn);

    auto label = geode::Label::create(labelText, "GoogleSans.fnt"_spr);
    label->setAnchorPoint({0.f, 0.5f});
    label->setScale(0.55f);
    label->setPosition({40.f, 15.f});
    fitLabelToColumn(label);
    node->addChild(label);

    if (!m_currentRow || m_currentRow->getChildrenCount() >= 2) prepareNewRow();
    m_currentRow->addChild(node);
    updateRowAlignment(m_currentRow);
}

void NXRHacksTab::addText(const std::string& text, float scale) {
    auto label = geode::Label::create(text, "GoogleSans.fnt"_spr);
    label->setScale(scale);

    prepareNewRow();
    m_currentRow->addChild(label);

    if (auto layout = static_cast<geode::RowLayout*>(m_currentRow->getLayout())) {
        layout->setAxisAlignment(geode::AxisAlignment::Center);
        m_currentRow->updateLayout();
    }
}

void NXRHacksTab::addConfigButton(const std::string& labelText, geode::Function<void()> callback, const std::string& secondLabelText, geode::Function<void()> secondCallback) {
    float columnWidth = 170.f;
    bool dualMode = !secondLabelText.empty() && secondCallback != nullptr;

    prepareNewRow();

    if (dualMode) {
        auto createHalfNode = [columnWidth](const std::string& text, geode::Function<void()> cb) -> CCMenu* {
            auto node = NXRMenu::create();
            node->setContentSize({columnWidth, 30.f});
            node->setAnchorPoint({0, 0.5f});

            auto btnSprite = ButtonSprite::create(text.c_str(), 140, true, "GoogleSans.fnt"_spr, NXR::Theme::button(), 25.f, 0.6f);
            auto btnClick = CCMenuItemExt::createSpriteExtra(btnSprite, [cb = std::move(cb)](CCMenuItemSpriteExtra* sender) mutable {
                if (cb) cb();
            });
            btnClick->setPosition({columnWidth / 2.f - 5.f, 15.f});
            node->addChild(btnClick);
            return node;
        };

        m_currentRow->addChild(createHalfNode(labelText, std::move(callback)));
        m_currentRow->addChild(createHalfNode(secondLabelText, std::move(secondCallback)));
    }
    else {
        auto node = NXRMenu::create();
        node->setContentSize({columnWidth * 2, 30.f});
        node->setAnchorPoint({0, 0.5f});

        auto btnSprite = ButtonSprite::create(labelText.c_str(), 315, true, "GoogleSans.fnt"_spr, NXR::Theme::button(), 25.f, 0.6f);
        auto btnClick = CCMenuItemExt::createSpriteExtra(btnSprite, [callback = std::move(callback)](CCMenuItemSpriteExtra* sender) mutable {
            if (callback) callback();
        });

        btnClick->setPosition({columnWidth - 5.f, 15.f});
        node->addChild(btnClick);

        m_currentRow->addChild(node);
    }

    if (auto layout = static_cast<geode::RowLayout*>(m_currentRow->getLayout())) {
        layout->setAxisAlignment(geode::AxisAlignment::Center);
        m_currentRow->updateLayout();
    }

    m_currentRow = nullptr;
}

geode::Label* NXRHacksTab::AddTextToToggle(const char *str, CCMenuItemToggler* toggler, float x_space) {
    auto label = geode::Label::create(str, "GoogleSans.fnt"_spr);
    label->setAnchorPoint({0.f, 0.5f});
    label->setPosition({toggler->getPositionX() + x_space, toggler->getPositionY()});
    label->setScale(0.65f);
    return label;
}

void NXRHacksTab::addRadioRow(const std::vector<std::string>& labels, geode::Function<int()> getCurrent, geode::Function<int(int)> onSelect) {
    prepareNewRow();

    const float cellWidth = 355.f / std::max<size_t>(labels.size(), 1);
    auto togglers = std::make_shared<std::vector<geode::Ref<CCMenuItemToggler>>>();
    auto select = std::make_shared<geode::Function<int(int)>>(std::move(onSelect));
    int current = getCurrent ? getCurrent() : 0;

    for (size_t i = 0; i < labels.size(); i++) {
        auto cell = NXRMenu::create();
        cell->setContentSize({cellWidth, 30.f});
        cell->setAnchorPoint({0, 0.5f});

        int index = static_cast<int>(i);
        auto toggle = CCMenuItemExt::createTogglerWithFilename("NXR_tableCheckOn.png"_spr, "NXR_tableCheckOff.png"_spr, 0.35f, [index, togglers, select](CCMenuItemToggler*) {
            int applied = (*select)(index);
            geode::queueInMainThread([togglers, applied] {
                for (size_t k = 0; k < togglers->size(); k++) {
                    if (auto* node = (*togglers)[k].data()) node->toggle(static_cast<int>(k) == applied);
                }
            });
        });
        toggle->setPosition({22.f, 15.f});
        toggle->toggle(index == current);
        togglers->push_back(geode::Ref<CCMenuItemToggler>(toggle));
        cell->addChild(toggle);

        auto label = geode::Label::create(labels[i], "GoogleSans.fnt"_spr);
        label->setAnchorPoint({0.f, 0.5f});
        label->setScale(0.6f);
        label->setPosition({46.f, 15.f});
        float maxWidth = cellWidth - 50.f;
        if (label->getScaledContentWidth() > maxWidth) label->setScale(label->getScale() * (maxWidth / label->getScaledContentWidth()));
        cell->addChild(label);

        m_currentRow->addChild(cell);
    }

    updateRowAlignment(m_currentRow);
    m_currentRow = nullptr;
}

void NXRHacksTab::addSelector(const std::string& title, geode::Function<std::string()> getText, geode::Function<void(std::function<void()>)> onOpen) {
    prepareNewRow();

    auto node = NXRMenu::create();
    node->setContentSize({355.f, 30.f});
    node->setAnchorPoint({0, 0.5f});

    auto titleLabel = geode::Label::create(title, "GoogleSans.fnt"_spr);
    titleLabel->setAnchorPoint({0.f, 0.5f});
    titleLabel->setScale(0.65f);
    titleLabel->setPosition({14.f, 15.f});
    node->addChild(titleLabel);

    auto text = std::make_shared<geode::Function<std::string()>>(std::move(getText));
    auto open = std::make_shared<geode::Function<void(std::function<void()>)>>(std::move(onOpen));

    auto sprite = ButtonSprite::create((*text)().c_str(), 168, true, "GoogleSans.fnt"_spr, NXR::Theme::button(), 26.f, 0.6f);
    geode::Ref<ButtonSprite> spriteRef(sprite);

    std::function<void()> refresh = [spriteRef, text]() {
        auto* image = spriteRef.data();
        if (!image) return;
        image->setString((*text)().c_str());
        if (image->m_label && image->m_label->getScaledContentWidth() > 140.f) {
            image->m_label->setScale(image->m_label->getScale() * (140.f / image->m_label->getScaledContentWidth()));
        }
    };
    refresh();

    auto button = CCMenuItemExt::createSpriteExtra(sprite, [open, refresh](CCMenuItemSpriteExtra*) {
        (*open)(refresh);
    });
    button->setPosition({250.f, 15.f});
    node->addChild(button);

    m_currentRow->addChild(node);
    updateRowAlignment(m_currentRow);
    m_currentRow = nullptr;
}

namespace {
    class NXRKeybindRow : public NXRMenu {
    public:
        static NXRKeybindRow* create(const std::string& label, geode::Function<std::string()> getText, geode::Function<void()> onSet, geode::Function<void()> onClear) {
            auto* ret = new NXRKeybindRow();
            if (ret->setup(label, std::move(getText), std::move(onSet), std::move(onClear))) {
                ret->autorelease();
                return ret;
            }
            delete ret;
            return nullptr;
        }

        void update(float dt) override {
            if (!m_sprite || !m_getText) return;

            const std::string text = m_getText();
            if (text == m_shown) return;
            m_shown = text;
            this->applyText(text);
        }

    private:
        geode::Ref<ButtonSprite> m_sprite;
        geode::Function<std::string()> m_getText;
        std::string m_shown;

        void applyText(const std::string& text) {
            auto* image = m_sprite.data();
            if (!image) return;

            image->setString(text.c_str());
            if (image->m_label && image->m_label->getScaledContentWidth() > 88.f) {
                image->m_label->setScale(image->m_label->getScale() * (88.f / image->m_label->getScaledContentWidth()));
            }
        }

        bool setup(const std::string& label, geode::Function<std::string()> getText, geode::Function<void()> onSet, geode::Function<void()> onClear) {
            if (!CCMenu::init()) return false;

            this->setContentSize({355.f, 30.f});
            this->setAnchorPoint({0.f, 0.5f});

            m_getText = std::move(getText);

            auto* title = geode::Label::create(label, "GoogleSans.fnt"_spr);
            title->setAnchorPoint({0.f, 0.5f});
            title->setScale(0.6f);
            title->setPosition({10.f, 15.f});
            const float maxTitle = 150.f;
            if (title->getScaledContentWidth() > maxTitle) {
                title->setScale(title->getScale() * (maxTitle / title->getScaledContentWidth()));
            }
            this->addChild(title);

            auto* keySprite = ButtonSprite::create("None", 96, true, "GoogleSans.fnt"_spr, NXR::Theme::button(), 24.f, 0.6f);
            m_sprite = geode::Ref<ButtonSprite>(keySprite);

            auto setCallback = std::make_shared<geode::Function<void()>>(std::move(onSet));
            auto* keyButton = CCMenuItemExt::createSpriteExtra(keySprite, [setCallback](CCMenuItemSpriteExtra*) {
                if (*setCallback) (*setCallback)();
            });
            keyButton->setPosition({245.f, 15.f});
            this->addChild(keyButton);

            auto* clearSprite = ButtonSprite::create("Clear", 46, true, "GoogleSans.fnt"_spr, NXR::Theme::button(), 24.f, 0.55f);
            auto clearCallback = std::make_shared<geode::Function<void()>>(std::move(onClear));
            auto* clearButton = CCMenuItemExt::createSpriteExtra(clearSprite, [clearCallback](CCMenuItemSpriteExtra*) {
                if (*clearCallback) (*clearCallback)();
            });
            clearButton->setPosition({322.f, 15.f});
            this->addChild(clearButton);

            m_shown = m_getText ? m_getText() : std::string("None");
            this->applyText(m_shown);
            this->scheduleUpdate();
            return true;
        }
    };
}

void NXRHacksTab::addKeybindRow(const std::string& label, geode::Function<std::string()> getText, geode::Function<void()> onSet, geode::Function<void()> onClear) {
    prepareNewRow();

    auto* row = NXRKeybindRow::create(label, std::move(getText), std::move(onSet), std::move(onClear));
    if (row) m_currentRow->addChild(row);

    updateRowAlignment(m_currentRow);
    m_currentRow = nullptr;
}
