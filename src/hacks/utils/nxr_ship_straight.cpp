#include <Geode/Geode.hpp>
#include <algorithm>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_straight_common.hpp"
#include "../../core/nxr_player_input.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"

NXR_HACK_CREATE(
    "Utils", "Ship Straight",
    "Makes the Ship fly 100% straight. Manual: you must click to keep it straight. "
    "Auto: auto-clicks fast but the ship stays 100% straight",
    true
);

namespace {
    NXR::Straight::State g_ship1;
    NXR::Straight::State g_ship2;

    void applyShipStraight(GJBaseGameLayer* layer, PlayerObject* player,
                           NXR::Straight::State& st, bool isPlayer1) {
        auto& config = NXRConfig::get();

        if (!player->m_isShip) {
            st.reset();
            return;
        }

        const int mode  = std::clamp(config.get<int>("nxr.utils.ship_straight::mode", 1), 1, 2);
        const int grace = std::max(0, config.get<int>("nxr.utils.ship_straight::grace", 4));

        if (mode == NXR::Straight::Auto) {
            const int rate = std::max(1, config.get<int>("nxr.utils.ship_straight::auto_rate", 1));
            st.autoCounter++;
            if (st.autoCounter >= rate) {
                st.autoCounter = 0;
                st.autoHolding = !st.autoHolding;
                if (st.autoHolding) NXR::Input::press(player); else NXR::Input::release(player);
            }
        } else if (st.autoHolding) {
            NXR::Input::release(player);
            st.autoHolding = false;
            st.autoCounter = 0;
        }

        st.idleFrames++;

        if (!NXR::Straight::shouldStraighten(mode, st, grace)) {
            st.hasLockY = false;
            return;
        }

        if (!st.hasLockY) {
            st.lockY = player->getPositionY();
            st.hasLockY = true;
        }

        player->m_yVelocity = 0.0;
        player->setPositionY(st.lockY);
        player->setRotation(0.f);
    }
}

class $modify(NXRShipStraightPlayerObject, PlayerObject) {
    static void onModify(auto& self) {
        auto& gui = NXR::Gui::get();
        auto& hack = gui.getWindow("Utils").findHackByName("Ship Straight");

        NXR::tryAddHook(self, hack, "PlayerObject::pushButton");
        NXR::tryAddHook(self, hack, "PlayerObject::releaseButton");

        hack.setCustomWindowCocos([
            mode = hack.formatAdditionalSetting("mode"),
            rate = hack.formatAdditionalSetting("auto_rate"),
            grace = hack.formatAdditionalSetting("grace"),
            p1 = hack.formatAdditionalSetting("p1"),
            p2 = hack.formatAdditionalSetting("p2")
        ](cocos2d::CCNode* popupNode) {
            auto* popup = static_cast<NXRHackSettingsPopup*>(popupNode);
            popup->addConfigModeToggle(mode, "Manual", "Auto");
            popup->addConfigIntInput("Auto Click Rate", rate, 1, 30, 1);
            popup->addConfigIntInput("Manual Grace Frames", grace, 0, 60, 4);
            popup->addSeparator();
            popup->addConfigToggle("Player 1", p1, true);
            popup->addConfigToggle("Player 2", p2, true);
        });
    }

    bool pushButton(PlayerButton button) {
        bool ret = PlayerObject::pushButton(button);
        if (button == PlayerButton::Jump && !NXR::Input::g_injecting) {
            int slot = NXR::Input::slotOf(this);
            if (slot != 0) {
                auto& st = slot == 1 ? g_ship1 : g_ship2;
                st.holding = true;
                st.idleFrames = 0;
            }
        }
        return ret;
    }

    bool releaseButton(PlayerButton button) {
        bool ret = PlayerObject::releaseButton(button);
        if (button == PlayerButton::Jump && !NXR::Input::g_injecting) {
            int slot = NXR::Input::slotOf(this);
            if (slot != 0) {
                (slot == 1 ? g_ship1 : g_ship2).holding = false;
            }
        }
        return ret;
    }
};

class $modify(NXRShipStraightPlayLayer, PlayLayer) {
    void resetLevel() {
        g_ship1.reset();
        g_ship2.reset();
        PlayLayer::resetLevel();
    }
};

class $modify(NXRShipStraightTickGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Utils").findHackByName("Ship Straight");
        NXR::trySetPriority(self, "GJBaseGameLayer::processCommands", -10);
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::processCommands");
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        auto* pl = PlayLayer::get();
        if (!pl || static_cast<GJBaseGameLayer*>(pl) != this) return;

        auto& config = NXRConfig::get();

        if (m_player1 && !m_player1->m_isDead) {
            if (config.get<bool>("nxr.utils.ship_straight::p1", true))
                applyShipStraight(this, m_player1, g_ship1, true);
            else
                g_ship1.reset();
        }

        if (m_gameState.m_isDualMode && m_player2 && !m_player2->m_isDead) {
            if (config.get<bool>("nxr.utils.ship_straight::p2", true))
                applyShipStraight(this, m_player2, g_ship2, false);
            else
                g_ship2.reset();
        } else {
            g_ship2.reset();
        }
    }
};
