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
    "Utils", "UFO Straight",
    "Keeps the UFO straight and level. Manual: straight while you click or touch an orb. "
    "Auto: auto multi-click, UFO stays straight (good for corridors between top/bottom spikes)",
    true
);

namespace {
    struct UfoState {
        NXR::Straight::State base;
        int orbFrames = 0;

        void reset() {
            base.reset();
            orbFrames = 0;
        }
    };

    UfoState g_ufo1;
    UfoState g_ufo2;

    bool isRelevantRing(GameObject* ring) {
        auto& config = NXRConfig::get();
        if (config.get<bool>("nxr.utils.ufo_straight::all_orbs", false)) return true;

        switch (ring->m_objectID) {
            case 84:
                return config.get<bool>("nxr.utils.ufo_straight::blue", true);
            case 1022:
                return config.get<bool>("nxr.utils.ufo_straight::green", true);
            case 1330:
                return config.get<bool>("nxr.utils.ufo_straight::black", true);
            case 1704:
            case 1751:
                return config.get<bool>("nxr.utils.ufo_straight::dash", true);
            default:
                if (typeinfo_cast<DashRingObject*>(ring))
                    return config.get<bool>("nxr.utils.ufo_straight::dash", true);
                return false;
        }
    }

    void armOrb(PlayerObject* p) {
        auto* pl = PlayLayer::get();
        if (!pl || !p->m_isBird) return;

        auto& config = NXRConfig::get();
        const int hold = std::max(1, config.get<int>("nxr.utils.ufo_straight::hold_frames", 12));

        if (p == pl->m_player1 && config.get<bool>("nxr.utils.ufo_straight::p1", true))
            g_ufo1.orbFrames = hold;
        else if (p == pl->m_player2 && config.get<bool>("nxr.utils.ufo_straight::p2", true))
            g_ufo2.orbFrames = hold;
    }

    void applyUfoStraight(GJBaseGameLayer* layer, PlayerObject* p,
                          UfoState& st, bool isPlayer1) {
        if (!p->m_isBird) {
            st.reset();
            return;
        }

        auto& config = NXRConfig::get();
        const int mode  = std::clamp(config.get<int>("nxr.utils.ufo_straight::mode", 1), 1, 2);
        const int grace = std::max(0, config.get<int>("nxr.utils.ufo_straight::grace", 6));

        if (mode == NXR::Straight::Auto) {
            const int rate = std::max(1, config.get<int>("nxr.utils.ufo_straight::auto_rate", 2));
            st.base.autoCounter++;
            if (st.base.autoCounter >= rate) {
                st.base.autoCounter = 0;
                st.base.autoHolding = !st.base.autoHolding;
                if (st.base.autoHolding) NXR::Input::press(p); else NXR::Input::release(p);
            }
        } else if (st.base.autoHolding) {
            NXR::Input::release(p);
            st.base.autoHolding = false;
            st.base.autoCounter = 0;
        }

        st.base.idleFrames++;

        const bool orbActive = st.orbFrames > 0;
        if (orbActive) st.orbFrames--;

        const bool straighten =
            orbActive || NXR::Straight::shouldStraighten(mode, st.base, grace);

        if (!straighten) {
            st.base.hasLockY = false;
            return;
        }

        if (orbActive && st.orbFrames == std::max(1, config.get<int>("nxr.utils.ufo_straight::hold_frames", 12)) - 1) {
            st.base.hasLockY = false;
        }

        if (!st.base.hasLockY) {
            st.base.lockY = p->getPositionY();
            st.base.hasLockY = true;
        }

        p->m_yVelocity = 0.0;
        p->setPositionY(st.base.lockY);
        p->setRotation(0.f);
    }
}

class $modify(NXRUfoStraightPlayerObject, PlayerObject) {
    static void onModify(auto& self) {
        auto& gui = NXR::Gui::get();
        auto& hack = gui.getWindow("Utils").findHackByName("UFO Straight");

        NXR::tryAddHook(self, hack, "PlayerObject::ringJump");
        NXR::tryAddHook(self, hack, "PlayerObject::startDashing");
        NXR::tryAddHook(self, hack, "PlayerObject::pushButton");
        NXR::tryAddHook(self, hack, "PlayerObject::releaseButton");

        hack.setCustomWindowCocos([
            mode = hack.formatAdditionalSetting("mode"),
            rate = hack.formatAdditionalSetting("auto_rate"),
            grace = hack.formatAdditionalSetting("grace"),
            blue = hack.formatAdditionalSetting("blue"),
            green = hack.formatAdditionalSetting("green"),
            black = hack.formatAdditionalSetting("black"),
            dash = hack.formatAdditionalSetting("dash"),
            allOrbs = hack.formatAdditionalSetting("all_orbs"),
            frames = hack.formatAdditionalSetting("hold_frames"),
            p1 = hack.formatAdditionalSetting("p1"),
            p2 = hack.formatAdditionalSetting("p2")
        ](cocos2d::CCNode* popupNode) {
            auto* popup = static_cast<NXRHackSettingsPopup*>(popupNode);
            popup->addConfigModeToggle(mode, "Manual", "Auto");
            popup->addConfigIntInput("Auto Click Rate", rate, 1, 30, 2);
            popup->addConfigIntInput("Manual Grace Frames", grace, 0, 60, 6);
            popup->addSeparator();
            popup->addConfigToggle("Blue Orb", blue, true);
            popup->addConfigToggle("Green Orb", green, true);
            popup->addConfigToggle("Black Orb", black, true);
            popup->addConfigToggle("Dash Orb (Green/Pink)", dash, true);
            popup->addConfigToggle("All Other Orbs", allOrbs, false);
            popup->addConfigIntInput("Orb Hold Frames", frames, 1, 60, 12);
            popup->addSeparator();
            popup->addConfigToggle("Player 1", p1, true);
            popup->addConfigToggle("Player 2", p2, true);
        });
    }

    void ringJump(RingObject* ring, bool skipCheck) {
        PlayerObject::ringJump(ring, skipCheck);

        if (ring && isRelevantRing(ring))
            armOrb(this);
    }

    void startDashing(DashRingObject* ring) {
        PlayerObject::startDashing(ring);

        if (NXRConfig::get().get<bool>("nxr.utils.ufo_straight::dash", true))
            armOrb(this);
    }

    bool pushButton(PlayerButton button) {
        bool ret = PlayerObject::pushButton(button);
        if (button == PlayerButton::Jump && !NXR::Input::g_injecting) {
            int slot = NXR::Input::slotOf(this);
            if (slot != 0) {
                auto& st = slot == 1 ? g_ufo1 : g_ufo2;
                st.base.holding = true;
                st.base.idleFrames = 0;
            }
        }
        return ret;
    }

    bool releaseButton(PlayerButton button) {
        bool ret = PlayerObject::releaseButton(button);
        if (button == PlayerButton::Jump && !NXR::Input::g_injecting) {
            int slot = NXR::Input::slotOf(this);
            if (slot != 0) {
                (slot == 1 ? g_ufo1 : g_ufo2).base.holding = false;
            }
        }
        return ret;
    }
};

class $modify(NXRUfoStraightPlayLayer, PlayLayer) {
    void resetLevel() {
        g_ufo1.reset();
        g_ufo2.reset();
        PlayLayer::resetLevel();
    }
};

class $modify(NXRUfoStraightTickGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Utils").findHackByName("UFO Straight");
        NXR::trySetPriority(self, "GJBaseGameLayer::processCommands", -20);
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::processCommands");
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        auto* pl = PlayLayer::get();
        if (!pl || static_cast<GJBaseGameLayer*>(pl) != this) return;

        auto& config = NXRConfig::get();

        if (m_player1 && !m_player1->m_isDead) {
            if (config.get<bool>("nxr.utils.ufo_straight::p1", true))
                applyUfoStraight(this, m_player1, g_ufo1, true);
            else
                g_ufo1.reset();
        }

        if (m_gameState.m_isDualMode && m_player2 && !m_player2->m_isDead) {
            if (config.get<bool>("nxr.utils.ufo_straight::p2", true))
                applyUfoStraight(this, m_player2, g_ufo2, false);
            else
                g_ufo2.reset();
        } else {
            g_ufo2.reset();
        }
    }
};
