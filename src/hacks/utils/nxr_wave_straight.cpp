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
#include "../../core/nxr_bot.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"

NXR_HACK_CREATE(
    "Utils", "Wave Straight",
    "Makes the Wave fly 100% straight. Manual: you must click to keep it straight. "
    "Auto: auto-clicks fast but the wave stays 100% straight",
    true
);

namespace {
    NXR::Straight::State g_wave1;
    NXR::Straight::State g_wave2;

    // Wave only: reset when the player is not in Wave form or while a replay is playing (the bot drives the inputs).
    void applyWaveStraight(PlayerObject* player, NXR::Straight::State& st) {
        if (!player->m_isDart || NXR::Bot::State::get().mode == NXR::Bot::Mode::Playing) {
            st.reset();
            return;
        }

        NXR::Straight::apply(player, st, "nxr.utils.wave_straight");
    }
}

class $modify(NXRWaveStraightPlayerObject, PlayerObject) {
    static void onModify(auto& self) {
        auto& gui = NXR::Gui::get();
        auto& hack = gui.getWindow("Utils").findHackByName("Wave Straight");

        NXR::tryAddHook(self, hack, "PlayerObject::pushButton");
        NXR::tryAddHook(self, hack, "PlayerObject::releaseButton");

        hack.setForm([
            mode = hack.formatAdditionalSetting("mode"),
            rate = hack.formatAdditionalSetting("auto_rate"),
            grace = hack.formatAdditionalSetting("grace"),
            p1 = hack.formatAdditionalSetting("p1"),
            p2 = hack.formatAdditionalSetting("p2")
        ](NXR::Form& form) {
            auto* popup = &form;
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
                auto& st = slot == 1 ? g_wave1 : g_wave2;
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
                (slot == 1 ? g_wave1 : g_wave2).holding = false;
            }
        }
        return ret;
    }
};

class $modify(NXRWaveStraightPlayLayer, PlayLayer) {
    void resetLevel() {
        g_wave1.reset();
        g_wave2.reset();
        PlayLayer::resetLevel();
    }
};

class $modify(NXRWaveStraightTickGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Utils").findHackByName("Wave Straight");
        NXR::trySetPriority(self, "GJBaseGameLayer::processCommands", -10);
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::processCommands");
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        auto* pl = PlayLayer::get();
        if (!pl || static_cast<GJBaseGameLayer*>(pl) != this) return;

        auto& config = NXRConfig::get();

        if (m_player1 && !m_player1->m_isDead) {
            if (config.get<bool>("nxr.utils.wave_straight::p1", true))
                applyWaveStraight(m_player1, g_wave1);
            else
                g_wave1.reset();
        }

        if (m_gameState.m_isDualMode && m_player2 && !m_player2->m_isDead) {
            if (config.get<bool>("nxr.utils.wave_straight::p2", true))
                applyWaveStraight(m_player2, g_wave2);
            else
                g_wave2.reset();
        } else {
            g_wave2.reset();
        }
    }
};
