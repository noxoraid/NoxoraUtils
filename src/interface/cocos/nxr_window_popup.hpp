#pragma once
#include <Geode/Geode.hpp>
#include <functional>
#include <string>
#include "nxr_hacks_tab.hpp"

// Hosts one tab's custom content (Bot controls, Settings) in a popup. Used by the Table layout.
class NXRWindowPopup : public geode::Popup {
protected:
    bool init(const std::string& title, const std::function<void(NXRHacksTab*)>& build);

public:
    static NXRWindowPopup* create(const std::string& title, const std::function<void(NXRHacksTab*)>& build);
};
