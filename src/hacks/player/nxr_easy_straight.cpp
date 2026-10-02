#include <Geode/Geode.hpp>
#include <algorithm>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_bot.hpp"
#include "../../core/nxr_player_input.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"

NXR_HACK_CREATE(
    "Player", "Easy Straight",
    "Helps keep Ship and Wave straight. Tap quickly and the player locks to a real straight line at that height. "
    "Hold the button longer and you move normally again. Timing uses game time, so it does not depend on FPS",
    true
);

namespace {
    struct Slot {
        double lastPress = -1.0e9;
        double lastRelease = -1.0e9;
        double pressStart = -1.0e9;
        bool down = false;
        int taps = 0;
        bool engaged = false;
        float lockY = 0.f;

        void reset() {
            lastPress = -1.0e9;
            lastRelease = -1.0e9;
            pressStart = -1.0e9;
            down = false;
            taps = 0;
            engaged = false;
            lockY = 0.f;
        }
    };

    Slot g_slot1;
    Slot g_slot2;

    constexpr const char* kShipKey = "nxr.player.easy_straight::ship";
    constexpr const char* kWaveKey = "nxr.player.easy_straight::wave";
    constexpr const char* kGapKey = "nxr.player.easy_straight::tap_gap";
    constexpr const char* kHoldKey = "nxr.player.easy_straight::hold_cancel";
    constexpr const char* kP1Key = "nxr.player.easy_straight::p1";
    constexpr const char* kP2Key = "nxr.player.easy_straight::p2";

    double tapGap() {
        return std::clamp(static_cast<double>(NXRConfig::get().get<float>(kGapKey, 0.25f)), 0.05, 1.0);
    }

    double holdCancel() {
        return std::clamp(static_cast<double>(NXRConfig::get().get<float>(kHoldKey, 0.2f)), 0.05, 1.0);
    }

    double levelTime() {
        auto* pl = PlayLayer::get();
        return pl ? static_cast<double>(pl->m_gameState.m_levelTime) : 0.0;
    }

    bool modeAllowed(PlayerObject* player) {
        auto& config = NXRConfig::get();
        if (player->m_isShip) return config.get<bool>(kShipKey, true) && !config.get<bool>("nxr.utils.ship_straight", false);
        if (player->m_isDart) return config.get<bool>(kWaveKey, true) && !config.get<bool>("nxr.utils.wave_straight", false);
        return false;
    }

    void applyEasyStraight(PlayerObject* player, Slot& st) {
        if (!modeAllowed(player) || NXR::Bot::State::get().mode == NXR::Bot::Mode::Playing) {
            st.engaged = false;
            return;
        }

        const double now = levelTime();
        const double gap = tapGap();
        const bool holdingLong = st.down && now - st.pressStart > holdCancel();
        const bool recent = st.down || now - st.lastRelease <= gap;
        const bool wanted = st.taps >= 2 && !holdingLong && recent;

        if (!wanted) {
            st.engaged = false;
            if (!st.down && now - st.lastRelease > gap) st.taps = 0;
            return;
        }

        if (!st.engaged) {
            st.engaged = true;
            st.lockY = player->getPositionY();
        }

        player->m_yVelocity = 0.0;
        player->setPositionY(st.lockY);
        player->setRotation(0.f);
    }
}

class $modify(NXREasyStraightPlayerObject, PlayerObject) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Player").findHackByName("Easy Straight");

        NXR::tryAddHook(self, hack, "PlayerObject::pushButton");
        NXR::tryAddHook(self, hack, "PlayerObject::releaseButton");

        hack.setForm([](NXR::Form& form) {
            auto* popup = &form;
            popup->addConfigToggle("Ship", kShipKey, true);
            popup->addConfigToggle("Wave", kWaveKey, true);
            popup->addSeparator();
            popup->addConfigFloatInput("Tap Gap (s)", kGapKey, 0.05f, 1.f, 0.25f);
            popup->addConfigFloatInput("Hold Cancel (s)", kHoldKey, 0.05f, 1.f, 0.2f);
            popup->addSeparator();
            popup->addConfigToggle("Player 1", kP1Key, true);
            popup->addConfigToggle("Player 2", kP2Key, true);
        });
    }

    bool pushButton(PlayerButton button) {
        bool ret = PlayerObject::pushButton(button);
        if (button == PlayerButton::Jump) {
            int slot = NXR::Input::slotOf(this);
            if (slot != 0) {
                auto& st = slot == 1 ? g_slot1 : g_slot2;
                const double now = levelTime();
                st.taps = now - st.lastPress <= tapGap() ? st.taps + 1 : 1;
                st.lastPress = now;
                st.pressStart = now;
                st.down = true;
            }
        }
        return ret;
    }

    bool releaseButton(PlayerButton button) {
        bool ret = PlayerObject::releaseButton(button);
        if (button == PlayerButton::Jump) {
            int slot = NXR::Input::slotOf(this);
            if (slot != 0) {
                auto& st = slot == 1 ? g_slot1 : g_slot2;
                st.down = false;
                st.lastRelease = levelTime();
            }
        }
        return ret;
    }
};

class $modify(NXREasyStraightPlayLayer, PlayLayer) {
    void resetLevel() {
        g_slot1.reset();
        g_slot2.reset();
        PlayLayer::resetLevel();
    }
};

class $modify(NXREasyStraightTickGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Player").findHackByName("Easy Straight");
        NXR::trySetPriority(self, "GJBaseGameLayer::processCommands", -5);
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::processCommands");
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        auto* pl = PlayLayer::get();
        if (!pl || static_cast<GJBaseGameLayer*>(pl) != this) return;

        auto& config = NXRConfig::get();

        if (m_player1 && !m_player1->m_isDead && config.get<bool>(kP1Key, true)) applyEasyStraight(m_player1, g_slot1);
        else g_slot1.engaged = false;

        if (m_gameState.m_isDualMode && m_player2 && !m_player2->m_isDead && config.get<bool>(kP2Key, true)) applyEasyStraight(m_player2, g_slot2);
        else g_slot2.engaged = false;
    }
};
