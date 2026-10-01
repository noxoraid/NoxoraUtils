#pragma once
#include <imgui-cocos.hpp>

namespace NXRWidget {
    enum Color {
        Button_Background,
        Button_Background_Hover,
        Button_Background_Active,
        Button_Foreground,
        Button_Foreground_Hover,
        Button_Foreground_Active,
        Button_Border,

        Checkbox_Background_Off,
        Checkbox_Background_Off_Hover,
        Checkbox_Background_Off_Active,
        Checkbox_Background_On,
        Checkbox_Background_On_Hover,
        Checkbox_Background_On_Active,
        Checkbox_Knob_Off,
        Checkbox_Knob_Off_Hover,
        Checkbox_Knob_Off_Active,
        Checkbox_Knob_On,
        Checkbox_Knob_On_Hover,
        Checkbox_Knob_On_Active,
        Checkbox_Knob_Shadow,
        Checkbox_Border,

        RadioButton_Ring_Off,
        RadioButton_Ring_Off_Hover,
        RadioButton_Ring_Off_Active,
        RadioButton_Ring_On,
        RadioButton_Ring_On_Hover,
        RadioButton_Ring_On_Active,
        RadioButton_Dot_Off,
        RadioButton_Dot_On,
        RadioButton_Dot_On_Hover,
        RadioButton_Dot_On_Active,
        RadioButton_Layer_Off_Hover,
        RadioButton_Layer_Off_Active,
        RadioButton_Layer_On_Hover,
        RadioButton_Layer_On_Active,

        Tooltip_Background,
        Tooltip_Foreground,
        Tooltip_Border,
        Drag_Background,
        Drag_Background_Hover,
        Drag_Background_Active,
        Drag_Middleground,
        Drag_Middleground_Hover,
        Drag_Middleground_Active,
        Drag_Foreground,
        Drag_Foreground_Hover,
        Drag_Foreground_Active,

        Glow_Popup_In,
        Glow_Popup_Out,

        Glow_Popup_Warning_In,
        Glow_Popup_Warning_Out,

        Glow_Popup_Critical_In,
        Glow_Popup_Critical_Out,

        Glow_Popup_Update_In,
        Glow_Popup_Update_Out,

        Color_Count
    };

    struct PopupMessage {
        std::string caption;
        float expiry_time;
        float slide_t;
        float height_t;
        bool dying;
    };

    extern std::map<Color, ImVec4> colorTable;

    bool Button(const char* label, const ImVec2& size_arg = ImVec2(0, 0));
    bool Checkbox(const char* label, bool* v);
    bool RadioButton(const char* label, bool active);

    void ApplyNativeGuiColors();
    void ApplyStyle(float scale);
    void DrawColorEditor();
    void ProccessOriginalTheme(bool save);

    void SaveTheme();
    void LoadTheme();

    void SpaceSeparator();
    void Tooltip(const char* text, bool hovered = true);

    bool DragFloat(const char* label, float* v, float speed = 1.f, float v_min = 0.f,
        float v_max = 0.f, const char* fmt = "%.2f");

    bool DragInt(const char* label, int* v, float speed = 1.f, int v_min = 0,
        int v_max = 0, const char* fmt = "%d");

    bool ArrowButton(const char* str_id, ImGuiDir dir);

    void AddRadialGradient(ImDrawList* draw_list, const ImVec2& center, float radius, ImU32 col_in, ImU32 col_out);
    void GlowWindow(ImU32 col_in, ImU32 col_out, float glow_spread = -1.0f);

    bool BeginSmoothScroll(const char* name, bool* p_open = nullptr, ImGuiWindowFlags f = 0);
    void EndSmoothScroll();

    void AddPopup(const std::string& caption, float time = 5.f);
    void RenderPopups();
}
