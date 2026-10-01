#include "nxr_widget_helper.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_keybinds.hpp"
#include "nxr_widget.hpp"
#include <fmt/format.h>
#include <imgui-cocos.hpp>

bool NXRWidgetConfig::HackCheckbox(const char* label, const std::string& config_key, bool default_value = false) {
    auto& config = NXRConfig::get();

    bool value = config.get<bool>(config_key, default_value);
    if (NXRWidget::Checkbox(label, &value))
    {
        auto& gui = NXR::Gui::get();
        auto* hack = gui.findHackByIDGlobal(config_key);
        if (hack != nullptr)
            hack->toggle();
    }
    return false;
}

bool NXRWidgetConfig::Checkbox(const char* label, const std::string& config_key, bool default_value = false) {
    auto& config = NXRConfig::get();
    bool value = config.get<bool>(config_key, default_value);
    if (NXRWidget::Checkbox(label, &value))
    {
        config.set<bool>(config_key, value);
        return true;
    }
    return false;
}

bool NXRWidgetConfig::ModeSwitch(const std::string& config_key, const char* off_label, const char* on_label, int default_value) {
    auto& config = NXRConfig::get();
    bool on = config.get<int>(config_key, default_value) == 2;
    std::string label = fmt::format("Mode: {}###{}", on ? on_label : off_label, config_key);
    if (NXRWidget::Checkbox(label.c_str(), &on)) {
        config.set<int>(config_key, on ? 2 : 1);
        return true;
    }
    return false;
}

bool NXRWidgetConfig::RadioInt(const std::string& config_key, int default_value, const std::vector<std::pair<std::string, int>>& options) {
    auto& config = NXRConfig::get();
    int current = config.get<int>(config_key, default_value);
    bool changed = false;
    for (const auto& [label, value] : options) {
        if (NXRWidget::RadioButton(label.c_str(), current == value)) {
            config.set<int>(config_key, value);
            current = value;
            changed = true;
        }
    }
    return changed;
}

bool NXRWidgetConfig::InputInt(const char* label, const std::string& config_key, int default_value) {
    auto& config = NXRConfig::get();
    int value = config.get<int>(config_key, default_value);
    if (ImGui::InputInt(label, &value, 1))
    {
        config.set<int>(config_key, value);
        return true;
    }
    return false;
}

bool NXRWidgetConfig::InputFloat(const char* label, const std::string& config_key, float default_value) {
    auto& config = NXRConfig::get();
    float value = config.get<float>(config_key, default_value);
    if (ImGui::InputFloat(label, &value, 1.f))
    {
        config.set<float>(config_key, value);
        return true;
    }
    return false;
}

bool NXRWidgetConfig::DragFloat(const char* label, const std::string& config_key, float step, float min, float max, float default_value, const char* format)
{
    auto& config = NXRConfig::get();
    float value = config.get<float>(config_key, default_value);
    if (NXRWidget::DragFloat(label, &value, step, min, max, format))
    {
        config.set<float>(config_key, value);
        return true;
    }
    return false;
}

bool NXRWidgetConfig::DragInt(const char* label, const std::string& config_key, float step, int min, int max, int default_value, const char* format)
{
    auto& config = NXRConfig::get();
    int value = config.get<int>(config_key, default_value);
    if (NXRWidget::DragInt(label, &value, step, min, max, format))
    {
        config.set<int>(config_key, value);
        return true;
    }
    return false;
}

void NXRWidgetConfig::DrawCustomKeybindButton(const std::string& id, const std::string& displayName, geode::Keybind defaultBind) {
    auto& kb = NXR::Keybinds::get();
    geode::Keybind currentBind = kb.getBind(id);

    if (!kb.hasCustom(id) && defaultBind.key != cocos2d::enumKeyCodes::KEY_None) {
        kb.changeBind(id, defaultBind);
        currentBind = defaultBind;
    }

    std::string statusText = "";

    if (kb.isRecordingCustom(id))
        statusText = "...";
    else if (currentBind.key == cocos2d::enumKeyCodes::KEY_None)
        statusText = "None";
    else
        statusText = fmt::format("{}", currentBind.toString());

    std::string fullBtnText = fmt::format("{}: {}", displayName, statusText);
    std::string uniqueID = fmt::format("{}##keybind_custom_btn_{}", fullBtnText, id);

    if (NXRWidget::Button(uniqueID.c_str(), {ImGui::GetContentRegionAvail().x, 0})) {
        kb.startRecordingCustom(id);
    }
}

void NXRWidgetConfig::DrawKeybindButton(const std::string& windowName, NXR::Hack& hack) {
    auto& kb = NXR::Keybinds::get();
    geode::Keybind currentBind = hack.getKeybind();

    std::string statusText = "";

    if (kb.isRecording(windowName, hack.getName()))
        statusText = "...";
    else if (currentBind.key == cocos2d::enumKeyCodes::KEY_None)
        statusText = "None";
    else
        statusText = fmt::format("{}", currentBind.toString());

    std::string fullBtnText = fmt::format("{}: {}", hack.getName(), statusText);

    std::string uniqueID = fmt::format("{}##keybind_btn_{}_{}", fullBtnText, windowName, hack.getName());

    if (NXRWidget::Button(uniqueID.c_str(), {ImGui::GetContentRegionAvail().x, 0})) {
        kb.startRecording(windowName, hack.getName());
    }
}

void NXRWidgetConfig::ColorEdit3Hex(const char* label, const std::string& key, const std::string& defaultHex) {
    auto& config = NXRConfig::get();
    std::string currentHex = config.get<std::string>(key, defaultHex);

    unsigned int hexValue = 0xFFFFFF;
    std::stringstream ss;
    ss << std::hex << (currentHex[0] == '#' ? currentHex.substr(1) : currentHex);
    ss >> hexValue;

    float color[3] = {
        ((hexValue >> 16) & 0xFF) / 255.f,
        ((hexValue >> 8) & 0xFF) / 255.f,
        (hexValue & 0xFF) / 255.f
    };

    if (ImGui::ColorEdit3(label, color, ImGuiColorEditFlags_NoInputs)) {
        std::string newHex = fmt::format("{:02X}{:02X}{:02X}",
            static_cast<int>(color[0] * 255.f),
            static_cast<int>(color[1] * 255.f),
            static_cast<int>(color[2] * 255.f));

        config.set<std::string>(key, newHex);
    }

    ImGui::SameLine();
    if (NXRWidget::Button(fmt::format("Reset##reset_{}", key).c_str())) {
        config.set<std::string>(key, defaultHex);
    }
}

void NXRWidgetConfig::ColorEdit4Hex(const char* label, const std::string& key, const std::string& defaultHex) {
    auto& config = NXRConfig::get();
    std::string currentHex = config.get<std::string>(key, defaultHex);

    std::string cleanHex = (currentHex[0] == '#' ? currentHex.substr(1) : currentHex);
    if (cleanHex.length() == 6) {
        cleanHex += "FF";
    }

    unsigned int hexValue = 0xFFFFFFFF;
    std::stringstream ss;
    ss << std::hex << cleanHex;
    ss >> hexValue;

    float color[4] = {
        ((hexValue >> 24) & 0xFF) / 255.f,
        ((hexValue >> 16) & 0xFF) / 255.f,
        ((hexValue >> 8) & 0xFF) / 255.f,
        (hexValue & 0xFF) / 255.f
    };

    if (ImGui::ColorEdit4(label, color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar)) {
        std::string newHex = fmt::format("{:02X}{:02X}{:02X}{:02X}",
            static_cast<int>(color[0] * 255.f),
            static_cast<int>(color[1] * 255.f),
            static_cast<int>(color[2] * 255.f),
            static_cast<int>(color[3] * 255.f));

        config.set<std::string>(key, newHex);
    }

    ImGui::SameLine();
    if (NXRWidget::Button(fmt::format("Reset##reset_{}", key).c_str())) {
        config.set<std::string>(key, defaultHex);
    }
}
