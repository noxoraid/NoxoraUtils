#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "nxr_config.hpp"

namespace NXR::Ui {
    using namespace geode::prelude;

    inline constexpr const char* kLayoutKey = "nxr.ui.layout";
    inline constexpr const char* kTableScaleKey = "nxr.ui.table_scale";
    inline constexpr const char* kToggleStyleKey = "nxr.ui.toggle_style";

    inline constexpr float kSwitchScale = 1.f;
    inline constexpr float kCheckScale = 1.5f;
    inline constexpr float kRadioScale = 1.2f;

    enum Layout : int {
        Panel = 0,
        Table = 1,
    };

    enum ToggleStyle : int {
        Switch = 0,
        Check = 1,
    };

    inline int toggleStyle() {
        return std::clamp(NXRConfig::get().get<int>(kToggleStyleKey, Switch), static_cast<int>(Switch), static_cast<int>(Check));
    }

    template <class Callback>
    inline CCMenuItemToggler* makeToggler(float scale, Callback&& callback) {
        if (toggleStyle() == Check) {
            return CCMenuItemExt::createTogglerWithFilename("NXR_tableCheckOn.png"_spr, "NXR_tableCheckOff.png"_spr, scale * kCheckScale, std::forward<Callback>(callback));
        }
        return CCMenuItemExt::createTogglerWithFilename("NXR_togglerOn.png"_spr, "NXR_togglerOff.png"_spr, scale * kSwitchScale, std::forward<Callback>(callback));
    }

    inline int defaultLayout() {
#ifdef GEODE_IS_DESKTOP
        return Table;
#else
        return Panel;
#endif
    }

    inline int layout() {
        return std::clamp(NXRConfig::get().get<int>(kLayoutKey, defaultLayout()), static_cast<int>(Panel), static_cast<int>(Table));
    }

    inline float tableScale() {
        return std::clamp(NXRConfig::get().get<float>(kTableScaleKey, 1.f), 0.6f, 1.6f);
    }

    bool menuOpen();
    void openMenu();
    void closeMenu();
    void toggleMenu();
    void reopenMenu();
    void showPopup(geode::Popup* popup, const std::string& title);
    void showChoice(const std::string& title, const std::vector<std::string>& notes, const std::vector<std::pair<std::string, std::function<void()>>>& choices);
}
