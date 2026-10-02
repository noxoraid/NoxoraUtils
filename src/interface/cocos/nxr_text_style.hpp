#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_utils.hpp"

namespace NXR::Ui {
    inline constexpr const char* kFontScaleKey = "nxr.ui.font_scale";
    inline constexpr const char* kFontColorKey = "nxr.ui.font_color";
    inline constexpr const char* kFontColorOnKey = "nxr.ui.font_color_on";
    inline constexpr const char* kStyledMarker = "nxr-text-styled";

    inline float fontScale() {
        return std::clamp(NXRConfig::get().get<float>(kFontScaleKey, 1.f), 0.6f, 1.6f);
    }

    inline cocos2d::ccColor3B textColor(cocos2d::ccColor3B fallback) {
        auto& config = NXRConfig::get();
        if (!config.get<bool>(kFontColorOnKey, true)) return fallback;
        return NXR::Utils::hexToColor(config.get<std::string>(kFontColorKey, "FFFFFF"));
    }

    inline void applyTextStyle(cocos2d::CCNode* root) {
        if (!root) return;
        if (geode::cast::typeinfo_cast<geode::TextInput*>(root)) return;

        const bool isLabel = geode::cast::typeinfo_cast<geode::Label*>(root) != nullptr
            || geode::cast::typeinfo_cast<cocos2d::CCLabelBMFont*>(root) != nullptr;

        if (isLabel && !root->getUserObject(kStyledMarker)) {
            root->setUserObject(kStyledMarker, cocos2d::CCString::create("1"));
            root->setScale(root->getScale() * fontScale());
            if (auto* rgba = geode::cast::typeinfo_cast<cocos2d::CCRGBAProtocol*>(root)) {
                rgba->setColor(textColor(rgba->getColor()));
            }
        }

        if (auto* children = root->getChildren()) {
            for (auto* child : geode::cocos::CCArrayExt<cocos2d::CCNode*>(children)) {
                applyTextStyle(child);
            }
        }
    }
}
