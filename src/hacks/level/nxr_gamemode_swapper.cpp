#include "../../interface/cocos/nxr_modal.hpp"
#include "../../interface/cocos/nxr_ui_kit.hpp"
#include "../../core/nxr_ui_mode.hpp"
#include "../../core/nxr_theme.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include "../../core/nxr_nox_utils.hpp"
#include <array>
#include <memory>
#include <vector>

using namespace geode::prelude;

namespace {

    bool g_portalsEnabled = true;

    constexpr int kModeCount = 8;
    constexpr int kSpeedCount = 5;

    constexpr std::array<float, kSpeedCount> kSpeedValues = {0.7f, 0.9f, 1.1f, 1.3f, 1.6f};

    constexpr std::array<const char*, kModeCount> kModeOn = {
        "gj_iconBtn_on_001.png", "gj_shipBtn_on_001.png", "gj_ballBtn_on_001.png", "gj_birdBtn_on_001.png",
        "gj_dartBtn_on_001.png", "gj_robotBtn_on_001.png", "gj_spiderBtn_on_001.png", "gj_swingBtn_on_001.png"
    };

    constexpr std::array<const char*, kModeCount> kModeOff = {
        "gj_iconBtn_off_001.png", "gj_shipBtn_off_001.png", "gj_ballBtn_off_001.png", "gj_birdBtn_off_001.png",
        "gj_dartBtn_off_001.png", "gj_robotBtn_off_001.png", "gj_spiderBtn_off_001.png", "gj_swingBtn_off_001.png"
    };

    constexpr std::array<const char*, kSpeedCount> kSpeedSprites = {
        "boost_01_001.png", "boost_02_001.png", "boost_03_001.png", "boost_04_001.png", "boost_05_001.png"
    };

    bool isPortalType(GameObjectType type) {
        return type == GameObjectType::CubePortal || type == GameObjectType::ShipPortal
            || type == GameObjectType::BallPortal || type == GameObjectType::UfoPortal
            || type == GameObjectType::WavePortal || type == GameObjectType::RobotPortal
            || type == GameObjectType::SpiderPortal || type == GameObjectType::SwingPortal
            || type == GameObjectType::TeleportPortal || type == GameObjectType::DualPortal
            || type == GameObjectType::GravityTogglePortal || type == GameObjectType::NormalMirrorPortal
            || type == GameObjectType::InverseMirrorPortal || type == GameObjectType::InverseGravityPortal
            || type == GameObjectType::NormalGravityPortal || type == GameObjectType::MiniSizePortal
            || type == GameObjectType::RegularSizePortal || type == GameObjectType::SoloPortal;
    }

    int currentMode(PlayerObject* p) {
        if (p->isInNormalMode()) return 0;
        if (p->m_isShip) return 1;
        if (p->m_isBall) return 2;
        if (p->m_isBird) return 3;
        if (p->m_isDart) return 4;
        if (p->m_isRobot) return 5;
        if (p->m_isSpider) return 6;
        if (p->m_isSwing) return 7;
        return -1;
    }

    int currentSpeed(PlayerObject* p) {
        for (int i = 0; i < kSpeedCount; i++) {
            if (p->m_playerSpeed == kSpeedValues[i]) return i;
        }
        return -1;
    }

    std::vector<PlayerObject*> targets(PlayLayer* pl) {
        std::vector<PlayerObject*> list;
        if (!pl) return list;
        if (pl->m_player1) list.push_back(pl->m_player1);
        if (pl->m_gameState.m_isDualMode && pl->m_player2) list.push_back(pl->m_player2);
        return list;
    }

    void applyMode(PlayLayer* pl, PlayerObject* p, int mode) {
        switch (mode) {
            case 0:
                p->toggleFlyMode(false, false);
                p->toggleRollMode(false, false);
                p->toggleBirdMode(false, false);
                p->toggleDartMode(false, false);
                p->toggleRobotMode(false, false);
                p->toggleSpiderMode(false, false);
                p->toggleSwingMode(false, false);
                break;
            case 1: p->toggleFlyMode(true, true); break;
            case 2: p->toggleRollMode(true, true); break;
            case 3: p->toggleBirdMode(true, true); break;
            case 4: p->toggleDartMode(true, true); break;
            case 5: p->toggleRobotMode(true, true); break;
            case 6: p->toggleSpiderMode(true, true); break;
            case 7: p->toggleSwingMode(true, true); break;
            default: return;
        }

        auto* obj = TeleportPortalObject::create("edit_eGameRotBtn_001.png", true);
        obj->m_cameraIsFreeMode = true;
        pl->playerWillSwitchMode(p, obj);
    }

    class GamemodePopup : public geode::Popup {
    protected:
        int m_mode = -1;
        int m_speed = -1;
        bool m_mini = false;
        bool m_flip = false;
        bool m_reverse = false;
        bool m_safe = false;

        bool m_initialMini = false;
        bool m_initialFlip = false;
        bool m_initialReverse = false;

        std::vector<Ref<CCMenuItemSpriteExtra>> m_modeButtons;
        std::vector<Ref<CCMenuItemSpriteExtra>> m_speedButtons;

        void refreshModes() {
            for (auto& button : m_modeButtons) button->setEnabled(button->getTag() != m_mode);
        }

        void refreshSpeeds() {
            for (auto& button : m_speedButtons) button->setEnabled(button->getTag() != m_speed);
        }

        void addToggle(CCMenu* menu, const char* text, bool value, geode::Function<void(bool)> callback) {
            auto cell = CCMenu::create();
            cell->setContentSize({150.f, 28.f});

            auto toggle = NXR::Ui::makeToggler(0.65f, [callback = std::move(callback)](CCMenuItemToggler* sender) mutable {
                callback(!sender->isOn());
            });
            toggle->setPosition({22.f, 14.f});
            toggle->toggle(value);
            cell->addChild(toggle);

            auto label = geode::Label::create(text, "GoogleSans.fnt"_spr);
            label->setAnchorPoint({0.f, 0.5f});
            label->setScale(0.6f);
            label->setPosition({46.f, 14.f});
            cell->addChild(label);

            menu->addChild(cell);
        }

        bool init() {
            if (!geode::Popup::init(360.f, 260.f, NXR::Theme::square())) return false;

            auto* pl = PlayLayer::get();
            if (!pl || !pl->m_player1) return false;

            auto size = m_mainLayer->getContentSize();
            auto* player = pl->m_player1;

            m_mode = currentMode(player);
            m_speed = currentSpeed(player);
            m_mini = m_initialMini = player->m_vehicleSize == 0.6f;
            m_flip = m_initialFlip = player->m_isUpsideDown;
            m_reverse = m_initialReverse = player->m_isGoingLeft;
            m_safe = !g_portalsEnabled;

            NXR::Modal::skin({m_mainLayer, m_bgSprite, m_closeBtn, m_buttonMenu}, "Gamemode Swapper", size.width, size.height);

            auto modeMenu = CCMenu::create();
            modeMenu->setLayout(RowLayout::create()->setGap(8.f)->setAutoScale(false));
            modeMenu->setContentSize({size.width - 20.f, 44.f});
            for (int i = 0; i < kModeCount; i++) {
                auto normal = CCSprite::createWithSpriteFrameName(kModeOff[i]);
                normal->setScale(0.75f);
                Ref<CCMenuItemSpriteExtra> button = CCMenuItemExt::createSpriteExtra(normal, [this](CCMenuItemSpriteExtra* sender) {
                    m_mode = sender->getTag();
                    refreshModes();
                });
                button->setTag(i);
                auto selected = CCSprite::createWithSpriteFrameName(kModeOn[i]);
                selected->setScale(0.75f);
                button->setDisabledImage(selected);
                m_modeButtons.push_back(button);
                modeMenu->addChild(button);
            }
            modeMenu->updateLayout();
            m_mainLayer->addChildAtPosition(modeMenu, Anchor::Center, ccp(0, 62.f));
            refreshModes();

            auto speedMenu = CCMenu::create();
            speedMenu->setLayout(RowLayout::create()->setGap(14.f)->setAutoScale(false));
            speedMenu->setContentSize({size.width - 20.f, 40.f});
            for (int i = 0; i < kSpeedCount; i++) {
                auto normal = CCSprite::createWithSpriteFrameName(kSpeedSprites[i]);
                normal->setScale(0.8f);
                normal->setOpacity(150);
                Ref<CCMenuItemSpriteExtra> button = CCMenuItemExt::createSpriteExtra(normal, [this](CCMenuItemSpriteExtra* sender) {
                    m_speed = sender->getTag();
                    refreshSpeeds();
                });
                button->setTag(i);
                auto selected = CCSprite::createWithSpriteFrameName(kSpeedSprites[i]);
                selected->setScale(0.8f);
                button->setDisabledImage(selected);
                m_speedButtons.push_back(button);
                speedMenu->addChild(button);
            }
            speedMenu->updateLayout();
            m_mainLayer->addChildAtPosition(speedMenu, Anchor::Center, ccp(0, 14.f));
            refreshSpeeds();

            auto toggles = CCMenu::create();
            toggles->setLayout(RowLayout::create()->setGap(10.f)->setGrowCrossAxis(true)->setAutoScale(false));
            toggles->setContentSize({330.f, 66.f});
            addToggle(toggles, "Mini", m_mini, [this](bool v) { m_mini = v; });
            addToggle(toggles, "Flip Gravity", m_flip, [this](bool v) { m_flip = v; });
            addToggle(toggles, "Reverse", m_reverse, [this](bool v) { m_reverse = v; });
            addToggle(toggles, "Safe Mode", m_safe, [this](bool v) { m_safe = v; });
            toggles->updateLayout();
            m_mainLayer->addChildAtPosition(toggles, Anchor::Center, ccp(0, -44.f));

            auto hint = geode::Label::create("Safe Mode: portals are ignored, you keep the chosen gamemode", "GoogleSans.fnt"_spr);
            hint->setScale(0.38f);
            hint->setColor(NXR::Kit::Pal::muted());
            hint->setPosition({size.width / 2.f, 46.f});
            m_mainLayer->addChild(hint);

            auto okButton = NXR::Modal::button("OK", 120.f, 34.f, true, [this] {
                apply();
                this->onClose(nullptr);
            });
            auto okMenu = CCMenu::create();
            okMenu->setPosition({0.f, 0.f});
            okButton->setPosition({size.width / 2.f, 22.f});
            okMenu->addChild(okButton);
            m_mainLayer->addChild(okMenu);

            return true;
        }

        void apply() {
            g_portalsEnabled = !m_safe;

            auto* pl = PlayLayer::get();
            if (!pl) return;

            for (auto* p : targets(pl)) {
                if (m_mode >= 0 && m_mode != currentMode(p)) applyMode(pl, p, m_mode);
                if (m_speed >= 0) p->m_playerSpeed = kSpeedValues[m_speed];
                if (m_mini != m_initialMini) p->togglePlayerScale(m_mini, true);
                if (m_flip != m_initialFlip) p->flipGravity(m_flip, true);
                if (m_reverse != m_initialReverse) p->doReversePlayer(m_reverse);
            }
        }

    public:
        static GamemodePopup* create() {
            auto* ret = new GamemodePopup();
            if (ret->init()) {
                ret->autorelease();
                return ret;
            }
            delete ret;
            return nullptr;
        }
    };
}

class $modify(NXRGamemodePauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto icon = CCSprite::create("NXR_gamemodeBtn.png"_spr);
        icon->setScale(1.0f);

        auto button = CCMenuItemExt::createSpriteExtra(icon, [](CCMenuItemSpriteExtra*) {
            if (!PlayLayer::get()) return;
            if (auto* popup = GamemodePopup::create()) popup->show();
        });
        button->setID("gamemode-swapper-button"_spr);

        auto* rightMenu = typeinfo_cast<CCMenu*>(this->getChildByID("right-button-menu"));
        auto* noxUtilsButton = NXR::NoxUtils::makePauseButton();

        if (rightMenu) {
            rightMenu->addChild(button);
            rightMenu->addChild(noxUtilsButton);
            rightMenu->updateLayout();
            return;
        }

        auto win = CCDirector::sharedDirector()->getWinSize();
        auto* fallback = CCMenu::create();
        fallback->setPosition({win.width - 32.f, 90.f});
        button->setPosition({0.f, 0.f});
        noxUtilsButton->setPosition({0.f, -(button->getContentSize().height + 6.f)});
        fallback->addChild(button);
        fallback->addChild(noxUtilsButton);
        this->addChild(fallback);
    }
};

class $modify(NXRGamemodeBaseGameLayer, GJBaseGameLayer) {
    bool canBeActivatedByPlayer(PlayerObject* player, EffectGameObject* object) {
        auto* pl = PlayLayer::get();
        if (!g_portalsEnabled && pl && static_cast<GJBaseGameLayer*>(pl) == this && object && isPortalType(object->m_objectType)) {
            return false;
        }
        return GJBaseGameLayer::canBeActivatedByPlayer(player, object);
    }
};

class $modify(NXRGamemodePlayLayer, PlayLayer) {
    void setupHasCompleted() {
        g_portalsEnabled = true;
        PlayLayer::setupHasCompleted();
    }
};
