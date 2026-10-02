#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_orb_assist.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"

NXR_HACK_CREATE(
    "Utils", "Easy Black Orb",
    "Two modes. No Touch: Black Orbs are never touched, you pass straight through them even if you touch them. "
    "Auto Click: automatically hits every Black Orb you touch, the click is stored so playback hits the orb",
    true
);

namespace {
    NXR::Orb::Tracker g_p1;
    NXR::Orb::Tracker g_p2;
}

class $modify(NXREasyBlackOrbGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Utils").findHackByName("Easy Black Orb");
        NXR::trySetPriority(self, "GJBaseGameLayer::processCommands", -10);
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::processCommands");
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::playerTouchedRing");

        hack.setCustomWindowCocos([
            mode = hack.formatAdditionalSetting("mode"),
            p1 = hack.formatAdditionalSetting("p1"),
            p2 = hack.formatAdditionalSetting("p2")
        ](cocos2d::CCNode* popupNode) {
            auto* popup = static_cast<NXRHackSettingsPopup*>(popupNode);
            popup->addConfigRadio("Mode", mode, {{"No Touch", 0}, {"Auto Click", 1}}, 0);
            popup->addSeparator();
            popup->addConfigToggle("Player 1", p1, true);
            popup->addConfigToggle("Player 2", p2, true);
        });
    }

    void playerTouchedRing(PlayerObject* player, RingObject* object) {
        auto* pl = PlayLayer::get();
        if (object && pl && static_cast<GJBaseGameLayer*>(pl) == this) {
            auto& config = NXRConfig::get();
            if (config.get<int>("nxr.utils.easy_black_orb::mode", 0) == 0 && object->m_objectID == NXR::Orb::kBlackOrb) {
                const bool isP2 = player == m_player2 && player != m_player1;
                const bool enabledFor = config.get<bool>(isP2 ? "nxr.utils.easy_black_orb::p2" : "nxr.utils.easy_black_orb::p1", true);
                if (enabledFor) return;
            }
        }
        GJBaseGameLayer::playerTouchedRing(player, object);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto* pl = PlayLayer::get();
        const bool autoClick = NXRConfig::get().get<int>("nxr.utils.easy_black_orb::mode", 0) == 1;
        if (autoClick && pl && static_cast<GJBaseGameLayer*>(pl) == this && !isHalfTick) {
            if (NXR::Orb::gameActive()) {
                auto& config = NXRConfig::get();
                auto match = [](GameObject* obj) { return obj->m_objectID == NXR::Orb::kBlackOrb; };

                if (config.get<bool>("nxr.utils.easy_black_orb::p1", true)) NXR::Orb::tick(m_player1, g_p1, false, match);
                else g_p1.reset();

                if (m_gameState.m_isDualMode && config.get<bool>("nxr.utils.easy_black_orb::p2", true)) NXR::Orb::tick(m_player2, g_p2, false, match);
                else g_p2.reset();
            } else {
                g_p1.holding = false;
                g_p2.holding = false;
            }
        }

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};

class $modify(NXREasyBlackOrbPlayLayer, PlayLayer) {
    void resetLevel() {
        g_p1.reset();
        g_p2.reset();
        PlayLayer::resetLevel();
    }
};
