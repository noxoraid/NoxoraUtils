#pragma once
#include <Geode/Geode.hpp>
#include <string>
#include <vector>
#include "../../core/nxr_form.hpp"
#include "../../core/nxr_hacks.hpp"

namespace NXR::Imgui {
    bool isOpen();
    void open();
    void close();
    void holdFor(cocos2d::CCNode* node);
    void openHackSettings(NXR::Hack& hack);

    NXR::Form& form();
    bool button(const std::string& label, float width = 0.f, bool active = false);
    int choice(const std::vector<std::string>& labels, int current, int columns = 0);
    void later(std::function<void()> fn);
}
