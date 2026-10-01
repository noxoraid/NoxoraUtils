#include "imgui.h"
#include "nxr_widget.hpp"
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include "nxr_layout.hpp"
#include "../../core/nxr_utils.hpp"
#include <json.hpp>

#ifdef GEODE_IS_ANDROID
#include "../../core/nxr_gui.hpp"
#endif

#define NXR_COLOR_F32(x) ImVec4 { (((x) >> 24) & 0xFF) / 255.0f, (((x) >> 16) & 0xFF) / 255.0f, (((x) >> 8) & 0xFF) / 255.0f, ((x) & 0xFF) / 255.0f }
static const auto themeFilePath = geode::Mod::get()->getSaveDir() / "theme.json";

namespace NXRWidget {
    std::map<Color, ImVec4> colorTable = {
        {Color::Button_Background,              NXR_COLOR_F32(0xF2802BFF)},
        {Color::Button_Background_Hover,        NXR_COLOR_F32(0xF2802BFF)},
        {Color::Button_Background_Active,       NXR_COLOR_F32(0xE47B2FFF)},
        {Color::Button_Foreground,              NXR_COLOR_F32(0x30231DFF)},
        {Color::Button_Foreground_Hover,        NXR_COLOR_F32(0x3B2A22FF)},
        {Color::Button_Foreground_Active,       NXR_COLOR_F32(0x241B16FF)},
        {Color::Button_Border,                  NXR_COLOR_F32(0x00000000)},

        {Color::Checkbox_Background_Off,        NXR_COLOR_F32(0x563B2CFF)},
        {Color::Checkbox_Background_Off_Hover,  NXR_COLOR_F32(0xB56938FF)},
        {Color::Checkbox_Background_Off_Active, NXR_COLOR_F32(0x493328FF)},
        {Color::Checkbox_Background_On,         NXR_COLOR_F32(0xF2802BFF)},
        {Color::Checkbox_Background_On_Hover,   NXR_COLOR_F32(0xF2802BFF)},
        {Color::Checkbox_Background_On_Active,  NXR_COLOR_F32(0xE47B2FFF)},
        {Color::Checkbox_Knob_Off,              NXR_COLOR_F32(0xED7E2DFF)},
        {Color::Checkbox_Knob_Off_Hover,        NXR_COLOR_F32(0xF2802BFF)},
        {Color::Checkbox_Knob_Off_Active,       NXR_COLOR_F32(0xC77136FF)},
        {Color::Checkbox_Knob_On,               NXR_COLOR_F32(0x33251EFF)},
        {Color::Checkbox_Knob_On_Hover,         NXR_COLOR_F32(0x412E24FF)},
        {Color::Checkbox_Knob_On_Active,        NXR_COLOR_F32(0x261C17FF)},
        {Color::Checkbox_Knob_Shadow,           NXR_COLOR_F32(0x0000002E)},
        {Color::Checkbox_Border,                NXR_COLOR_F32(0x00000000)},

        {Color::RadioButton_Ring_Off,          NXR_COLOR_F32(0xB56938FF)},
        {Color::RadioButton_Ring_Off_Hover,    NXR_COLOR_F32(0xC97136FF)},
        {Color::RadioButton_Ring_Off_Active,   NXR_COLOR_F32(0x985D39FF)},
        {Color::RadioButton_Ring_On,           NXR_COLOR_F32(0xF2802BFF)},
        {Color::RadioButton_Ring_On_Hover,     NXR_COLOR_F32(0xF2802BFF)},
        {Color::RadioButton_Ring_On_Active,    NXR_COLOR_F32(0xE47B2FFF)},
        {Color::RadioButton_Dot_Off,           NXR_COLOR_F32(0x00000000)},
        {Color::RadioButton_Dot_On,            NXR_COLOR_F32(0xF2802BFF)},
        {Color::RadioButton_Dot_On_Hover,      NXR_COLOR_F32(0xF2802BFF)},
        {Color::RadioButton_Dot_On_Active,     NXR_COLOR_F32(0xE47B2FFF)},
        {Color::RadioButton_Layer_Off_Hover,   NXR_COLOR_F32(0xED7E2D14)},
        {Color::RadioButton_Layer_Off_Active,  NXR_COLOR_F32(0xED7E2D1F)},
        {Color::RadioButton_Layer_On_Hover,    NXR_COLOR_F32(0xF2802B14)},
        {Color::RadioButton_Layer_On_Active,   NXR_COLOR_F32(0xF2802B1F)},

        {Color::Tooltip_Background,             NXR_COLOR_F32(0x33251EFF)},
        {Color::Tooltip_Foreground,             NXR_COLOR_F32(0xF2802BFF)},
        {Color::Tooltip_Border,                 NXR_COLOR_F32(0xF2802BFF)},
        {Color::Drag_Background,                NXR_COLOR_F32(0x563B2CFF)},
        {Color::Drag_Background_Hover,          NXR_COLOR_F32(0xB56938FF)},
        {Color::Drag_Background_Active,         NXR_COLOR_F32(0xF2802BFF)},
        {Color::Drag_Middleground,              NXR_COLOR_F32(0xAB6539FF)},
        {Color::Drag_Middleground_Hover,        NXR_COLOR_F32(0xD17434FF)},
        {Color::Drag_Middleground_Active,       NXR_COLOR_F32(0xE47B2FFF)},
        {Color::Drag_Foreground,                NXR_COLOR_F32(0xED7E2DFF)},
        {Color::Drag_Foreground_Hover,          NXR_COLOR_F32(0xF2802BFF)},
        {Color::Drag_Foreground_Active,         NXR_COLOR_F32(0x30231DFF)},

        {Color::Glow_Popup_In,                  NXR_COLOR_F32(0xE47B2F64)},
        {Color::Glow_Popup_Out,                 NXR_COLOR_F32(0xE47B2F00)},

        {Color::Glow_Popup_Warning_In,          NXR_COLOR_F32(0x563B2C96)},
        {Color::Glow_Popup_Warning_Out,         NXR_COLOR_F32(0xFF808000)},

        {Color::Glow_Popup_Critical_In,         NXR_COLOR_F32(0xFF808096)},
        {Color::Glow_Popup_Critical_Out,        NXR_COLOR_F32(0xFF808000)},

        {Color::Glow_Popup_Update_In,           NXR_COLOR_F32(0x563B2C96)},
        {Color::Glow_Popup_Update_Out,          NXR_COLOR_F32(0xFF842600)},
    };

    std::map<ImGuiCol_, ImVec4> nativeColorTable = {
        {ImGuiCol_Text,                  NXR_COLOR_F32(0xE1E1E1E1)},
        {ImGuiCol_TextDisabled,          NXR_COLOR_F32(0xC77136FF)},

        {ImGuiCol_WindowBg,              NXR_COLOR_F32(0x30231DFA)},
        {ImGuiCol_Border,                NXR_COLOR_F32(0x404040FF)},

        {ImGuiCol_TitleBg,               NXR_COLOR_F32(0x412E24FF)},
        {ImGuiCol_TitleBgActive,         NXR_COLOR_F32(0x412E24FF)},
        {ImGuiCol_TitleBgCollapsed,      NXR_COLOR_F32(0x412E24FF)},

        {ImGuiCol_ScrollbarBg,           NXR_COLOR_F32(0x30231D00)},

        {ImGuiCol_FrameBg,               NXR_COLOR_F32(0x563B2CFF)},
        {ImGuiCol_FrameBgHovered,        NXR_COLOR_F32(0xB56938FF)},
        {ImGuiCol_FrameBgActive,         NXR_COLOR_F32(0x493328FF)},

        {ImGuiCol_ScrollbarGrab,         NXR_COLOR_F32(0xFFFFFF98)},
        {ImGuiCol_ScrollbarGrabHovered,  NXR_COLOR_F32(0xFFFFFF98)},
        {ImGuiCol_ScrollbarGrabActive,   NXR_COLOR_F32(0xFFFFFF98)},

        {ImGuiCol_ResizeGrip,            NXR_COLOR_F32(0x563B2CFF)},
        {ImGuiCol_ResizeGripHovered,     NXR_COLOR_F32(0xB56938FF)},
        {ImGuiCol_ResizeGripActive,      NXR_COLOR_F32(0xF2802BFF)},

        {ImGuiCol_PopupBg,               NXR_COLOR_F32(0x30231DFA)},
        {ImGuiCol_ModalWindowDimBg,      NXR_COLOR_F32(0x0000007F)},

        {ImGuiCol_Button,                NXR_COLOR_F32(0x563B2CFF)},
        {ImGuiCol_ButtonHovered,         NXR_COLOR_F32(0xB56938FF)},
        {ImGuiCol_ButtonActive,          NXR_COLOR_F32(0x493328FF)},

        {ImGuiCol_Header,                NXR_COLOR_F32(0x563B2CFF)},
        {ImGuiCol_HeaderHovered,         NXR_COLOR_F32(0xB56938FF)},
        {ImGuiCol_HeaderActive,          NXR_COLOR_F32(0x493328FF)},

        {ImGuiCol_Tab,                   NXR_COLOR_F32(0x493328FF)},
        {ImGuiCol_TabHovered,            NXR_COLOR_F32(0x563B2CFF)},
        {ImGuiCol_TabActive,             NXR_COLOR_F32(0x945B38FF)},

        {ImGuiCol_TableHeaderBg,         NXR_COLOR_F32(0x412E24FF)},
        {ImGuiCol_TableBorderStrong,     NXR_COLOR_F32(0x563B2CFF)},
        {ImGuiCol_TableBorderLight,      NXR_COLOR_F32(0x493328FF)},

        {ImGuiCol_TableRowBg,            NXR_COLOR_F32(0x30231DFA)},
        {ImGuiCol_TableRowBgAlt,         NXR_COLOR_F32(0x3B2A22FF)},

        {ImGuiCol_Separator,             NXR_COLOR_F32(0xB56938FF)}
    };

    void ProccessOriginalTheme(bool save) {
        static std::map<Color, ImVec4> table1;
        static std::map<ImGuiCol_, ImVec4> table2;

        if (save) {
            table1 = colorTable;
            table2 = nativeColorTable;
        }
        else if (!table1.empty()) {
            colorTable = table1;
            nativeColorTable = table2;
            ApplyNativeGuiColors();
        }
    }

    static const char* GetCustomColorName(Color color) {
        switch (color) {
            case Color::Button_Background:             return "Button Background";
            case Color::Button_Background_Hover:       return "Button Background Hover";
            case Color::Button_Background_Active:      return "Button Background Active";
            case Color::Button_Foreground:             return "Button Foreground";
            case Color::Button_Foreground_Hover:       return "Button Foreground Hover";
            case Color::Button_Foreground_Active:      return "Button Foreground Active";
            case Color::Button_Border:                 return "Button Border";
            case Color::Checkbox_Background_Off:       return "Checkbox Background Off";
            case Color::Checkbox_Background_Off_Hover: return "Checkbox Background Off Hover";
            case Color::Checkbox_Background_Off_Active:return "Checkbox Background Off Active";
            case Color::Checkbox_Background_On:        return "Checkbox Background On";
            case Color::Checkbox_Background_On_Hover:  return "Checkbox Background On Hover";
            case Color::Checkbox_Background_On_Active: return "Checkbox Background On Active";
            case Color::Checkbox_Knob_Off:             return "Checkbox Knob Off";
            case Color::Checkbox_Knob_Off_Hover:       return "Checkbox Knob Off Hover";
            case Color::Checkbox_Knob_Off_Active:      return "Checkbox Knob Off Active";
            case Color::Checkbox_Knob_On:              return "Checkbox Knob On";
            case Color::Checkbox_Knob_On_Hover:        return "Checkbox Knob On Hover";
            case Color::Checkbox_Knob_On_Active:       return "Checkbox Knob On Active";
            case Color::Checkbox_Knob_Shadow:          return "Checkbox Knob Shadow";
            case Color::Checkbox_Border:               return "Checkbox Border";
            case Color::RadioButton_Ring_Off:          return "Radio Ring Off";
            case Color::RadioButton_Ring_Off_Hover:    return "Radio Ring Off Hover";
            case Color::RadioButton_Ring_Off_Active:   return "Radio Ring Off Active";
            case Color::RadioButton_Ring_On:           return "Radio Ring On";
            case Color::RadioButton_Ring_On_Hover:     return "Radio Ring On Hover";
            case Color::RadioButton_Ring_On_Active:    return "Radio Ring On Active";
            case Color::RadioButton_Dot_Off:           return "Radio Dot Off";
            case Color::RadioButton_Dot_On:            return "Radio Dot On";
            case Color::RadioButton_Dot_On_Hover:      return "Radio Dot On Hover";
            case Color::RadioButton_Dot_On_Active:     return "Radio Dot On Active";
            case Color::RadioButton_Layer_Off_Hover:   return "Radio Layer Off Hover";
            case Color::RadioButton_Layer_Off_Active:  return "Radio Layer Off Active";
            case Color::RadioButton_Layer_On_Hover:    return "Radio Layer On Hover";
            case Color::RadioButton_Layer_On_Active:   return "Radio Layer On Active";
            case Color::Tooltip_Background:            return "Tooltip Background";
            case Color::Tooltip_Foreground:            return "Tooltip Foreground";
            case Color::Tooltip_Border:                return "Tooltip Border";
            case Color::Drag_Background:               return "Drag Background";
            case Color::Drag_Background_Hover:         return "Drag Background Hover";
            case Color::Drag_Background_Active:        return "Drag Background Active";
            case Color::Drag_Middleground:             return "Drag Middleground";
            case Color::Drag_Middleground_Hover:       return "Drag Middleground Hover";
            case Color::Drag_Middleground_Active:      return "Drag Middleground Active";
            case Color::Drag_Foreground:               return "Drag Foreground";
            case Color::Drag_Foreground_Hover:         return "Drag Foreground Hover";
            case Color::Drag_Foreground_Active:        return "Drag Foreground Active";

            case Color::Glow_Popup_In:                 return "Glow Popup In";
            case Color::Glow_Popup_Out:                return "Glow Popup Out";
            case Color::Glow_Popup_Warning_In:         return "Glow Popup Warning In";
            case Color::Glow_Popup_Warning_Out:        return "Glow Popup Warning Out";
            case Color::Glow_Popup_Critical_In:        return "Glow Popup Critical In";
            case Color::Glow_Popup_Critical_Out:       return "Glow Popup Critical Out";
            case Color::Glow_Popup_Update_In:          return "Glow Popup Update In";
            case Color::Glow_Popup_Update_Out:         return "Glow Popup Update Out";
            default:                                   return "Unknown Custom Color";
        }
    }

    inline bool isLegacyRender() {
        static const bool isLegacy = geode::Mod::get()->getSettingValue<bool>("nxr-legacy-render");
        return isLegacy;
    }

    void ApplyNativeGuiColors() {
        auto* colors = ImGui::GetStyle().Colors;

        for (const auto& [id, color] : nativeColorTable) {
            colors[id] = color;
        }
    }

    void SaveTheme() {
        nlohmann::json json;

        for (const auto& [id, color] : colorTable) {
            std::string name = GetCustomColorName(id);
            json["custom"][name] = { color.x, color.y, color.z, color.w };
        }

        for (const auto& [id, color] : nativeColorTable) {
            std::string name = ImGui::GetStyleColorName(id);
            json["native"][name] = { color.x, color.y, color.z, color.w };
        }

        std::ofstream outFile(themeFilePath);
        if (outFile.is_open()) {
            outFile << json.dump(4);
            outFile.close();
        }
    }

    void LoadTheme() {
        std::ifstream inFile(themeFilePath);
        if (!inFile.is_open()) return;

        std::string file_contents((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
        inFile.close();

        nlohmann::json json = nlohmann::json::parse(file_contents, nullptr, false);
        if (json.is_discarded()) {
            return;
        }

        if (json.contains("custom") && json["custom"].is_object()) {
            for (auto& [id, color] : colorTable) {
                std::string name = GetCustomColorName(id);
                if (json["custom"].contains(name)) {
                    auto& val = json["custom"][name];
                    if (val.is_array() && val.size() == 4) {
                        color = ImVec4(val[0], val[1], val[2], val[3]);
                    }
                }
            }
        }

        if (json.contains("native") && json["native"].is_object()) {
            for (auto& [id, color] : nativeColorTable) {
                std::string name = ImGui::GetStyleColorName(id);
                if (json["native"].contains(name)) {
                    auto& val = json["native"][name];
                    if (val.is_array() && val.size() == 4) {
                        color = ImVec4(val[0], val[1], val[2], val[3]);
                    }
                }
            }
            ApplyNativeGuiColors();
        }
    }

    void DrawColorEditor() {
        auto& layout = NXR::Layout::Manager::get();
        float fontSize = layout.multipleScale(32.0f);
        float baseWidth = layout.multipleScale(400.0f);
        float paddingBetween = layout.multipleScale(20.0f);

        ImVec2 screenSize = ImGui::GetIO().DisplaySize;
        float windowHeight = screenSize.y * 0.75f;
        float totalBlockWidth = (baseWidth * 2.0f) + paddingBetween;
        float startX = (screenSize.x - totalBlockWidth) * 0.5f;
        float startY = (screenSize.y - windowHeight) * 0.5f;

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

        ImGui::SetNextWindowPos(ImVec2(startX, startY));
        ImGui::SetNextWindowSize(ImVec2(baseWidth, windowHeight));

        if (ImGui::Begin("Widget Preview", nullptr, windowFlags)) {
            float oldScale = ImGui::GetFont()->Scale;
            ImGui::GetFont()->Scale = fontSize / ImGui::GetFontSize();
            ImGui::PushFont(ImGui::GetFont());
            ImGui::TextColored(ImVec4(0.239f, 0.949f, 0.592f, 1.0f), "NXR UI Preview");
            ImGui::GetFont()->Scale = oldScale;
            ImGui::PopFont();

            ImGui::Separator();
            ImGui::Spacing();

            NXRWidget::Button("Custom Button", ImVec2(layout.multipleScale(150.0f), 0));
            ImGui::SameLine();
            NXRWidget::ArrowButton("##prev_arrow_l", ImGuiDir_Left);
            ImGui::SameLine();
            NXRWidget::ArrowButton("##prev_arrow_r", ImGuiDir_Right);
            ImGui::Spacing();
            ImGui::Button("Native ImGui Button", ImVec2(layout.multipleScale(180.0f), 0));

            static bool chk1 = true, chk2 = false;
            NXRWidget::Checkbox("Toggle Switch Active", &chk1);
            NXRWidget::Checkbox("Toggle Switch Inactive", &chk2);
            SpaceSeparator();

            static int radioVal = 0;
            if (NXRWidget::RadioButton("Radio Option 1", radioVal == 0)) radioVal = 0;
            if (NXRWidget::RadioButton("Radio Option 2", radioVal == 1)) radioVal = 1;

            static float fVal1 = 0.5f;
            static int iVal1 = 50;
            NXRWidget::DragFloat("Custom Drag Float", &fVal1, 0.01f, 0.0f, 1.0f);
            NXRWidget::DragInt("Custom Drag Int", &iVal1, 1, 0, 100);

            static int comboIdx = 0;
            const char* items[] = { "Apple", "Banana", "Cherry", "Kiwi" };
            ImGui::Combo("Native Combo", &comboIdx, items, IM_ARRAYSIZE(items));
            SpaceSeparator();

            ImGui::Text("Hover here for Tooltip");
            NXRWidget::Tooltip("This is your tooltip window!", ImGui::IsItemHovered());
            ImGui::Spacing();

            SpaceSeparator();

            ImGui::Text("Table Example:");

            if (ImGui::BeginTable("simple_table", 3, ImGuiTableFlags_Borders))
            {
                ImGui::TableSetupColumn("Fruits");
                ImGui::TableSetupColumn("Vegetables");
                ImGui::TableSetupColumn("Other");
                ImGui::TableHeadersRow();

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Apple");
                ImGui::TableNextColumn(); ImGui::Text("Carrot");
                ImGui::TableNextColumn(); ImGui::Text("Bread");

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Banana");
                ImGui::TableNextColumn(); ImGui::Text("Cucumber");
                ImGui::TableNextColumn(); ImGui::Text("Milk");

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Orange");
                ImGui::TableNextColumn(); ImGui::Text("Tomato");
                ImGui::TableNextColumn(); ImGui::Text("Cheese");

                ImGui::EndTable();
            }

            SpaceSeparator();

            if (ImGui::BeginTabBar("ExampleTabBar")) {
                if (ImGui::BeginTabItem("Example 1")) {
                    ImGui::Spacing();
                    ImGui::Text("Hello world! First tab");
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Example 2")) {
                    ImGui::Spacing();
                    ImGui::Text("Hello world! Second tab");
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }

            SpaceSeparator();

            if (NXRWidget::Button("Show Popup for Glow Test")) {
                ImGui::OpenPopup("Glow Test");
            }

            if (ImGui::BeginPopupModal("Glow Test", 0, ImGuiWindowFlags_AlwaysAutoResize)) {
                static int color_type = 0;
                auto& layout = NXR::Layout::Manager::get();

                auto glow_in = ImGui::ColorConvertFloat4ToU32(NXRWidget::colorTable[NXRWidget::Glow_Popup_In]);
                auto glow_out = ImGui::ColorConvertFloat4ToU32(NXRWidget::colorTable[NXRWidget::Glow_Popup_Out]);

                if (color_type == 1) {
                    glow_in = ImGui::ColorConvertFloat4ToU32(NXRWidget::colorTable[NXRWidget::Glow_Popup_Warning_In]);
                    glow_out = ImGui::ColorConvertFloat4ToU32(NXRWidget::colorTable[NXRWidget::Glow_Popup_Warning_Out]);
                }
                else if (color_type == 2) {
                    glow_in = ImGui::ColorConvertFloat4ToU32(NXRWidget::colorTable[NXRWidget::Glow_Popup_Critical_In]);
                    glow_out = ImGui::ColorConvertFloat4ToU32(NXRWidget::colorTable[NXRWidget::Glow_Popup_Critical_Out]);
                }
                else if (color_type == 3) {
                    glow_in = ImGui::ColorConvertFloat4ToU32(NXRWidget::colorTable[NXRWidget::Glow_Popup_Update_In]);
                    glow_out = ImGui::ColorConvertFloat4ToU32(NXRWidget::colorTable[NXRWidget::Glow_Popup_Update_Out]);
                }
                NXRWidget::GlowWindow(glow_in, glow_out, layout.multipleScale(500.f));

                if (NXRWidget::RadioButton("Glow Default", color_type == 0)) {
                    color_type = 0;
                } ImGui::SameLine();

                if (NXRWidget::RadioButton("Glow Warning", color_type == 1)) {
                    color_type = 1;
                } ImGui::SameLine();

                if (NXRWidget::RadioButton("Glow Critical Warning", color_type == 2)) {
                    color_type = 2;
                } ImGui::SameLine();

                if (NXRWidget::RadioButton("Glow Update", color_type == 3)) {
                    color_type = 3;
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Text("Glow Color Palette:");
                ImGui::Spacing();

                if (color_type == 0) {
                    ImGui::ColorEdit4("Glow In", &NXRWidget::colorTable[NXRWidget::Glow_Popup_In].x, ImGuiColorEditFlags_NoInputs);
                    ImGui::ColorEdit4("Glow Out", &NXRWidget::colorTable[NXRWidget::Glow_Popup_Out].x, ImGuiColorEditFlags_NoInputs);
                }
                else if (color_type == 1) {
                    ImGui::ColorEdit4("Glow Warning In", &NXRWidget::colorTable[NXRWidget::Glow_Popup_Warning_In].x, ImGuiColorEditFlags_NoInputs);
                    ImGui::ColorEdit4("Glow Warning Out", &NXRWidget::colorTable[NXRWidget::Glow_Popup_Warning_Out].x, ImGuiColorEditFlags_NoInputs);
                }
                else if (color_type == 2) {
                    ImGui::ColorEdit4("Glow Critical In", &NXRWidget::colorTable[NXRWidget::Glow_Popup_Critical_In].x, ImGuiColorEditFlags_NoInputs);
                    ImGui::ColorEdit4("Glow Critical Out", &NXRWidget::colorTable[NXRWidget::Glow_Popup_Critical_Out].x, ImGuiColorEditFlags_NoInputs);
                }
                else if (color_type == 3) {
                    ImGui::ColorEdit4("Glow Update In", &NXRWidget::colorTable[NXRWidget::Glow_Popup_Update_In].x, ImGuiColorEditFlags_NoInputs);
                    ImGui::ColorEdit4("Glow Update Out", &NXRWidget::colorTable[NXRWidget::Glow_Popup_Update_Out].x, ImGuiColorEditFlags_NoInputs);
                }

                ImGui::Spacing();

                if (NXRWidget::Button("Close")) {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(startX + baseWidth + paddingBetween, startY));
        ImGui::SetNextWindowSize(ImVec2(baseWidth, windowHeight));

        if (ImGui::Begin("Color Palette Editor", nullptr, windowFlags)) {
            if (NXRWidget::Button("Import")) {
                geode::async::spawn(
                    geode::utils::file::pick(
                        geode::utils::file::PickMode::OpenFile,
                        geode::utils::file::FilePickOptions{
                            .filters = { { "JSON Theme File", { "*.json" } } }
                        }
                    ),
                    [](geode::utils::file::PickResult res) {
                        if (!res) return;
                        auto pathOpt = std::move(res).unwrapOr(std::nullopt);
                        if (!pathOpt) return;

                        auto selectedPath = *pathOpt;
                        if (!NXR::Utils::isOnlyAsciiPath(selectedPath)) {
                            AddPopup("Invalid path. Please remove any Cyrillic characters");
                            return;
                        }

                        if (selectedPath.extension() != ".json") {
                            AddPopup("Invalid file format. Only .json files are allowed");
                            return;
                        }

                        std::error_code ec;
                        std::filesystem::copy_file(selectedPath, themeFilePath, std::filesystem::copy_options::overwrite_existing, ec);
                        if (ec) {
                            AddPopup("Failed to copy theme file");
                            return;
                        }

                        LoadTheme();
                        AddPopup("Theme imported successfully!");
                    }
                );
            }
            ImGui::SameLine();

            if (NXRWidget::Button("Export")) {
                SaveTheme();
                geode::async::spawn(
                    geode::utils::file::pick(
                        geode::utils::file::PickMode::SaveFile,
                        geode::utils::file::FilePickOptions{
                            .defaultPath = "theme.json",
                            .filters = { { "JSON Theme File", { "*.json" } } }
                        }
                    ),
                    [](geode::utils::file::PickResult res) {
                        if (!res) return;
                        auto pathOpt = std::move(res).unwrapOr(std::nullopt);
                        if (!pathOpt) return;

                        auto selectedPath = *pathOpt;
                        if (!NXR::Utils::isOnlyAsciiPath(selectedPath)) {
                            AddPopup("Invalid path. Please remove any Cyrillic characters");
                            return;
                        }

                        std::error_code ec;
                        std::filesystem::copy_file(themeFilePath, selectedPath, std::filesystem::copy_options::overwrite_existing, ec);
                        if (ec) {
                            AddPopup("Failed to export theme file");
                            return;
                        }

                        AddPopup("Theme exported successfully!");
                    }
                );
            }
            ImGui::SameLine();

            if (NXRWidget::Button("Reset")) {
                NXRWidget::ProccessOriginalTheme(false);
            }
            ImGui::BeginChild("PaletteScrollArea", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

            ImGui::Text("Custom Widget Colors");
            ImGui::Separator(); ImGui::Spacing();

            for (auto& [id, color] : colorTable) {
                ImGui::PushID((int)id);
                ImGui::ColorEdit4(GetCustomColorName(id), &color.x, ImGuiColorEditFlags_NoInputs);
                ImGui::PopID();
            }

            ImGui::Spacing(); ImGui::Spacing();
            ImGui::Text("Native ImGui Colors");
            ImGui::Separator(); ImGui::Spacing();

            bool nativeChanged = false;
            for (auto& [id, color] : nativeColorTable) {
                ImGui::PushID(id);
                if (ImGui::ColorEdit4(ImGui::GetStyleColorName(id), &color.x, ImGuiColorEditFlags_NoInputs)) nativeChanged = true;
                ImGui::PopID();
            }

            if (nativeChanged) ApplyNativeGuiColors();

            ImGui::EndChild();
        }
        ImGui::End();
    }

    void ApplyStyle(float scale) {
        ImGuiStyle& style = ImGui::GetStyle();
        ImGuiIO& io = ImGui::GetIO();
        io.FontGlobalScale = scale;

        style.WindowPadding = ImVec2(10.f * scale, 10.f * scale);
        style.FramePadding = ImVec2(4.f * scale, 3.f * scale);
        style.ItemSpacing = ImVec2(8.f * scale, 4.f * scale);
        style.ItemInnerSpacing = ImVec2(4.f * scale, 4.f * scale);
        style.IndentSpacing = 21.f * scale;
        style.ScrollbarSize = 12.f * scale;
        style.GrabMinSize = 12.f * scale;

        const bool legacy = isLegacyRender();

        if (legacy) {
            style.AntiAliasedFill = false;
            style.AntiAliasedLines = false;
            style.AntiAliasedLinesUseTex = false;
        }

        style.WindowRounding    = legacy ? 0.0f : 12.0f * scale;
        style.ChildRounding     = legacy ? 0.0f : 6.0f * scale;
        style.FrameRounding     = legacy ? 0.0f : 20.0f * scale;
        style.PopupRounding     = legacy ? 0.0f : 12.0f * scale;
        style.ScrollbarRounding = legacy ? 0.0f : 12.0f * scale;
        style.GrabRounding      = legacy ? 0.0f : 12.0f * scale;
        style.TabRounding       = legacy ? 0.0f : 6.0f * scale;

        style.WindowBorderSize = 0;
        style.WindowTitleAlign = ImVec2(0.50f, 0.50f);
        style.WindowMenuButtonPosition = ImGuiDir_Right;
    }

    static inline void SetColorHSV(Color cid, ImVec4 hsv) {
        float r, g, b; ImGui::ColorConvertHSVtoRGB(hsv.x, hsv.y, hsv.z, r, g, b);
        colorTable[cid] = ImVec4 { r, g, b, hsv.w };
    }

    static inline void SetImGuiColorHSV(ImGuiCol cid, ImVec4 hsv) {
        auto *colors = ImGui::GetStyle().Colors;
        float r, g, b; ImGui::ColorConvertHSVtoRGB(hsv.x, hsv.y, hsv.z, r, g, b);
        colors[cid] = ImVec4 { r, g, b, hsv.w };
    }

    static inline ImU32 ToU32(ImVec4 c, float global_alpha)
    {
        c.w *= global_alpha;
        return ImGui::ColorConvertFloat4ToU32(c);
    }

    static inline ImU32 ToU32A(ImVec4 c, float global_alpha, float anim_alpha)
    {
        c.w *= global_alpha * anim_alpha;
        return ImGui::ColorConvertFloat4ToU32(c);
    }

    static inline ImVec4 LerpC(ImVec4 a, ImVec4 b, float t)
    {
        return { a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t, a.w+(b.w-a.w)*t };
    }

    bool Button(const char* label, const ImVec2& size_arg)
    {
        if (isLegacyRender()) {
            ImGui::PushStyleColor(ImGuiCol_Button, colorTable[Color::Button_Background]);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colorTable[Color::Button_Background_Hover]);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, colorTable[Color::Button_Background_Active]);
            ImGui::PushStyleColor(ImGuiCol_Text, colorTable[Color::Button_Foreground]);

            bool ret = ImGui::Button(label, size_arg);

            ImGui::PopStyleColor(4);
            return ret;
        }

        struct ButtonState { float th, tHeld; };
        static std::unordered_map<ImGuiID, ButtonState> s_states;

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiIO& io = g.IO;
        const float alpha = style.Alpha;

        const ImGuiID id = window->GetID(label);

        const char* lend = ImGui::FindRenderedTextEnd(label);
        const ImVec2 label_sz = ImGui::CalcTextSize(label, lend, false);
        const ImVec2 pad = style.FramePadding;
        const ImVec2 total_sz = ImGui::CalcItemSize(size_arg, label_sz.x + pad.x * 2.f, label_sz.y + pad.y * 2.f);

        const ImVec2 pos = window->DC.CursorPos;
        const ImRect bb = { pos, pos + total_sz };
        ImGui::ItemSize(total_sz, pad.y);
        if (!ImGui::ItemAdd(bb, id)) return false;

        auto& st = s_states.emplace(id, ButtonState{ 0.f, 0.f }).first->second;

        bool hovered, held;
        const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        if (pressed) ImGui::MarkItemEdited(id);

        auto Lerp = [&](float& val, float tgt, float spd) {
            val += (tgt - val) * ImMin(1.f, io.DeltaTime * spd);
            if (ImAbs(val - tgt) < 0.004f) val = tgt; else ImGui::MarkItemEdited(id);
        };
        Lerp(st.th, (hovered || held) ? 1.f : 0.f,  9.f);
        Lerp(st.tHeld, held ? 1.f : 0.f, held ? 10.f : 6.f);

        auto Smootherstep = [](float t) -> float {
            t = ImClamp(t, 0.f, 1.f);
            return t * t * t * (t * (t * 6.f - 15.f) + 10.f);
        };

        const float easedHeld = Smootherstep(st.tHeld);
        const float rnd = style.FrameRounding * (1.f - easedHeld * 0.8f);

        const ImVec4 col_bg_idle = colorTable[Color::Button_Background];
        const ImVec4 col_bg_hov = colorTable[Color::Button_Background_Hover];
        const ImVec4 col_bg_held = colorTable[Color::Button_Background_Active];
        const ImVec4 col_txt_idle = colorTable[Color::Button_Foreground];
        const ImVec4 col_txt_hov = colorTable[Color::Button_Foreground_Hover];
        const ImVec4 col_txt_held = colorTable[Color::Button_Foreground_Active];
        const ImVec4 col_border = colorTable[Color::Button_Border];

        const ImVec4 bg_final = LerpC(LerpC(col_bg_idle,  col_bg_hov,  st.th), col_bg_held,  easedHeld);
        const ImVec4 txt_final = LerpC(LerpC(col_txt_idle, col_txt_hov, st.th), col_txt_held, easedHeld);

        ImDrawList* dl = window->DrawList;

        dl->AddRectFilled(
            { bb.Min.x + 1.f, bb.Min.y + 2.f },
            { bb.Max.x + 1.f, bb.Max.y + 2.f },
            ImGui::ColorConvertFloat4ToU32({ 0.f, 0.f, 0.f, 0.14f * alpha * (1.f - easedHeld) }), rnd
        );

        dl->AddRectFilled(bb.Min, bb.Max, ToU32(bg_final, alpha), rnd);

        if (col_border.w > 0.f)
            dl->AddRect(bb.Min, bb.Max, ToU32(col_border, alpha),
                        rnd, 0, style.FrameBorderSize > 0.f ? style.FrameBorderSize : 1.f);

        const ImVec2 text_pos = {
            bb.Min.x + (total_sz.x - label_sz.x) * 0.5f,
            bb.Min.y + (total_sz.y - label_sz.y) * 0.5f
        };
        dl->AddText(text_pos, ToU32(txt_final, alpha), label, lend);

        return pressed;
    }

    bool Checkbox(const char* label, bool* v)
    {
        if (isLegacyRender()) {
            const ImVec4 frameBg    = *v ? colorTable[Color::Checkbox_Background_On]        : colorTable[Color::Checkbox_Background_Off];
            const ImVec4 frameBgHov = *v ? colorTable[Color::Checkbox_Background_On_Hover]  : colorTable[Color::Checkbox_Background_Off_Hover];
            const ImVec4 frameBgAct = *v ? colorTable[Color::Checkbox_Background_On_Active] : colorTable[Color::Checkbox_Background_Off_Active];
            const ImVec4 checkCol   = *v ? colorTable[Color::Checkbox_Knob_On]              : colorTable[Color::Checkbox_Knob_Off];

            ImGui::PushStyleColor(ImGuiCol_FrameBg,            frameBg);
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,     frameBgHov);
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive,      frameBgAct);
            ImGui::PushStyleColor(ImGuiCol_CheckMark,          checkCol);
            ImGui::PushStyleColor(ImGuiCol_CheckboxSelectedBg, frameBg);

            bool ret = ImGui::Checkbox(label, v);

            ImGui::PopStyleColor(5);
            return ret;
        }

        struct ToggleState { float t, th, tHeld; };
        static std::unordered_map<ImGuiID, ToggleState> s_states;

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiIO& io = g.IO;
        const float alpha = style.Alpha;

        const float H = ImGui::GetFrameHeight();
        const float W = H * 1.8f;
        const float KR = H * 0.5f - 2.5f;

        const char* lend = ImGui::FindRenderedTextEnd(label);
        const ImVec2 label_sz = ImGui::CalcTextSize(label, lend, true);
        const ImVec2 total_sz = { W + style.ItemInnerSpacing.x + label_sz.x, H };

        const ImVec2 pos = window->DC.CursorPos;
        const ImRect total_bb = { pos, pos + total_sz };
        const ImRect pill_bb = { pos, { pos.x + W, pos.y + H } };
        ImGui::ItemSize(total_sz, style.FramePadding.y);
        if (!ImGui::ItemAdd(total_bb, ImGui::GetID(label))) return false;

        const ImGuiID id = ImGui::GetID(label);
        auto& st = s_states.emplace(id, ToggleState{ *v ? 1.f : 0.f, 0.f, 0.f }).first->second;

        bool hovered, held;
        const bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
        bool changed = false;
        if (pressed) { *v = !*v; changed = true; ImGui::MarkItemEdited(id); }

        auto Lerp = [&](float& val, float tgt, float spd) {
            val += (tgt - val) * ImMin(1.f, io.DeltaTime * spd);
            if (ImAbs(val - tgt) < 0.004f) val = tgt; else ImGui::MarkItemEdited(id);
        };
        Lerp(st.t, *v ? 1.f : 0.f, 14.f);
        Lerp(st.th, (hovered || held) ? 1.f : 0.f,  9.f);
        Lerp(st.tHeld, held ? 1.f : 0.f, 18.f);

        const ImVec4 col_bg_off = colorTable[Color::Checkbox_Background_Off];
        const ImVec4 col_bg_off_hov = colorTable[Color::Checkbox_Background_Off_Hover];
        const ImVec4 col_bg_off_held = colorTable[Color::Checkbox_Background_Off_Active];
        const ImVec4 col_bg_on = colorTable[Color::Checkbox_Background_On];
        const ImVec4 col_bg_on_hov = colorTable[Color::Checkbox_Background_On_Hover];
        const ImVec4 col_bg_on_held = colorTable[Color::Checkbox_Background_On_Active];
        const ImVec4 col_knob_off = colorTable[Color::Checkbox_Knob_Off];
        const ImVec4 col_knob_off_hov = colorTable[Color::Checkbox_Knob_Off_Hover];
        const ImVec4 col_knob_off_held = colorTable[Color::Checkbox_Knob_Off_Active];
        const ImVec4 col_knob_on = colorTable[Color::Checkbox_Knob_On];
        const ImVec4 col_knob_on_hov = colorTable[Color::Checkbox_Knob_On_Hover];
        const ImVec4 col_knob_on_held = colorTable[Color::Checkbox_Knob_On_Active];
        const ImVec4 col_knob_shadow = colorTable[Color::Checkbox_Knob_Shadow];
        const ImVec4 col_border = colorTable[Color::Checkbox_Border];

        const ImVec4 bg_base = LerpC(col_bg_off, col_bg_on, st.t);
        const ImVec4 bg_hov = LerpC(col_bg_off_hov, col_bg_on_hov, st.t);
        const ImVec4 bg_held = LerpC(col_bg_off_held, col_bg_on_held, st.t);
        const ImVec4 bg_final = LerpC(LerpC(bg_base, bg_hov, st.th), bg_held, st.tHeld);

        const ImVec4 kn_base = LerpC(col_knob_off, col_knob_on, st.t);
        const ImVec4 kn_hov = LerpC(col_knob_off_hov, col_knob_on_hov, st.t);
        const ImVec4 kn_held = LerpC(col_knob_off_held, col_knob_on_held, st.t);
        const ImVec4 kn_final = LerpC(LerpC(kn_base, kn_hov, st.th), kn_held, st.tHeld);

        ImDrawList* dl  = window->DrawList;
        const float rnd = H * 0.5f;

        dl->AddRectFilled(pill_bb.Min, pill_bb.Max, ToU32(bg_final, alpha), rnd);

        if (col_border.w > 0.f)
            dl->AddRect(pill_bb.Min, pill_bb.Max, ToU32(col_border, alpha),
                        rnd, 0, style.FrameBorderSize > 0.f ? style.FrameBorderSize : 1.f);

        const float kr_final = KR - st.tHeld * 1.2f;
        const float kx = pos.x + KR + 2.5f + (W - H) * st.t;
        const float ky = pos.y + H * 0.5f;

        if (col_knob_shadow.w > 0.f)
        {
            ImVec4 drop_shadow = col_knob_shadow;
            dl->AddCircleFilled({ kx, ky + 0.8f }, kr_final + 1.f, ToU32(drop_shadow, alpha), 32);
        }

        dl->AddCircleFilled({ kx, ky }, kr_final, ToU32(kn_final, alpha), 32);

        if (col_knob_shadow.w > 0.f)
        {
            ImVec4 outline_shadow = col_knob_shadow;
            dl->AddCircle({ kx, ky }, kr_final, ToU32(outline_shadow, 0.45f * alpha), 32, 1.f);
        }

        if (lend > label)
            dl->AddText({ pos.x + W + style.ItemInnerSpacing.x, pos.y + style.FramePadding.y },
                        ImGui::GetColorU32(ImGuiCol_Text), label, lend);

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
        return changed;
    }

    bool RadioButton(const char* label, bool active)
    {
        if (isLegacyRender()) {
            const ImVec4 frameBg    = active ? colorTable[Color::RadioButton_Ring_On]        : colorTable[Color::RadioButton_Ring_Off];
            const ImVec4 frameBgHov = active ? colorTable[Color::RadioButton_Ring_On_Hover]  : colorTable[Color::RadioButton_Ring_Off_Hover];
            const ImVec4 frameBgAct = active ? colorTable[Color::RadioButton_Ring_On_Active] : colorTable[Color::RadioButton_Ring_Off_Active];
            const ImVec4 checkCol   = active ? colorTable[Color::RadioButton_Dot_On]         : colorTable[Color::RadioButton_Dot_Off];

            ImGui::PushStyleColor(ImGuiCol_FrameBg,            frameBg);
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,     frameBgHov);
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive,      frameBgAct);
            ImGui::PushStyleColor(ImGuiCol_CheckMark,          checkCol);

            bool ret = ImGui::RadioButton(label, active);

            ImGui::PopStyleColor(4);
            return ret;
        }

        struct RadioState { float t, th, tHeld; };
        static std::unordered_map<ImGuiID, RadioState> s_states;

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g     = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiIO&    io    = g.IO;
        const float alpha = style.Alpha;

        const float H  = ImGui::GetFrameHeight();
        const float OR = H * 0.5f;
        const float BW = 2.f;
        const float IR = OR * 0.50f;

        const float SR = OR * 1.35f;

        const char* lend      = ImGui::FindRenderedTextEnd(label);
        const ImVec2 label_sz = ImGui::CalcTextSize(label, lend, true);
        const ImVec2 total_sz = { H + style.ItemInnerSpacing.x + label_sz.x, H };
        const ImVec2 pos      = window->DC.CursorPos;
        const ImRect total_bb = { pos, pos + total_sz };
        const ImRect radio_bb = { pos, { pos.x + H, pos.y + H } };
        const ImVec2 center   = { pos.x + OR, pos.y + OR };

        ImGui::ItemSize(total_sz, style.FramePadding.y);
        const ImGuiID id = ImGui::GetID(label);
        if (!ImGui::ItemAdd(total_bb, id)) return false;

        auto& st = s_states.emplace(id, RadioState{ active ? 1.f : 0.f, 0.f, 0.f })
                            .first->second;

        bool hovered, held;
        const bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);

        auto Lerp = [&](float& val, float tgt, float spd) {
            val += (tgt - val) * ImMin(1.f, io.DeltaTime * spd);
            if (ImAbs(val - tgt) < 0.004f) val = tgt;
            else ImGui::MarkItemEdited(id);
        };

        Lerp(st.t,     active ? 1.f : 0.f,             14.f);
        Lerp(st.th,    (hovered || held) ? 1.f : 0.f,   9.f);
        Lerp(st.tHeld, held ? 1.f : 0.f,               18.f);

        const ImVec4 col_ring_off      = colorTable[Color::RadioButton_Ring_Off];
        const ImVec4 col_ring_off_hov  = colorTable[Color::RadioButton_Ring_Off_Hover];
        const ImVec4 col_ring_off_held = colorTable[Color::RadioButton_Ring_Off_Active];
        const ImVec4 col_ring_on       = colorTable[Color::RadioButton_Ring_On];
        const ImVec4 col_ring_on_hov   = colorTable[Color::RadioButton_Ring_On_Hover];
        const ImVec4 col_ring_on_held  = colorTable[Color::RadioButton_Ring_On_Active];

        const ImVec4 col_dot_off       = colorTable[Color::RadioButton_Dot_Off];
        const ImVec4 col_dot_on        = colorTable[Color::RadioButton_Dot_On];
        const ImVec4 col_dot_on_hov    = colorTable[Color::RadioButton_Dot_On_Hover];
        const ImVec4 col_dot_on_held   = colorTable[Color::RadioButton_Dot_On_Active];

        const ImVec4 col_layer_off_hov  = colorTable[Color::RadioButton_Layer_Off_Hover];
        const ImVec4 col_layer_off_held = colorTable[Color::RadioButton_Layer_Off_Active];
        const ImVec4 col_layer_on_hov   = colorTable[Color::RadioButton_Layer_On_Hover];
        const ImVec4 col_layer_on_held  = colorTable[Color::RadioButton_Layer_On_Active];

        const ImVec4 ring_base  = LerpC(col_ring_off,     col_ring_on,     st.t);
        const ImVec4 ring_hov   = LerpC(col_ring_off_hov,  col_ring_on_hov,  st.t);
        const ImVec4 ring_held  = LerpC(col_ring_off_held, col_ring_on_held, st.t);
        const ImVec4 ring_final = LerpC(LerpC(ring_base, ring_hov, st.th), ring_held, st.tHeld);

        const ImVec4 dot_base  = LerpC(col_dot_off, col_dot_on,     st.t);
        const ImVec4 dot_hov   = LerpC(col_dot_off, col_dot_on_hov,  st.t);
        const ImVec4 dot_held  = LerpC(col_dot_off, col_dot_on_held, st.t);
        const ImVec4 dot_final = LerpC(LerpC(dot_base, dot_hov, st.th), dot_held, st.tHeld);

        const ImVec4 lay_hov  = LerpC(col_layer_off_hov,  col_layer_on_hov,  st.t);
        const ImVec4 lay_held = LerpC(col_layer_off_held, col_layer_on_held, st.t);

        ImVec4 layer_final = LerpC(lay_hov, lay_held, st.tHeld);
        layer_final.w *= (st.th + st.tHeld * 0.5f);

        ImDrawList* dl = window->DrawList;

        if (layer_final.w > 0.002f)
            dl->AddCircleFilled(center, SR, ToU32(layer_final, alpha), 48);

        const float ring_w = BW + st.t * 0.4f;
        dl->AddCircle(center, OR - ring_w * 0.5f,
                    ToU32(ring_final, alpha), 48, ring_w);

        const float dot_r = IR * st.t * (1.f - st.tHeld * 0.12f);
        if (dot_r > 0.5f)
        {
            dl->AddCircleFilled({ center.x, center.y + 0.8f }, dot_r + 1.f,
                                ImGui::ColorConvertFloat4ToU32({ 0.f, 0.f, 0.f, 0.15f * alpha * st.t }),
                                32);
            dl->AddCircleFilled(center, dot_r, ToU32(dot_final, alpha), 48);
            dl->AddCircle(center, dot_r,
                        ImGui::ColorConvertFloat4ToU32({ 0.f, 0.f, 0.f, 0.06f * alpha }), 48, 1.f);
        }

        if (lend > label)
            dl->AddText({ pos.x + H + style.ItemInnerSpacing.x, pos.y + style.FramePadding.y },
                        ImGui::GetColorU32(ImGuiCol_Text), label, lend);

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
        return pressed;
    }

    void SpaceSeparator() {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    void Tooltip(const char* text, bool hovered)
    {
        struct TooltipState { float t, delay; ImVec2 frozen_pos; };
        static std::unordered_map<std::string, TooltipState> s_states;

        ImGuiContext& g = *GImGui;
        const ImGuiIO& io = g.IO;
        const float alpha = g.Style.Alpha;

        constexpr float SPD_IN = 22.f;
        constexpr float SPD_OUT = 14.f;
        constexpr float DELAY_SEC = 0.45f;

        auto& st = s_states.emplace(text, TooltipState{ 0.f, 0.f, {0.f, 0.f} }).first->second;

        if (hovered)
        {
            st.delay += io.DeltaTime;
            if (st.delay < DELAY_SEC)
            {
                if (st.t > 0.f) {
                    st.t -= st.t * ImMin(1.f, io.DeltaTime * SPD_OUT);
                    if (st.t < 0.003f) st.t = 0.f;
                }
                ImGui::GetForegroundDrawList();
                return;
            }
            st.t += (1.f - st.t) * ImMin(1.f, io.DeltaTime * SPD_IN);
            if (1.f - st.t < 0.003f) st.t = 1.f;
        }
        else
        {
            st.delay = 0.f;
            st.t -= st.t * ImMin(1.f, io.DeltaTime * SPD_OUT);
            if (st.t < 0.003f) st.t = 0.f;
        }

        if (st.t > 0.f && st.t < 1.f) ImGui::GetForegroundDrawList();
        if (st.t <= 0.001f) return;

        const ImGuiStyle& style = g.Style;

        const ImVec4 col_bg = colorTable[Color::Tooltip_Background];
        const ImVec4 col_border = colorTable[Color::Tooltip_Border];
        const ImVec4 col_text = colorTable[Color::Tooltip_Foreground];

        const char* lend = ImGui::FindRenderedTextEnd(text);
        const ImVec2 text_sz = ImGui::CalcTextSize(text, lend, false);
        const ImVec2 pad = style.WindowPadding;
        const ImVec2 box_sz = { text_sz.x + pad.x * 2.f, text_sz.y + pad.y * 2.f };
        const ImVec2 disp = io.DisplaySize;
        const ImVec2 off = { 16.f, 12.f };

        if (hovered)
        {
            constexpr float M = 6.f;
            ImVec2 p = { io.MousePos.x + off.x, io.MousePos.y + off.y };
            if (p.x + box_sz.x > disp.x - M) p.x = io.MousePos.x - box_sz.x - off.x;
            if (p.y + box_sz.y > disp.y - M) p.y = io.MousePos.y - box_sz.y - off.y;
            if (p.x < M) p.x = M;
            if (p.y < M) p.y = M;
            st.frozen_pos = p;
        }

        const float a = st.t * alpha;
        const ImRect bb = { st.frozen_pos, st.frozen_pos + box_sz };
        const ImVec2 slide_off = { 0.f, (1.f - st.t) * 6.f };
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        const float rnd = style.WindowRounding;

        dl->AddRectFilled(bb.Min + ImVec2(2.f, 3.f) + slide_off,
            bb.Max + ImVec2(2.f, 3.f) + slide_off,
            ImGui::ColorConvertFloat4ToU32({ 0.f, 0.f, 0.f, 0.18f * a }), rnd + 1.f
        );

        dl->AddRectFilled(bb.Min + slide_off, bb.Max + slide_off, ToU32A(col_bg, 1.f, a), rnd);
        dl->AddRect(bb.Min + slide_off, bb.Max + slide_off, ToU32A(col_border, 1.f, a), rnd, 0, 1.f);
        dl->AddText(bb.Min + pad + slide_off, ToU32A(col_text, 1.f, a), text, lend);
    }

    bool DragFloat(const char* label, float* v, float speed, float v_min, float v_max, const char* fmt)
    {
        struct DragState { float th, tHeld; };
        static std::unordered_map<ImGuiID, DragState> s_states;

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiIO& io = g.IO;

        const ImGuiID id = window->GetID(label);
        DragState& st = s_states.emplace(id, DragState{ 0.f, 0.f }).first->second;

        const ImVec2 pos = window->DC.CursorPos;
        const float  drag_w = ImGui::CalcItemWidth();
        const ImRect drag_bb = { pos, { pos.x + drag_w, pos.y + ImGui::GetFrameHeight() } };

        bool wheel_changed = false;
        if (ImGui::IsMouseHoveringRect(drag_bb.Min, drag_bb.Max) && io.MouseWheel != 0.f)
        {
            *v += io.MouseWheel > 0.f ? speed : -speed;
            if (v_min != v_max) *v = ImClamp(*v, v_min, v_max);
            wheel_changed = true;
            g.IO.MouseWheel = 0.f;
        }

        auto Lerp = [&](float& val, float tgt, float spd) {
            val += (tgt - val) * ImMin(1.f, io.DeltaTime * spd);
            if (ImAbs(val - tgt) < 0.004f) val = tgt;
        };
        Lerp(st.th, st.th, 9.f);
        Lerp(st.tHeld, st.tHeld, 18.f);

        const ImVec4 col_bg_idle = colorTable[Color::Drag_Background];
        const ImVec4 col_bg_hov = colorTable[Color::Drag_Background_Hover];
        const ImVec4 col_bg_held = colorTable[Color::Drag_Background_Active];
        const ImVec4 col_fill_idle = colorTable[Color::Drag_Middleground];
        const ImVec4 col_fill_hov = colorTable[Color::Drag_Middleground_Hover];
        const ImVec4 col_fill_held = colorTable[Color::Drag_Middleground_Active];
        const ImVec4 col_txt_idle = colorTable[Color::Drag_Foreground];
        const ImVec4 col_txt_hov = colorTable[Color::Drag_Foreground_Hover];
        const ImVec4 col_txt_held = colorTable[Color::Drag_Foreground_Active];

        const ImVec4 bg_now = LerpC(LerpC(col_bg_idle, col_bg_hov, st.th), col_bg_held, st.tHeld);
        const ImVec4 fill_now = LerpC(LerpC(col_fill_idle, col_fill_hov, st.th), col_fill_held, st.tHeld);
        const ImVec4 txt_now = LerpC(LerpC(col_txt_idle, col_txt_hov, st.th), col_txt_held, st.tHeld);

        ImGui::PushStyleColor(ImGuiCol_FrameBg, bg_now);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, bg_now);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, bg_now);
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, fill_now);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, fill_now);
        ImGui::PushStyleColor(ImGuiCol_Text, txt_now);

        ImGui::PushItemWidth(drag_w);
        const bool native_changed = ImGui::DragFloat(
            fmt::format("##DragWidget_{}", label).c_str(),
            v, speed, v_min, v_max,
            fmt
        );
        ImGui::PopItemWidth();
        ImGui::PopStyleColor(6);

        const char* label_display = label;
        if (label_display[0] == '#' && label_display[1] == '#')
            label_display = "";

        if (label_display[0] != '\0')
        {
            const ImVec2 label_pos = {
                drag_bb.Max.x + style.ItemInnerSpacing.x,
                drag_bb.Min.y + style.FramePadding.y
            };
            ImGui::RenderText(label_pos, label_display);
        }

        const bool is_hov = ImGui::IsItemHovered();
        const bool is_active = ImGui::IsItemActive();
        const bool is_typing = ImGui::TempInputIsActive(id);
        const bool held = is_active && !is_typing;

        Lerp(st.th, (is_hov || is_active) ? 1.f : 0.f, 9.f);
        Lerp(st.tHeld, held ? 1.f : 0.f, 18.f);

        if (st.th != (is_hov || is_active ? 1.f : 0.f) ||
            st.tHeld != (held ? 1.f : 0.f))
            ImGui::MarkItemEdited(id);

        return native_changed || wheel_changed;
    }

    bool DragInt(const char* label, int* v, float speed, int v_min, int v_max, const char* fmt)
    {
        struct DragState { float th, tHeld; };
        static std::unordered_map<ImGuiID, DragState> s_states;

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;
        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiIO& io = g.IO;
        const ImGuiID id = window->GetID(label);
        DragState& st = s_states.emplace(id, DragState{ 0.f, 0.f }).first->second;

        const ImVec2 pos = window->DC.CursorPos;
        const float  drag_w = ImGui::CalcItemWidth();
        const ImRect drag_bb = { pos, { pos.x + drag_w, pos.y + ImGui::GetFrameHeight() } };

        bool wheel_changed = false;
        if (ImGui::IsMouseHoveringRect(drag_bb.Min, drag_bb.Max) && io.MouseWheel != 0.f)
        {
            *v += io.MouseWheel > 0.f ? static_cast<int>(speed) : -static_cast<int>(speed);
            if (v_min != v_max) *v = ImClamp(*v, v_min, v_max);
            wheel_changed = true;
            g.IO.MouseWheel = 0.f;
        }

        auto Lerp = [&](float& val, float tgt, float spd) {
            val += (tgt - val) * ImMin(1.f, io.DeltaTime * spd);
            if (ImAbs(val - tgt) < 0.004f) val = tgt;
        };

        const ImVec4 col_bg_idle = colorTable[Color::Drag_Background];
        const ImVec4 col_bg_hov = colorTable[Color::Drag_Background_Hover];
        const ImVec4 col_bg_held = colorTable[Color::Drag_Background_Active];
        const ImVec4 col_fill_idle = colorTable[Color::Drag_Middleground];
        const ImVec4 col_fill_hov = colorTable[Color::Drag_Middleground_Hover];
        const ImVec4 col_fill_held = colorTable[Color::Drag_Middleground_Active];
        const ImVec4 col_txt_idle = colorTable[Color::Drag_Foreground];
        const ImVec4 col_txt_hov  = colorTable[Color::Drag_Foreground_Hover];
        const ImVec4 col_txt_held = colorTable[Color::Drag_Foreground_Active];

        const ImVec4 bg_now = LerpC(LerpC(col_bg_idle,   col_bg_hov,   st.th), col_bg_held,   st.tHeld);
        const ImVec4 fill_now = LerpC(LerpC(col_fill_idle, col_fill_hov, st.th), col_fill_held, st.tHeld);
        const ImVec4 txt_now = LerpC(LerpC(col_txt_idle,  col_txt_hov,  st.th), col_txt_held,  st.tHeld);

        ImGui::PushStyleColor(ImGuiCol_FrameBg,bg_now);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, bg_now);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, bg_now);
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, fill_now);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, fill_now);
        ImGui::PushStyleColor(ImGuiCol_Text, txt_now);

        ImGui::PushItemWidth(drag_w);
        const bool native_changed = ImGui::DragInt(
            fmt::format("##DragWidget_{}", label).c_str(),
            v, speed, v_min, v_max,
            fmt ? fmt : "%d"
        );
        ImGui::PopItemWidth();
        ImGui::PopStyleColor(6);

        const char* label_display = label;
        if (label_display[0] == '#' && label_display[1] == '#')
            label_display = "";
        if (label_display[0] != '\0')
        {
            const ImVec2 label_pos = {
                drag_bb.Max.x + style.ItemInnerSpacing.x,
                drag_bb.Min.y + style.FramePadding.y
            };
            ImGui::RenderText(label_pos, label_display);
        }

        const bool is_hov  = ImGui::IsItemHovered();
        const bool is_active = ImGui::IsItemActive();
        const bool is_typing = ImGui::TempInputIsActive(id);
        const bool held = is_active && !is_typing;

        Lerp(st.th, (is_hov || is_active) ? 1.f : 0.f, 9.f);
        Lerp(st.tHeld, held ? 1.f : 0.f, 18.f);

        if (st.th != (is_hov || is_active ? 1.f : 0.f) ||
            st.tHeld != (held ? 1.f : 0.f))
            ImGui::MarkItemEdited(id);

        return native_changed || wheel_changed;
    }

    bool ArrowButton(const char* str_id, ImGuiDir dir)
    {
        struct ArrowState { float th, tHeld; };
        static std::unordered_map<ImGuiID, ArrowState> s_states;

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiIO& io = g.IO;
        const float alpha = style.Alpha;

        const ImGuiID id = window->GetID(str_id);
        ArrowState& st = s_states.emplace(id, ArrowState{0.f, 0.f}).first->second;

        auto Lerp = [&](float& val, float tgt, float spd) {
            val += (tgt - val) * ImMin(1.f, io.DeltaTime * spd);
            if (ImAbs(val - tgt) < 0.004f) val = tgt; else ImGui::MarkItemEdited(id);
        };

        const ImVec4 col_bg_idle = colorTable[Color::Button_Background];
        const ImVec4 col_bg_hov = colorTable[Color::Button_Background_Hover];
        const ImVec4 col_bg_held = colorTable[Color::Button_Background_Active];

        const ImVec4 col_arr_idle = colorTable[Color::Button_Foreground];
        const ImVec4 col_arr_hov = colorTable[Color::Button_Foreground_Hover];
        const ImVec4 col_arr_held = colorTable[Color::Button_Foreground_Active];

        const ImVec4 bg_now = LerpC(LerpC(col_bg_idle, col_bg_hov, st.th), col_bg_held,  st.tHeld);
        const ImVec4 arr_now = LerpC(LerpC(col_arr_idle, col_arr_hov, st.th), col_arr_held, st.tHeld);

        ImGui::PushStyleColor(ImGuiCol_Button, bg_now);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bg_now);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, bg_now);
        ImGui::PushStyleColor(ImGuiCol_Text, arr_now);

        const bool pressed = ImGui::ArrowButton(str_id, dir);

        ImGui::PopStyleColor(4);

        const bool is_hov = ImGui::IsItemHovered();
        const bool is_active = ImGui::IsItemActive();

        Lerp(st.th, (is_hov || is_active) ? 1.f : 0.f,  9.f);
        Lerp(st.tHeld, is_active ? 1.f : 0.f, 18.f);

        return pressed;
    }

    void AddRadialGradient(ImDrawList* draw_list, const ImVec2& center, float radius, ImU32 col_in, ImU32 col_out)
    {
        if (((col_in | col_out) & IM_COL32_A_MASK) == 0 || radius < 0.5f)
            return;

        draw_list->_PathArcToFastEx(center, radius, 0, IM_DRAWLIST_ARCFAST_SAMPLE_MAX, 0);
        const int count = draw_list->_Path.Size - 1;

        unsigned int vtx_base = draw_list->_VtxCurrentIdx;
        draw_list->PrimReserve(count * 3, count + 1);

        const ImVec2 uv = draw_list->_Data->TexUvWhitePixel;
        draw_list->PrimWriteVtx(center, uv, col_in);
        for (int n = 0; n < count; n++)
            draw_list->PrimWriteVtx(draw_list->_Path[n], uv, col_out);

        for (int n = 0; n < count; n++)
        {
            draw_list->PrimWriteIdx((ImDrawIdx)(vtx_base));
            draw_list->PrimWriteIdx((ImDrawIdx)(vtx_base + 1 + n));
            draw_list->PrimWriteIdx((ImDrawIdx)(vtx_base + 1 + ((n + 1) % count)));
        }
        draw_list->_Path.Size = 0;
    }

    void GlowWindow(ImU32 col_in, ImU32 col_out, float glow_spread)
    {
        if (isLegacyRender()) return;
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems || ((col_in | col_out) & IM_COL32_A_MASK) == 0)
            return;

        ImVec2 pos  = window->Pos;
        ImVec2 size = window->Size;
        float corner_radius = window->WindowRounding;

        if (glow_spread <= 0.0f)
            glow_spread = ImMax(size.x, size.y) * 0.3f;

        if (glow_spread < 0.5f)
            return;

        ImDrawList* dl = ImGui::GetForegroundDrawList();

        const int num_segments = 12;
        const int total_pairs = (num_segments + 1) * 4;
        const int total_vertices = total_pairs * 2;
        const int total_indices = total_pairs * 6;

        dl->PrimReserve(total_indices, total_vertices);

        ImRect rect_inner(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        unsigned int vtx_base = dl->_VtxCurrentIdx;
        const ImVec2 uv = dl->_Data->TexUvWhitePixel;

        float rad = ImMin(corner_radius, ImMin(size.x, size.y) * 0.5f);

        ImVec2 centers[4] = {
            ImVec2(rect_inner.Min.x + rad, rect_inner.Min.y + rad),
            ImVec2(rect_inner.Max.x - rad, rect_inner.Min.y + rad),
            ImVec2(rect_inner.Max.x - rad, rect_inner.Max.y - rad),
            ImVec2(rect_inner.Min.x + rad, rect_inner.Max.y - rad)
        };

        float start_angles[4] = { 3.14159265f, 3.14159265f * 1.5f, 0.0f, 3.14159265f * 0.5f };

        for (int type = 0; type < 4; type++)
        {
            for (int i = 0; i <= num_segments; i++)
            {
                float angle = start_angles[type] + (i * (3.14159265f / 2.0f) / num_segments);
                ImVec2 dir(cosf(angle), sinf(angle));

                dl->PrimWriteVtx(centers[type] + dir * rad, uv, col_in);
                dl->PrimWriteVtx(centers[type] + dir * (rad + glow_spread), uv, col_out);
            }
        }

        for (int i = 0; i < total_pairs; i++)
        {
            int i0 = i * 2;
            int i1 = i * 2 + 1;
            int i2 = ((i + 1) % total_pairs) * 2;
            int i3 = ((i + 1) % total_pairs) * 2 + 1;

            dl->PrimWriteIdx((ImDrawIdx)(vtx_base + i0));
            dl->PrimWriteIdx((ImDrawIdx)(vtx_base + i1));
            dl->PrimWriteIdx((ImDrawIdx)(vtx_base + i3));

            dl->PrimWriteIdx((ImDrawIdx)(vtx_base + i0));
            dl->PrimWriteIdx((ImDrawIdx)(vtx_base + i3));
            dl->PrimWriteIdx((ImDrawIdx)(vtx_base + i2));
        }
    }

    struct SS { float y=0, t=0, h=0, wh=0, a=0; };
    static std::unordered_map<ImGuiID, SS> m;
    static std::vector<ImGuiID> st;

    bool BeginSmoothScroll(const char* name, bool* p_open, ImGuiWindowFlags f)
    {
        bool v = ImGui::Begin(name, p_open, f | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGuiID id = ImGui::GetCurrentWindow()->ID;
        st.push_back(id);
        auto& s = m[id];
        if (!v) return false;

        ImGuiIO& io = ImGui::GetIO();
        float dt = io.DeltaTime;
        bool hov = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);

        if (hov && io.MouseWheel != 0.f) {
            s.t -= io.MouseWheel * 40.f;
            io.MouseWheel = 0.f;
        }

        float maxY = ImMax(0.f, s.h - s.wh);
        s.t = ImClamp(s.t, 0.f, maxY);

        float k = ImMin(1.f, dt * 8.f);
        s.y += (s.t - s.y) * k;

        if (ImAbs(s.y - s.t) < 0.2f) s.y = s.t;
        s.y = ImClamp(s.y, 0.f, maxY);

        ImGui::SetScrollY(s.y);

        float tgtA = (maxY > 0 && (hov || ImAbs(s.y - s.t) > 0.5f)) ? 1.f : 0.f;
        s.a += (tgtA - s.a) * ImMin(1.f, dt * 10.f);
        if (s.a < 0.01f) s.a = 0.f;

        return true;
    }

    void EndSmoothScroll()
    {
        if (st.empty()) { ImGui::End(); return; }

        ImGuiID id = st.back();
        st.pop_back();
        auto& s = m[id];

        ImGuiWindow* w = ImGui::GetCurrentWindow();
        s.wh = w->InnerRect.GetHeight();
        s.h = s.wh + w->ScrollMax.y;

        if (s.a > 0.f)
        {
            ImGuiStyle& style = ImGui::GetStyle();

            auto& layout = NXR::Layout::Manager::get();
            const float scrollbar_size = layout.multipleScale(8.f);
            const float rounding = style.ScrollbarRounding;

            const float padding = style.WindowPadding.x * 0.35f;
            const float width = scrollbar_size * 0.55f;

            float maxY = ImMax(1.f, s.h - s.wh);
            float frac = ImClamp(s.y / maxY, 0.f, 1.f);

            float barH = ImMax(
                scrollbar_size,
                (s.wh / ImMax(s.h, s.wh)) * (s.wh - padding * 2.f)
            );

            float track = s.wh - barH - padding * 2.f;

            float x = w->InnerRect.Max.x - width - padding;
            float y = w->InnerRect.Min.y + padding + frac * track;

            ImVec2 a = { x, y };
            ImVec2 b = { x + width, y + barH };

            ImVec4 scrollbar = ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarGrab);
            scrollbar.w *= ImGui::GetStyle().Alpha * s.a;

            ImGui::GetWindowDrawList()->AddRectFilled(a, b, ImGui::GetColorU32(scrollbar), rounding);
        }
        ImGui::End();
    }

    static std::vector<PopupMessage> s_popups;

    void AddPopup(const std::string& caption, float time)
    {
        #ifdef GEODE_IS_DESKTOP
        s_popups.insert(s_popups.begin(), {
            caption,
            static_cast<float>(ImGui::GetTime()) + time,
            0.0f,
            0.0f,
            false
        });
        #endif

        #ifdef GEODE_IS_MOBILE
        NXR::MaterialLayer(FLAlertLayer::create("Info", caption.c_str(), "OK"))->show();
        #endif
    }

    void RenderPopups()
    {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        const ImVec2 dis = ImGui::GetIO().DisplaySize;
        const float dt = ImGui::GetIO().DeltaTime;
        const float now = static_cast<float>(ImGui::GetTime());

        auto& layout = NXR::Layout::Manager::get();

        for (auto& m : s_popups)
        {
            if (!m.dying && now >= m.expiry_time) m.dying = true;
            if (!m.dying) {
                m.slide_t  += (1.0f - m.slide_t)  * std::min(1.0f, dt * 10.0f);
                m.height_t += (1.0f - m.height_t) * std::min(1.0f, dt * 10.0f);
            } else {
                m.slide_t  += (0.0f - m.slide_t)  * std::min(1.0f, dt * 10.0f);
                m.height_t += (0.0f - m.height_t) * std::min(1.0f, dt * 10.0f);
            }
        }

        s_popups.erase(
            std::remove_if(s_popups.begin(), s_popups.end(),
                [](const PopupMessage& m) { return m.dying && m.slide_t <= 0.01f && m.height_t <= 0.01f; }),
            s_popups.end()
        );

        float curY = dis.y - layout.multipleScale(18.0f);

        const ImVec4 col_bg = colorTable[Color::Tooltip_Background];
        const ImVec4 col_border = colorTable[Color::Tooltip_Border];
        const ImVec4 col_text = colorTable[Color::Tooltip_Foreground];

        const float rnd = layout.multipleScale(GImGui->Style.WindowRounding);
        const ImVec2 pad = { layout.multipleScale(GImGui->Style.WindowPadding.x), layout.multipleScale(GImGui->Style.WindowPadding.y) };

        for (const auto& m : s_popups)
        {
            float sCardW = layout.multipleScale(272.0f);

            float textMaxW = sCardW - pad.x * 2.0f;
            ImVec2 textSz = ImGui::CalcTextSize(m.caption.c_str(), nullptr, false, textMaxW);

            float totalCardH = pad.y * 2.0f + textSz.y;
            float slotH = totalCardH * m.height_t;

            float xOff = (sCardW + layout.multipleScale(18.0f)) * (1.0f - m.slide_t);
            float x = dis.x - sCardW - layout.multipleScale(18.0f) + xOff;

            float y = curY - slotH;

            ImVec2 pMin = { x, y };
            ImVec2 pMax = { x + sCardW, y + slotH };

            ImU32 u_bg = ImGui::ColorConvertFloat4ToU32({ col_bg.x, col_bg.y, col_bg.z, col_bg.w });
            ImU32 u_border = ImGui::ColorConvertFloat4ToU32({ col_border.x, col_border.y, col_border.z, col_border.w });
            ImU32 u_text = ImGui::ColorConvertFloat4ToU32({ col_text.x, col_text.y, col_text.z, col_text.w });
            ImU32 u_shadow = ImGui::ColorConvertFloat4ToU32({ 0.0f, 0.0f, 0.0f, 0.18f });

            dl->AddRectFilled({ pMin.x + 2.0f, pMin.y + 3.0f }, { pMax.x + 2.0f, pMax.y + 3.0f }, u_shadow, rnd + 1.0f);
            dl->AddRectFilled(pMin, pMax, u_bg, rnd);
            dl->AddRect(pMin, pMax, u_border, rnd, 0, 1.0f);

            dl->PushClipRect(pMin, pMax, true);
            dl->AddText(ImGui::GetFont(), layout.multipleScale(ImGui::GetFontSize()), { pMin.x + pad.x, pMin.y + pad.y }, u_text, m.caption.c_str(), nullptr, textMaxW);
            dl->PopClipRect();

            curY -= slotH + layout.multipleScale(8.0f);
        }
    }
}
