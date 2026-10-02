#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <vector>
#include <string>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_utils.hpp"
#include "../../core/nxr_level_stats.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"

NXR_HACK_CREATE(
    "Level", "Layout Mode",
    "Strips the level down to its gameplay layout. Pick a style, choose what to hide, recolor solids, hazards and interactables, and tune opacity",
    false
);

namespace {
    using NXR::Stats::Cat;

    constexpr const char* kPrefix = "nxr.level.layout_mode::";

    struct Tracked {
        geode::Ref<GameObject> object;
        cocos2d::ccColor3B color;
        GLubyte opacity;
        Cat cat;
    };

    struct LayoutCache {
        geode::Ref<GameObject> anchor;
        PlayLayer* owner = nullptr;
        std::vector<Tracked> tracked;
        std::vector<geode::Ref<GameObject>> glows;
        int tintCountdown = 0;
        std::string tintSignature;

        void clear() {
            anchor = nullptr;
            owner = nullptr;
            tracked.clear();
            glows.clear();
            tintCountdown = 0;
            tintSignature.clear();
        }
    };

    LayoutCache g_cache;

    struct Palette {
        std::string background;
        std::string solid;
        std::string hazard;
        std::string interact;
        bool tint;
    };

    Palette paletteFor(int style) {
        auto& config = NXRConfig::get();
        switch (style) {
            case 2: return {"0F2A52", "7FB2FF", "FF6B6B", "FFD84D", true};
            case 3: return {"1E1E1E", "9A9A9A", "E8E8E8", "FFFFFF", true};
            case 4: return {"0B0B14", "3A3F8F", "FF3B6E", "3BFFB0", true};
            case 5: return {"000000", "FFFFFF", "FF0000", "00FF00", true};
            default:
                return {
                    config.get<std::string>(std::string(kPrefix) + "bg_color", "1E1E1E"),
                    config.get<std::string>(std::string(kPrefix) + "solid_color", "9A9A9A"),
                    config.get<std::string>(std::string(kPrefix) + "hazard_color", "FF4D4D"),
                    config.get<std::string>(std::string(kPrefix) + "interact_color", "FFD84D"),
                    config.get<bool>(std::string(kPrefix) + "tint_objects", false)
                };
        }
    }

    void rebuild(PlayLayer* layer) {
        g_cache.clear();
        if (!layer->m_objects || layer->m_objects->count() == 0) return;

        g_cache.owner = layer;
        g_cache.anchor = static_cast<GameObject*>(layer->m_objects->objectAtIndex(0));
        g_cache.tracked.reserve(static_cast<size_t>(layer->m_objects->count()));

        for (auto* obj : geode::cocos::CCArrayExt<GameObject*>(layer->m_objects)) {
            if (!obj) continue;
            g_cache.tracked.push_back({geode::Ref<GameObject>(obj), obj->getColor(), obj->getOpacity(), NXR::Stats::categorize(obj)});
            if (obj->m_glowSprite) g_cache.glows.emplace_back(obj);
        }
    }

    void restore() {
        for (auto& entry : g_cache.tracked) {
            entry.object->setVisible(true);
            entry.object->setColor(entry.color);
            entry.object->setOpacity(entry.opacity);
        }
        for (auto& obj : g_cache.glows) {
            if (obj->m_glowSprite) obj->m_glowSprite->setVisible(true);
        }
        if (auto* layer = PlayLayer::get(); layer && layer->m_background) {
            layer->m_background->setVisible(true);
        }
        if (auto* layer = PlayLayer::get(); layer && layer->m_groundLayer) {
            layer->m_groundLayer->setVisible(true);
        }
        g_cache.clear();
    }

    cocos2d::ccColor3B colorFor(Cat cat, const Palette& palette) {
        switch (cat) {
            case Cat::Hazard: return NXR::Utils::hexToColor(palette.hazard);
            case Cat::Orb:
            case Cat::Pad:
            case Cat::Portal: return NXR::Utils::hexToColor(palette.interact);
            default: return NXR::Utils::hexToColor(palette.solid);
        }
    }

    void apply(PlayLayer* layer) {
        if (!layer || !layer->m_objects || layer->m_objects->count() == 0) return;

        auto* first = static_cast<GameObject*>(layer->m_objects->objectAtIndex(0));
        if (g_cache.owner != layer || g_cache.anchor.data() != first) {
            rebuild(layer);
        }

        auto& config = NXRConfig::get();
        const std::string prefix = kPrefix;

        const int style = config.get<int>(prefix + "style", 1);
        const Palette palette = paletteFor(style);

        const bool hideDeco = config.get<bool>(prefix + "hide_deco", true);
        const bool hideGlow = config.get<bool>(prefix + "hide_glow", true);
        const bool hideCoins = config.get<bool>(prefix + "hide_coins", false);
        const bool hideHazards = config.get<bool>(prefix + "hide_hazards", false);
        const bool hideGround = config.get<bool>(prefix + "hide_ground", false);
        const bool hideBg = config.get<bool>(prefix + "hide_bg", false);
        const bool customBg = style != 1 || config.get<bool>(prefix + "custom_bg", true);
        const float solidOpacity = std::clamp(config.get<float>(prefix + "solid_opacity", 100.f), 0.f, 100.f);
        const float decoOpacity = std::clamp(config.get<float>(prefix + "deco_opacity", 0.f), 0.f, 100.f);

        for (auto& entry : g_cache.tracked) {
            bool visible = true;
            switch (entry.cat) {
                case Cat::Deco: visible = !hideDeco || decoOpacity > 0.f; break;
                case Cat::Coin: visible = !hideCoins; break;
                case Cat::Hazard: visible = !hideHazards; break;
                default: break;
            }
            if (!visible) entry.object->setVisible(false);
        }

        if (hideGlow) {
            for (auto& obj : g_cache.glows) {
                if (obj->m_glowSprite) obj->m_glowSprite->setVisible(false);
            }
        }

        if (layer->m_background) {
            layer->m_background->setVisible(!hideBg);
            if (customBg) layer->m_background->setColor(NXR::Utils::hexToColor(palette.background));
        }

        if (layer->m_groundLayer) {
            layer->m_groundLayer->setVisible(!hideGround);
        }

        std::string signature = fmt::format(
            "{}|{}|{}|{}|{}|{}|{}|{}|{}",
            style, palette.tint, palette.solid, palette.hazard, palette.interact,
            solidOpacity, decoOpacity, hideDeco, g_cache.tracked.size()
        );

        if (g_cache.tintCountdown > 0 && signature == g_cache.tintSignature) {
            g_cache.tintCountdown--;
            return;
        }

        g_cache.tintSignature = signature;
        g_cache.tintCountdown = 20;

        for (auto& entry : g_cache.tracked) {
            switch (entry.cat) {
                case Cat::Solid:
                case Cat::Hazard:
                case Cat::Orb:
                case Cat::Pad:
                case Cat::Portal:
                    entry.object->setColor(palette.tint ? colorFor(entry.cat, palette) : entry.color);
                    break;
                default:
                    break;
            }

            switch (entry.cat) {
                case Cat::Solid:
                    entry.object->setOpacity(static_cast<GLubyte>(entry.opacity * solidOpacity / 100.f));
                    break;
                case Cat::Deco:
                    entry.object->setOpacity(hideDeco ? static_cast<GLubyte>(entry.opacity * decoOpacity / 100.f) : entry.opacity);
                    break;
                default:
                    break;
            }
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

        hack.setForm([
            style = hack.formatAdditionalSetting("style"),
            hideDeco = hack.formatAdditionalSetting("hide_deco"),
            hideGlow = hack.formatAdditionalSetting("hide_glow"),
            hideCoins = hack.formatAdditionalSetting("hide_coins"),
            hideHazards = hack.formatAdditionalSetting("hide_hazards"),
            hideGround = hack.formatAdditionalSetting("hide_ground"),
            hideBg = hack.formatAdditionalSetting("hide_bg"),
            customBg = hack.formatAdditionalSetting("custom_bg"),
            bgColor = hack.formatAdditionalSetting("bg_color"),
            tint = hack.formatAdditionalSetting("tint_objects"),
            solidColor = hack.formatAdditionalSetting("solid_color"),
            hazardColor = hack.formatAdditionalSetting("hazard_color"),
            interactColor = hack.formatAdditionalSetting("interact_color"),
            solidOpacity = hack.formatAdditionalSetting("solid_opacity"),
            decoOpacity = hack.formatAdditionalSetting("deco_opacity")
        ](NXR::Form& form) {
            auto* popup = &form;

            popup->addConfigSelect("Style", style, {
                {"Custom", 1},
                {"Blueprint", 2},
                {"Mono", 3},
                {"Neon", 4},
                {"High Contrast", 5}
            }, 1, [popup](int) { popup->requestRebuild(); });

            popup->addSeparator();
            popup->addConfigToggle("Hide Decorations", hideDeco, true);
            popup->addConfigToggle("Hide Glow", hideGlow, true);
            popup->addConfigToggle("Hide Coins", hideCoins, false);
            popup->addConfigToggle("Hide Hazards", hideHazards, false);
            popup->addConfigToggle("Hide Ground", hideGround, false);
            popup->addConfigToggle("Hide Background", hideBg, false);

            popup->addSeparator();
            popup->addConfigFloatInput("Solid Opacity %", solidOpacity, 0.f, 100.f, 100.f);
            popup->addConfigFloatInput("Deco Opacity %", decoOpacity, 0.f, 100.f, 0.f);

            if (NXRConfig::get().get<int>(style, 1) == 1) {
                popup->addSeparator();
                popup->addConfigToggle("Custom Background", customBg, true);
                popup->addConfigColor3Hex("Background Color", bgColor, "1E1E1E");
                popup->addConfigToggle("Recolor Objects", tint, false);
                popup->addConfigColor3Hex("Solid Color", solidColor, "9A9A9A");
                popup->addConfigColor3Hex("Hazard Color", hazardColor, "FF4D4D");
                popup->addConfigColor3Hex("Orb / Pad / Portal Color", interactColor, "FFD84D");
            }
        });
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        apply(this);
    }
};
