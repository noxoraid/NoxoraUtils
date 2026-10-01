#pragma once
#include <string>
#include <vector>
#include <utility>
#include "../../core/nxr_hacks.hpp"

namespace NXRWidgetConfig {
    bool Checkbox(const char* label, const std::string& config_key, bool default_value);
    bool HackCheckbox(const char* label, const std::string& config_key, bool default_value);

    bool ModeSwitch(const std::string& config_key, const char* off_label, const char* on_label, int default_value = 1);
    bool RadioInt(const std::string& config_key, int default_value, const std::vector<std::pair<std::string, int>>& options);

    bool InputInt(const char* label, const std::string& config_key, int default_value);
    bool InputFloat(const char* label, const std::string& config_key, float default_value);
    bool DragFloat(const char* label, const std::string& config_key, float step, float min, float max, float default_value, const char* format);
    bool DragInt(const char* label, const std::string& config_key, float step, int min, int max, int default_value, const char* format);

    void ColorEdit3Hex(const char* label, const std::string& key, const std::string& defaultHex);
    void ColorEdit4Hex(const char* label, const std::string& key, const std::string& defaultHex);

    void DrawCustomKeybindButton(const std::string& id, const std::string& displayName, geode::Keybind defaultBind = geode::Keybind(cocos2d::KEY_None, KeyboardModifier::None));
    void DrawKeybindButton(const std::string& windowName, NXR::Hack& hack);
}
