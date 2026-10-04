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
    inline constexpr const char* kSettingsPopupKey = "nxr.ui.settings_popup";
    inline constexpr const char* kPanelScaleKey = "nxr.ui.panel_scale";
    inline constexpr const char* kPanelOpacityKey = "nxr.ui.panel_opacity";
    inline constexpr const char* kPanelXKey = "nxr.ui.panel_x";
    inline constexpr const char* kPanelYKey = "nxr.ui.panel_y";
    inline constexpr const char* kFavoritesKey = "nxr.ui.favorites";
    inline constexpr const char* kRecentColorsKey = "nxr.ui.recent_colors";
    inline constexpr const char* kTabKey = "nxr.ui.last_tab";
    inline constexpr const char* kAccentColorKey = "nxr.ui.accent_color";
    inline constexpr const char* kPanelColorKey = "nxr.ui.panel_color";
    inline constexpr const char* kGradientOnKey = "nxr.ui.gradient_on";
    inline constexpr const char* kGradientColorKey = "nxr.ui.gradient_color";
    inline constexpr const char* kGradientDirKey = "nxr.ui.gradient_dir";
    inline constexpr float kPanelScaleMin = 0.5f;
    inline constexpr float kPanelScaleMax = 1.5f;
    inline constexpr const char* kLogoScaleKey = "nxr.ui_icon.scale";
    inline constexpr const char* kLogoHideGameKey = "nxr.ui_icon.hide_on_game";
    inline constexpr const char* kLogoHideEditorKey = "nxr.ui_icon.hide_on_editor";

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

    enum SettingsPopup : int {
        Popup = 0,
        Clean = 1,
    };

    inline int settingsPopup() {
        return std::clamp(NXRConfig::get().get<int>(kSettingsPopupKey, Popup), static_cast<int>(Popup), static_cast<int>(Clean));
    }

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

    inline float panelScale() {
        return std::clamp(NXRConfig::get().get<float>(kPanelScaleKey, 1.f), kPanelScaleMin, kPanelScaleMax);
    }

    inline float panelOpacity() {
        return std::clamp(NXRConfig::get().get<float>(kPanelOpacityKey, 0.96f), 0.35f, 1.f);
    }

    inline std::vector<std::string> splitList(const std::string& text, char separator) {
        std::vector<std::string> out;
        std::string part;
        for (char c : text) {
            if (c == separator) {
                if (!part.empty()) out.push_back(part);
                part.clear();
            } else {
                part.push_back(c);
            }
        }
        if (!part.empty()) out.push_back(part);
        return out;
    }

    inline std::string joinList(const std::vector<std::string>& items, char separator) {
        std::string out;
        for (size_t i = 0; i < items.size(); i++) {
            if (i > 0) out.push_back(separator);
            out += items[i];
        }
        return out;
    }

    inline std::vector<std::string> favorites() {
        return splitList(NXRConfig::get().get<std::string>(kFavoritesKey, ""), '|');
    }

    inline bool isFavorite(const std::string& id) {
        auto list = favorites();
        return std::find(list.begin(), list.end(), id) != list.end();
    }

    inline void setFavorite(const std::string& id, bool value) {
        auto list = favorites();
        auto it = std::find(list.begin(), list.end(), id);
        if (value && it == list.end()) list.push_back(id);
        if (!value && it != list.end()) list.erase(it);
        NXRConfig::get().set<std::string>(kFavoritesKey, joinList(list, '|'));
    }

    inline std::vector<std::string> recentColors() {
        return splitList(NXRConfig::get().get<std::string>(kRecentColorsKey, ""), ',');
    }

    inline void pushRecentColor(const std::string& hex8) {
        auto list = recentColors();
        auto it = std::find(list.begin(), list.end(), hex8);
        if (it != list.end()) list.erase(it);
        list.insert(list.begin(), hex8);
        if (list.size() > 6) list.resize(6);
        NXRConfig::get().set<std::string>(kRecentColorsKey, joinList(list, ','));
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
    void pickReplay(const std::string& title, const std::string& action, std::function<void(const std::string&)> onPick, bool allowClear = false);
    void showChoice(const std::string& title, const std::vector<std::string>& notes, const std::vector<std::pair<std::string, std::function<void()>>>& choices);
}
