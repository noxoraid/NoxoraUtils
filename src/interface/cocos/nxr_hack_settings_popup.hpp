#pragma once
#include <Geode/Geode.hpp>
#include <string>
#include "../../core/nxr_hacks.hpp"

class NXRHackSettingsPopup {
public:
    static void open(NXR::Hack& hack, const std::string& origin = "");
};
