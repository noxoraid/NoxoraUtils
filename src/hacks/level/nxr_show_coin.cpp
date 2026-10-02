#include <Geode/Geode.hpp>
#include <algorithm>
#include <Geode/modify/PlayLayer.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_utils.hpp"
#include "../../core/nxr_coin_scan.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"

NXR_HACK_CREATE(
    "Level", "Show Coin",
    "Draws a long line from the player to every coin so you can see where each coin is. Color is set in the settings",
    false
);

namespace {
    geode::Ref<cocos2d::CCDrawNode> g_node;
    PlayLayer* g_owner = nullptr;

    void detach() {
        if (g_node && g_node->getParent()) g_node->removeFromParent();
        g_node = nullptr;
        g_owner = nullptr;
    }

    void ensureNode(PlayLayer* layer) {
        if (g_node && g_owner == layer && g_node->getParent()) return;
        detach();
        if (!layer || !layer->m_objectLayer) return;

        g_node = cocos2d::CCDrawNode::create();
        g_node->setZOrder(2000);
        layer->m_objectLayer->addChild(g_node);
        g_owner = layer;
    }

    void draw(PlayLayer* layer) {
        auto& config = NXRConfig::get();
        ensureNode(layer);
        if (!g_node || !layer->m_player1) return;

        g_node->clear();

        auto& cache = NXR::Coins::ensure(layer);
        if (cache.coins.empty()) return;

        const auto rgb = NXR::Utils::hexToColor(config.get<std::string>("nxr.level.show_coin::color", "FFD700"));
        const float alpha = std::clamp(config.get<int>("nxr.level.show_coin::opacity", 220), 0, 255) / 255.f;
        const float thickness = std::max(0.5f, config.get<float>("nxr.level.show_coin::thickness", 1.5f));
        const bool hideTaken = config.get<bool>("nxr.level.show_coin::hide_collected", true);
        const bool p2 = config.get<bool>("nxr.level.show_coin::from_p2", false);

        const cocos2d::ccColor4F color = { rgb.r / 255.f, rgb.g / 255.f, rgb.b / 255.f, alpha };

        const auto origin = (p2 && layer->m_player2 && layer->m_gameState.m_isDualMode)
            ? layer->m_player2->getPosition()
            : layer->m_player1->getPosition();

        for (auto& coin : cache.coins) {
            if (!coin.object) continue;
            if (hideTaken && coin.collected) continue;
            g_node->drawSegment(origin, coin.home, thickness, color);
        }
    }
}

class $modify(NXRShowCoinPlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Level").findHackByName("Show Coin");

        NXR::tryAddHook(self, hack, "PlayLayer::postUpdate");

        hack.setHandler([](bool state) {
            if (!state) detach();
        });

        hack.setCustomWindowCocos([
            colorKey = hack.formatAdditionalSetting("color"),
            opacityKey = hack.formatAdditionalSetting("opacity"),
            thickKey = hack.formatAdditionalSetting("thickness"),
            hideKey = hack.formatAdditionalSetting("hide_collected"),
            p2Key = hack.formatAdditionalSetting("from_p2")
        ](cocos2d::CCNode* popupNode) {
            auto* popup = static_cast<NXRHackSettingsPopup*>(popupNode);
            popup->addConfigColor3Hex("Line Color", colorKey, "FFD700");
            popup->addConfigIntInput("Opacity", opacityKey, 0, 255, 220);
            popup->addConfigFloatInput("Thickness", thickKey, 0.5f, 10.f, 1.5f);
            popup->addSeparator();
            popup->addConfigToggle("Hide Collected Coins", hideKey, true);
            popup->addConfigToggle("Draw From Player 2", p2Key, false);
        });
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        ::draw(this);
    }

    void onQuit() {
        detach();
        PlayLayer::onQuit();
    }
};
