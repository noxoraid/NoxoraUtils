#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <vector>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_utils.hpp"
#include "../../interface/imgui/nxr_widget_helper.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"
#include "imgui.h"

NXR_HACK_CREATE(
    "Level", "Layout Mode",
    "Strips the level down to its gameplay layout. Choose what to hide and how the background looks in the settings",
    false
);

namespace {
    struct LayoutCache {
        geode::Ref<GameObject> anchor;
        PlayLayer* owner = nullptr;
        std::vector<geode::Ref<GameObject>> decorations;
        std::vector<geode::Ref<GameObject>> glows;

        void clear() {
            anchor = nullptr;
            owner = nullptr;
            decorations.clear();
            glows.clear();
        }
    };

    LayoutCache g_cache;

    void rebuild(PlayLayer* layer) {
        g_cache.clear();
        if (!layer->m_objects || layer->m_objects->count() == 0) return;

        g_cache.owner = layer;
        g_cache.anchor = static_cast<GameObject*>(layer->m_objects->objectAtIndex(0));

        for (auto* obj : geode::cocos::CCArrayExt<GameObject*>(layer->m_objects)) {
            if (!obj) continue;
            if (obj->m_objectType == GameObjectType::Decoration) {
                g_cache.decorations.emplace_back(obj);
            }
            if (obj->m_glowSprite) {
                g_cache.glows.emplace_back(obj);
            }
        }
    }

    void restore() {
        for (auto& obj : g_cache.decorations) obj->setVisible(true);
        for (auto& obj : g_cache.glows) {
            if (obj->m_glowSprite) obj->m_glowSprite->setVisible(true);
        }
        g_cache.clear();
    }

    void apply(PlayLayer* layer) {
        if (!layer || !layer->m_objects || layer->m_objects->count() == 0) return;

        auto* first = static_cast<GameObject*>(layer->m_objects->objectAtIndex(0));
        if (g_cache.owner != layer || g_cache.anchor.data() != first) {
            rebuild(layer);
        }

        auto& config = NXRConfig::get();

        if (config.get<bool>("nxr.level.layout_mode::hide_deco", true)) {
            for (auto& obj : g_cache.decorations) obj->setVisible(false);
        }

        if (config.get<bool>("nxr.level.layout_mode::hide_glow", true)) {
            for (auto& obj : g_cache.glows) {
                if (obj->m_glowSprite) obj->m_glowSprite->setVisible(false);
            }
        }

        if (config.get<bool>("nxr.level.layout_mode::custom_bg", true) && layer->m_background) {
            layer->m_background->setColor(
                NXR::Utils::hexToColor(config.get<std::string>("nxr.level.layout_mode::bg_color", "1E1E1E"))
            );
        }
    }
}

class $modify(NXRLayoutModePlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Level").findHackByName("Layout Mode");

        NXR::tryAddHook(self, hack, "PlayLayer::postUpdate");

        hack.setHandler([](bool state) {
            if (!state) restore();
        });

        hack.setCustomWindowImGui([
            hideDeco = hack.formatAdditionalSetting("hide_deco"),
            hideGlow = hack.formatAdditionalSetting("hide_glow"),
            customBg = hack.formatAdditionalSetting("custom_bg"),
            bgColor = hack.formatAdditionalSetting("bg_color")
        ]{
            NXRWidgetConfig::Checkbox("Hide Decorations", hideDeco, true);
            NXRWidgetConfig::Checkbox("Hide Glow", hideGlow, true);
            ImGui::Separator();
            NXRWidgetConfig::Checkbox("Custom Background", customBg, true);
            NXRWidgetConfig::ColorEdit3Hex("Background Color", bgColor, "1E1E1E");
        });

        hack.setCustomWindowCocos([
            hideDeco = hack.formatAdditionalSetting("hide_deco"),
            hideGlow = hack.formatAdditionalSetting("hide_glow"),
            customBg = hack.formatAdditionalSetting("custom_bg"),
            bgColor = hack.formatAdditionalSetting("bg_color")
        ](cocos2d::CCNode* popupNode) {
            auto* popup = static_cast<NXRHackSettingsPopup*>(popupNode);
            popup->addConfigToggle("Hide Decorations", hideDeco, true);
            popup->addConfigToggle("Hide Glow", hideGlow, true);
            popup->addSeparator();
            popup->addConfigToggle("Custom Background", customBg, true);
            popup->addConfigColor3Hex("Background Color", bgColor, "1E1E1E");
        });
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        apply(this);
    }
};
