#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include "nxr_config.hpp"

// Which menu layout is used. Panel = the original tabbed popup, Table = draggable windows (one per tab).
namespace NXR::Ui {
    using namespace geode::prelude;

    inline constexpr const char* kLayoutKey = "nxr.ui.layout";
    inline constexpr const char* kTableScaleKey = "nxr.ui.table_scale";

    enum Layout : int {
        Panel = 0,
        Table = 1,
    };

    // How on/off switches look and behave. Both are tap once = on, tap again = off.
    //   Switch   = the NXR slider toggle (original)
    //   Checkbox = a check mark box
    inline constexpr const char* kToggleStyleKey = "nxr.ui.toggle_style";

    enum ToggleStyle : int {
        Switch = 0,
        Checkbox = 1,
    };

    inline int toggleStyle() {
        return std::clamp(NXRConfig::get().get<int>(kToggleStyleKey, Switch), static_cast<int>(Switch), static_cast<int>(Checkbox));
    }

    inline constexpr const char* kCheckOnFrame = "GJ_checkOn_001.png";
    inline constexpr const char* kCheckOffFrame = "GJ_checkOff_001.png";

    // Drop-in replacement for CCMenuItemExt::createTogglerWithFilename with the NXR sprites.
    // `scale` is the size meant for the Switch, the Checkbox is sized to match.
    template <class Callback>
    inline CCMenuItemToggler* makeToggler(float scale, Callback&& callback) {
        if (toggleStyle() == Checkbox) {
            return CCMenuItemExt::createTogglerWithFrameName(kCheckOnFrame, kCheckOffFrame, scale * 0.9f, std::forward<Callback>(callback));
        }
        return CCMenuItemExt::createTogglerWithFilename("NXR_togglerOn.png"_spr, "NXR_togglerOff.png"_spr, scale, std::forward<Callback>(callback));
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

    // Implemented in nxr_menu_host.cpp
    bool menuOpen();
    void openMenu();
    void closeMenu();
    void toggleMenu();
    void reopenMenu();
}
