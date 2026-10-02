#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include "nxr_config.hpp"

namespace NXR::Theme {
    using namespace geode::prelude;

    inline constexpr const char* kKey = "nxr.ui.theme";

    enum Id : int {
        Basic = 1,
        Normal = 2,
        Medium = 3,
        Pro = 4,
    };

    inline int current() {
        return std::clamp(NXRConfig::get().get<int>(kKey, Normal), static_cast<int>(Basic), static_cast<int>(Pro));
    }

    // Title bar colour of the Table layout, matched to each theme
    inline ccColor3B accent() {
        switch (current()) {
            case Basic: return {120, 120, 120};
            case Medium: return {58, 100, 180};
            case Pro: return {140, 56, 190};
            default: return {96, 112, 150};
        }
    }

    inline const char* square() {
        switch (current()) {
            case Basic: return "NXR_square_basic.png"_spr;
            case Medium: return "NXR_square_medium.png"_spr;
            case Pro: return "NXR_square_pro.png"_spr;
            default: return "NXR_square.png"_spr;
        }
    }

    inline const char* panel() {
        switch (current()) {
            case Basic: return "NXR_roundBG_basic.png"_spr;
            case Medium: return "NXR_roundBG_medium.png"_spr;
            case Pro: return "NXR_roundBG_pro.png"_spr;
            default: return "NXR_roundBG.png"_spr;
        }
    }

    inline const char* button() {
        switch (current()) {
            case Basic: return "NXR_button_01_basic.png"_spr;
            case Medium: return "NXR_button_01_medium.png"_spr;
            case Pro: return "NXR_button_01_pro.png"_spr;
            default: return "NXR_button_01.png"_spr;
        }
    }

    inline const char* buttonOn() {
        switch (current()) {
            case Basic: return "NXR_button_02_basic.png"_spr;
            case Medium: return "NXR_button_02_medium.png"_spr;
            case Pro: return "NXR_button_02_pro.png"_spr;
            default: return "NXR_button_02.png"_spr;
        }
    }
}
