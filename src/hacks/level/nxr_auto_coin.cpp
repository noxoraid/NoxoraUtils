#include <Geode/Geode.hpp>
#include <algorithm>
#include <cmath>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_coin_scan.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"

NXR_HACK_CREATE(
    "Level", "Auto Pick Up Coin",
    "Automatically collects every coin in the level, wherever it is placed",
    true
);

namespace {
    void pullCoins(PlayLayer* layer, PlayerObject* p1, PlayerObject* p2) {
        auto& cache = NXR::Coins::ensure(layer);
        if (cache.coins.empty()) return;
        if (!p1 && !p2) return;

        for (auto& coin : cache.coins) {
            if (coin.collected || !coin.object) continue;

            PlayerObject* target = p1 ? p1 : p2;
            if (p1 && p2) {
                const auto home = coin.home;
                const auto a = p1->getPosition();
                const auto b = p2->getPosition();
                const float d1 = (home.x - a.x) * (home.x - a.x) + (home.y - a.y) * (home.y - a.y);
                const float d2 = (home.x - b.x) * (home.x - b.x) + (home.y - b.y) * (home.y - b.y);
                target = d1 <= d2 ? p1 : p2;
            }
            coin.object->setPosition(target->getPosition());
        }
    }

    void restoreCoins() {
        auto& cache = NXR::Coins::cache();
        for (auto& coin : cache.coins) {
            if (!coin.object || coin.collected) continue;
            coin.object->setPosition(coin.home);
        }
    }

    void markCollected() {
        auto& cache = NXR::Coins::cache();
        for (auto& coin : cache.coins) {
            if (!coin.object || coin.collected) continue;
            if (!coin.object->isVisible()) coin.collected = true;
        }
    }
}

class $modify(NXRAutoCoinBaseGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Level").findHackByName("Auto Pick Up Coin");

        NXR::trySetPriority(self, "GJBaseGameLayer::processCommands", -15);
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::processCommands");

        hack.setHandler([](bool state) {
            if (!state) {
                restoreCoins();
                NXR::Coins::cache().clear();
            }
        });

        hack.setCustomWindowCocos([
            p1 = hack.formatAdditionalSetting("p1"),
            p2 = hack.formatAdditionalSetting("p2")
        ](cocos2d::CCNode* popupNode) {
            auto* popup = static_cast<NXRHackSettingsPopup*>(popupNode);
            popup->addConfigToggle("Player 1", p1, true);
            popup->addConfigToggle("Player 2", p2, true);
        });
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto* pl = PlayLayer::get();
        const bool active = pl && static_cast<GJBaseGameLayer*>(pl) == this;

        if (active) {
            auto& config = NXRConfig::get();
            PlayerObject* p1 = (m_player1 && !m_player1->m_isDead
                && config.get<bool>("nxr.level.auto_pick_up_coin::p1", true)) ? m_player1 : nullptr;
            PlayerObject* p2 = (m_player2 && m_gameState.m_isDualMode && !m_player2->m_isDead
                && config.get<bool>("nxr.level.auto_pick_up_coin::p2", true)) ? m_player2 : nullptr;
            pullCoins(pl, p1, p2);
        }

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        if (active) {
            markCollected();
            restoreCoins();
        }
    }
};
