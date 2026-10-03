#include "nxr_imgui_menu.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui-cocos.hpp>
#include <Geode/modify/UILayer.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <array>
#include <functional>
#include <string>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <unordered_set>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_theme.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_keybinds.hpp"
#include "../../core/nxr_ui_mode.hpp"
#include "../../core/nxr_utils.hpp"
#include "../../core/nxr_bot.hpp"
#include "../cocos/nxr_text_style.hpp"

using namespace geode::prelude;

namespace {
    constexpr float kReferenceHeight = 1080.f;
    constexpr float kFontPixels = 32.f;
    constexpr float kDefaultFontPixels = 13.f;
    constexpr float kWindowEm = 17.f;
    constexpr float kWindowGap = 8.f;
    constexpr float kDragStart = 10.f;
    constexpr const char* kOpenMenuBind = "nxr.menu::toggle";

    struct Picker {
        int serial = 0;
        std::string title;
        std::string action;
        bool allowClear = false;
        std::function<void(const std::string&)> onPick;
        std::vector<std::string> names;
        std::string selected;
        std::string infoFor;
        std::string info;
    };

    struct Runtime {
        bool open = false;
        std::vector<std::array<float, 4>> rects;
        std::vector<Picker> pickers;
        int pickerSerial = 0;
        bool resetLayout = false;
        bool fontLoaded = false;
        Ref<CCNode> hold;
        std::vector<NXR::Hack*> settings;
        std::unordered_set<std::string> infoOpen;
        ImGuiID dragId = 0;
        ImGuiID titleDown = 0;
        ImVec2 titlePos = ImVec2(0.f, 0.f);
        bool scrolled = false;
        bool sliderActive = false;
        float uiScale = 1.f;
        float appliedScale = -1.f;
        int appliedTheme = -1;
        ccColor3B appliedText = {0, 0, 0};
        std::string iniPath;
    };

    Runtime g;

    ImVec4 rgb(ccColor3B c, float a = 1.f) {
        return ImVec4(c.r / 255.f, c.g / 255.f, c.b / 255.f, a);
    }

    ImVec4 mix(ImVec4 a, ImVec4 b, float t) {
        return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
    }

    ImVec4 accentColor() {
        return rgb(NXR::Theme::accent());
    }

    void applyStyle(float scale) {
        ImGuiStyle& st = ImGui::GetStyle();
        st = ImGuiStyle();

        st.WindowPadding = ImVec2(12.f, 10.f);
        st.FramePadding = ImVec2(12.f, 8.f);
        st.ItemSpacing = ImVec2(10.f, 8.f);
        st.ItemInnerSpacing = ImVec2(8.f, 6.f);
        st.TouchExtraPadding = ImVec2(2.f, 2.f);
        st.IndentSpacing = 20.f;
        st.ScrollbarSize = 22.f;
        st.GrabMinSize = 24.f;
        st.WindowRounding = 8.f;
        st.ChildRounding = 6.f;
        st.FrameRounding = 6.f;
        st.PopupRounding = 8.f;
        st.ScrollbarRounding = 8.f;
        st.GrabRounding = 6.f;
        st.WindowBorderSize = 1.5f;
        st.FrameBorderSize = 0.f;
        st.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        st.ScaleAllSizes(scale);

        const ImVec4 accent = accentColor();
        const ImVec4 white(1.f, 1.f, 1.f, 1.f);
        const ImVec4 body(34.f / 255.f, 32.f / 255.f, 62.f / 255.f, 0.97f);
        const ImVec4 title(44.f / 255.f, 42.f / 255.f, 84.f / 255.f, 1.f);
        const ImVec4 button(72.f / 255.f, 68.f / 255.f, 142.f / 255.f, 1.f);
        const ImVec4 frame(24.f / 255.f, 23.f / 255.f, 44.f / 255.f, 1.f);
        const ImVec4 text = rgb(NXR::Ui::textColor({255, 255, 255}));

        auto* c = st.Colors;
        c[ImGuiCol_Text] = text;
        c[ImGuiCol_TextDisabled] = ImVec4(text.x, text.y, text.z, 0.5f);
        c[ImGuiCol_WindowBg] = body;
        c[ImGuiCol_ChildBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_PopupBg] = ImVec4(30.f / 255.f, 28.f / 255.f, 54.f / 255.f, 0.98f);
        c[ImGuiCol_Border] = ImVec4(accent.x, accent.y, accent.z, 0.9f);
        c[ImGuiCol_BorderShadow] = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_FrameBg] = frame;
        c[ImGuiCol_FrameBgHovered] = mix(frame, button, 0.5f);
        c[ImGuiCol_FrameBgActive] = button;
        c[ImGuiCol_TitleBg] = title;
        c[ImGuiCol_TitleBgActive] = mix(title, accent, 0.25f);
        c[ImGuiCol_TitleBgCollapsed] = title;
        c[ImGuiCol_MenuBarBg] = title;
        c[ImGuiCol_ScrollbarBg] = ImVec4(16.f / 255.f, 16.f / 255.f, 30.f / 255.f, 0.8f);
        c[ImGuiCol_ScrollbarGrab] = mix(button, white, 0.1f);
        c[ImGuiCol_ScrollbarGrabHovered] = mix(button, white, 0.25f);
        c[ImGuiCol_ScrollbarGrabActive] = mix(button, white, 0.4f);
        c[ImGuiCol_CheckMark] = white;
        c[ImGuiCol_SliderGrab] = mix(accent, white, 0.35f);
        c[ImGuiCol_SliderGrabActive] = mix(accent, white, 0.6f);
        c[ImGuiCol_Button] = button;
        c[ImGuiCol_ButtonHovered] = mix(button, white, 0.15f);
        c[ImGuiCol_ButtonActive] = mix(button, white, 0.3f);
        c[ImGuiCol_Header] = button;
        c[ImGuiCol_HeaderHovered] = mix(button, white, 0.15f);
        c[ImGuiCol_HeaderActive] = mix(button, white, 0.3f);
        c[ImGuiCol_Separator] = ImVec4(accent.x, accent.y, accent.z, 0.5f);
        c[ImGuiCol_ResizeGrip] = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_ResizeGripHovered] = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_ResizeGripActive] = ImVec4(0.f, 0.f, 0.f, 0.f);

        g.appliedScale = scale;
        g.appliedTheme = NXR::Theme::current();
        g.appliedText = NXR::Ui::textColor({255, 255, 255});
    }

    void applyFontScale(float scale) {
#if IMGUI_VERSION_NUM >= 19200
        ImGui::GetStyle().FontScaleMain = scale;
#else
        ImGui::GetIO().FontGlobalScale = scale;
#endif
    }

    void beginFrame() {
        ImGuiIO& io = ImGui::GetIO();
        const float base = std::max(0.5f, io.DisplaySize.y / kReferenceHeight);
        g.uiScale = base * NXR::Ui::tableScale();
        const float fontUnit = g.fontLoaded ? 1.f : kFontPixels / kDefaultFontPixels;

        const auto textColor = NXR::Ui::textColor({255, 255, 255});
        const bool stale = std::fabs(g.uiScale - g.appliedScale) > 0.001f
            || NXR::Theme::current() != g.appliedTheme
            || textColor.r != g.appliedText.r || textColor.g != g.appliedText.g || textColor.b != g.appliedText.b;
        if (stale) applyStyle(g.uiScale);

        applyFontScale(g.uiScale * NXR::Ui::fontScale() * fontUnit);
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.MouseDoubleClickTime = 0.f;

        if (ImGui::IsMouseClicked(0)) {
            g.scrolled = false;
            g.dragId = 0;
            g.titleDown = 0;
        }
    }

    void deferMain(std::function<void()> fn) {
        geode::queueInMainThread(std::move(fn));
    }

    template <class Fn, class... Args>
    void laterCall(Fn&& fn, Args... args) {
        auto holder = std::make_shared<std::decay_t<Fn>>(std::forward<Fn>(fn));
        geode::queueInMainThread([holder, args...] {
            if (*holder) (*holder)(args...);
        });
    }

    bool tapButton(const char* label, ImVec2 size, bool active) {
        const ImGuiStyle& st = ImGui::GetStyle();
        if (active) {
            const ImVec4 accent = mix(accentColor(), ImVec4(1.f, 1.f, 1.f, 1.f), 0.1f);
            ImGui::PushStyleColor(ImGuiCol_Button, accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, mix(accent, ImVec4(1.f, 1.f, 1.f, 1.f), 0.12f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, mix(accent, ImVec4(1.f, 1.f, 1.f, 1.f), 0.25f));
        }
        const bool pressed = ImGui::Button(label, size);
        if (active) ImGui::PopStyleColor(3);
        (void) st;
        return pressed && !g.scrolled;
    }

    bool toggleRow(const char* label, bool* value, float width, bool cheat, bool disabled) {
        const ImGuiStyle& st = ImGui::GetStyle();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float h = ImGui::GetFrameHeight();
        if (width <= 0.f) width = ImGui::GetContentRegionAvail().x;

        const ImVec2 p = ImGui::GetCursorScreenPos();
        const bool pressed = ImGui::InvisibleButton("##row", ImVec2(width, h));
        const bool hovered = ImGui::IsItemHovered();

        bool changed = false;
        if (pressed && !g.scrolled && !disabled) {
            *value = !*value;
            changed = true;
        }

        const float alpha = disabled ? 0.4f : 1.f;
        const ImVec4 accent = accentColor();
        const ImVec4 trackOff(60.f / 255.f, 58.f / 255.f, 92.f / 255.f, alpha);
        const ImVec4 trackOn(accent.x, accent.y, accent.z, alpha);
        const ImU32 white = ImGui::ColorConvertFloat4ToU32(ImVec4(1.f, 1.f, 1.f, alpha));

        if (hovered && !disabled) {
            draw->AddRectFilled(p, ImVec2(p.x + width, p.y + h), IM_COL32(255, 255, 255, 16), st.FrameRounding);
        }

        float boxW = h;
        if (NXR::Ui::toggleStyle() == NXR::Ui::Check) {
            const ImVec2 q(p.x + h, p.y + h);
            draw->AddRectFilled(p, q, ImGui::ColorConvertFloat4ToU32(*value ? trackOn : trackOff), st.FrameRounding);
            if (*value) {
                const float t = std::max(2.f, h * 0.1f);
                const ImVec2 a(p.x + h * 0.24f, p.y + h * 0.52f);
                const ImVec2 b(p.x + h * 0.43f, p.y + h * 0.72f);
                const ImVec2 c(p.x + h * 0.77f, p.y + h * 0.29f);
                draw->AddLine(a, b, white, t);
                draw->AddLine(b, c, white, t);
            }
        } else {
            boxW = h * 1.8f;
            const float r = h * 0.5f;
            draw->AddRectFilled(p, ImVec2(p.x + boxW, p.y + h), ImGui::ColorConvertFloat4ToU32(*value ? trackOn : trackOff), r);
            const float knobX = p.x + r + (*value ? boxW - h : 0.f);
            draw->AddCircleFilled(ImVec2(knobX, p.y + r), r - std::max(2.f, h * 0.1f), white);
        }

        const float textX = p.x + boxW + st.ItemInnerSpacing.x * 1.5f;
        ImVec4 textColor = cheat ? ImVec4(1.f, 120.f / 255.f, 140.f / 255.f, alpha) : st.Colors[ImGuiCol_Text];
        textColor.w *= alpha;
        draw->PushClipRect(ImVec2(textX, p.y), ImVec2(p.x + width, p.y + h), true);
        draw->AddText(ImVec2(textX, p.y + (h - ImGui::GetFontSize()) * 0.5f), ImGui::ColorConvertFloat4ToU32(textColor), label);
        draw->PopClipRect();

        return changed;
    }

    void touchScroll(bool hasTitle = true) {
        ImGuiIO& io = ImGui::GetIO();
        const ImGuiID id = ImGui::GetID("##touchscroll");
        const bool inBody = io.MousePos.y > ImGui::GetWindowPos().y + (hasTitle ? ImGui::GetFrameHeight() : 0.f);

        if (ImGui::IsMouseClicked(0) && inBody && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
            g.dragId = id;
        }

        if (g.dragId != id || !ImGui::IsMouseDown(0) || g.sliderActive) return;

        if (g.scrolled || ImGui::IsMouseDragging(0, kDragStart * g.uiScale)) {
            ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseDelta.y);
            g.scrolled = true;
        }
    }

    void titleTap(bool hasClose = false) {
        const ImGuiStyle& st = ImGui::GetStyle();
        const ImVec2 pos = ImGui::GetWindowPos();
        const float width = ImGui::GetWindowWidth();
        const float height = ImGui::GetFrameHeight();
        const float edge = st.FramePadding.x * 2.f + ImGui::GetFontSize();

        float left = 0.f;
        float right = hasClose ? edge : 0.f;
        if (st.WindowMenuButtonPosition == ImGuiDir_Left) left = edge;
        else if (st.WindowMenuButtonPosition == ImGuiDir_Right) right += edge;

        const ImVec2 a(pos.x + left, pos.y);
        const ImVec2 b(pos.x + width - right, pos.y + height);
        const ImGuiID id = ImGui::GetID("##titletap");
        const bool over = ImGui::IsMouseHoveringRect(a, b, false) && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

        if (ImGui::IsMouseClicked(0) && over) {
            g.titleDown = id;
            g.titlePos = pos;
        }

        if (g.titleDown == id && ImGui::IsMouseReleased(0)) {
            g.titleDown = 0;
            const ImVec2 drag = ImGui::GetMouseDragDelta(0, 0.f);
            const float limit = kDragStart * g.uiScale;
            const bool still = drag.x * drag.x + drag.y * drag.y < limit * limit;
            const bool unmoved = std::fabs(pos.x - g.titlePos.x) < 1.f && std::fabs(pos.y - g.titlePos.y) < 1.f;
            if (over && still && unmoved && !g.scrolled) {
                ImGui::SetWindowCollapsed(!ImGui::IsWindowCollapsed(), ImGuiCond_Always);
            }
        }
    }

    bool beginWindow(const char* title, int slot, bool* open = nullptr) {
        const ImGuiIO& io = ImGui::GetIO();
        const float width = ImGui::GetFontSize() * kWindowEm;
        const float titleH = ImGui::GetFrameHeight();
        const float gap = kWindowGap * g.uiScale;
        const int perColumn = std::max(1, static_cast<int>((io.DisplaySize.y - gap) / (titleH + gap)));
        const ImVec2 pos(gap + static_cast<float>(slot / perColumn) * (width + gap), gap + static_cast<float>(slot % perColumn) * (titleH + gap));
        const ImGuiCond cond = g.resetLayout ? ImGuiCond_Always : ImGuiCond_FirstUseEver;

        ImGui::SetNextWindowPos(pos, cond);
        ImGui::SetNextWindowCollapsed(true, cond);
        ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.f), ImVec2(width, io.DisplaySize.y * 0.88f));
        g.sliderActive = false;
        return ImGui::Begin(title, open, ImGuiWindowFlags_AlwaysAutoResize);
    }

    class ImguiForm final : public NXR::Form {
    public:
        void begin() { m_id = 0; }

        void addConfigToggle(const std::string& label, const std::string& key, bool defaultValue, geode::Function<void(bool)> callback) override {
            ImGui::PushID(m_id++);
            bool value = NXRConfig::get().get<bool>(key, defaultValue);
            if (toggleRow(label.c_str(), &value, 0.f, false, false)) {
                NXRConfig::get().set<bool>(key, value);
                if (callback) laterCall(std::move(callback), value);
            }
            ImGui::PopID();
        }

        void addBoundToggle(const std::string& label, bool current, geode::Function<void(bool)> setter) override {
            ImGui::PushID(m_id++);
            bool value = current;
            if (toggleRow(label.c_str(), &value, 0.f, false, false)) {
                if (setter) laterCall(std::move(setter), value);
            }
            ImGui::PopID();
        }

        void addConfigModeToggle(const std::string& key, const std::string& offText, const std::string& onText, int defaultValue, geode::Function<void(int)> callback) override {
            addConfigSelect("Mode", key, {{offText, 1}, {onText, 2}}, defaultValue, std::move(callback));
        }

        void addConfigRadio(const std::string& label, const std::string& key, const std::vector<std::pair<std::string, int>>& options, int defaultValue, geode::Function<void(int)> callback) override {
            addConfigSelect(label, key, options, defaultValue, std::move(callback));
        }

        void addConfigSelect(const std::string& label, const std::string& key, const std::vector<std::pair<std::string, int>>& options, int defaultValue, geode::Function<void(int)> callback) override {
            if (options.empty()) return;
            ImGui::PushID(m_id++);

            const int now = NXRConfig::get().get<int>(key, defaultValue);
            int current = 0;
            std::vector<std::string> names;
            for (size_t i = 0; i < options.size(); i++) {
                names.push_back(options[i].first);
                if (options[i].second == now) current = static_cast<int>(i);
            }

            ImGui::TextUnformatted(label.c_str());
            const int picked = NXR::Imgui::choice(names, current);
            if (picked >= 0 && picked != current) {
                const int value = options[picked].second;
                NXRConfig::get().set<int>(key, value);
                if (callback) laterCall(std::move(callback), value);
            }
            ImGui::PopID();
        }

        void addConfigIntInput(const std::string& label, const std::string& key, int min, int max, int defaultValue, geode::Function<void(int)> callback) override {
            ImGui::PushID(m_id++);
            int value = std::clamp(NXRConfig::get().get<int>(key, defaultValue), min, max);
            const int before = value;

            ImGui::TextUnformatted(label.c_str());

            const float btn = ImGui::GetFrameHeight();
            const float gap = ImGui::GetStyle().ItemSpacing.x;
            const bool minus = tapButton("-", ImVec2(btn, btn), false);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(std::max(btn, ImGui::GetContentRegionAvail().x - btn - gap));

            ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp;
            if (min >= 1 && max / std::max(1, min) >= 100) flags |= ImGuiSliderFlags_Logarithmic;
#ifdef GEODE_IS_MOBILE
            flags |= ImGuiSliderFlags_NoInput;
#endif
            const float speed = std::max(1.f, static_cast<float>(max - min) / 300.f);
            ImGui::DragInt("##value", &value, speed, min, max, "%d", flags);
            g.sliderActive = g.sliderActive || ImGui::IsItemActive();

            ImGui::SameLine();
            const bool plus = tapButton("+", ImVec2(btn, btn), false);

            if (minus) value = std::max(min, value - 1);
            if (plus) value = std::min(max, value + 1);

            if (value != before) {
                NXRConfig::get().set<int>(key, value);
                if (callback) laterCall(std::move(callback), value);
            }
            ImGui::PopID();
        }

        void addConfigFloatInput(const std::string& label, const std::string& key, float min, float max, float defaultValue, geode::Function<void(float)> callback) override {
            ImGui::PushID(m_id++);
            float value = std::clamp(NXRConfig::get().get<float>(key, defaultValue), min, max);
            const float before = value;
            const float range = max - min;
            const bool logarithmic = min > 0.f && max / min >= 100.f;

            ImGui::TextUnformatted(label.c_str());

            const float btn = ImGui::GetFrameHeight();
            const float gap = ImGui::GetStyle().ItemSpacing.x;
            const bool minus = tapButton("-", ImVec2(btn, btn), false);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(std::max(btn, ImGui::GetContentRegionAvail().x - btn - gap));

            ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp;
            if (logarithmic) flags |= ImGuiSliderFlags_Logarithmic;
#ifdef GEODE_IS_MOBILE
            flags |= ImGuiSliderFlags_NoInput;
#endif
            const float step = range > 100.f ? 1.f : (range > 10.f ? 0.1f : 0.01f);
            const float speed = std::max(step * 0.5f, range / 300.f);
            ImGui::DragFloat("##value", &value, speed, min, max, range >= 100.f ? "%.1f" : "%.2f", flags);
            g.sliderActive = g.sliderActive || ImGui::IsItemActive();

            ImGui::SameLine();
            const bool plus = tapButton("+", ImVec2(btn, btn), false);

            if (minus) value = std::max(min, logarithmic ? value / 1.1f : value - step);
            if (plus) value = std::min(max, logarithmic ? value * 1.1f : value + step);

            if (std::fabs(value - before) > 1e-6f) {
                NXRConfig::get().set<float>(key, value);
                if (callback) laterCall(std::move(callback), value);
            }
            ImGui::PopID();
        }

        void addConfigColor3Hex(const std::string& label, const std::string& key, const std::string& defaultHex) override {
            ImGui::PushID(m_id++);
            const std::string hex = NXRConfig::get().get<std::string>(key, defaultHex);
            const ccColor3B color = NXR::Utils::hexToColor(hex);
            float value[3] = {color.r / 255.f, color.g / 255.f, color.b / 255.f};

            if (ImGui::ColorEdit3("##color", value, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
                NXRConfig::get().set<std::string>(key, fmt::format("{:02X}{:02X}{:02X}",
                    static_cast<int>(std::round(value[0] * 255.f)),
                    static_cast<int>(std::round(value[1] * 255.f)),
                    static_cast<int>(std::round(value[2] * 255.f))));
            }
            g.sliderActive = g.sliderActive || ImGui::IsItemActive();

            ImGui::SameLine();
            if (tapButton("Reset", ImVec2(0.f, 0.f), false)) NXRConfig::get().set<std::string>(key, defaultHex);
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label.c_str());
            ImGui::PopID();
        }

        void addConfigColor4Hex(const std::string& label, const std::string& key, const std::string& defaultHex) override {
            ImGui::PushID(m_id++);
            const std::string hex = NXRConfig::get().get<std::string>(key, defaultHex);
            const ccColor4F color = NXR::Utils::hexToColor4F(hex);
            float value[4] = {color.r, color.g, color.b, color.a};

            if (ImGui::ColorEdit4("##color", value, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar)) {
                NXRConfig::get().set<std::string>(key, fmt::format("{:02X}{:02X}{:02X}{:02X}",
                    static_cast<int>(std::round(value[0] * 255.f)),
                    static_cast<int>(std::round(value[1] * 255.f)),
                    static_cast<int>(std::round(value[2] * 255.f)),
                    static_cast<int>(std::round(value[3] * 255.f))));
            }
            g.sliderActive = g.sliderActive || ImGui::IsItemActive();

            ImGui::SameLine();
            if (tapButton("Reset", ImVec2(0.f, 0.f), false)) NXRConfig::get().set<std::string>(key, defaultHex);
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label.c_str());
            ImGui::PopID();
        }

        void addSeparator(float) override {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }

        void requestRebuild() override {}

    private:
        int m_id = 0;
    };

    ImguiForm g_form;

    void drawHackRow(NXR::Hack& hack) {
        ImGui::PushID(hack.getID().c_str());

        const ImGuiStyle& st = ImGui::GetStyle();
        const float h = ImGui::GetFrameHeight();
        const bool hasSettings = hack.hasForm();
        const bool hasInfo = !hack.getDesc().empty();

        float extra = 0.f;
        if (hasInfo) extra += h + st.ItemSpacing.x;
        if (hasSettings) extra += h + st.ItemSpacing.x;

        bool on = hack.getEnabled();
        const float width = ImGui::GetContentRegionAvail().x - extra;
        if (toggleRow(hack.getName().c_str(), &on, width, hack.isCheating(), hack.getDisabled())) {
            NXR::Hack* target = &hack;
            deferMain([target, on] { target->setEnabled(on); });
        }

        const std::string& id = hack.getID();
        const bool infoShown = g.infoOpen.contains(id);

        if (hasInfo) {
            ImGui::SameLine();
            if (tapButton("i", ImVec2(h, h), infoShown)) {
                if (infoShown) g.infoOpen.erase(id);
                else g.infoOpen.insert(id);
            }
        }

        if (hasSettings) {
            ImGui::SameLine();
            if (tapButton(">", ImVec2(h, h), false)) NXR::Imgui::openHackSettings(hack);
        }

        if (hasInfo && infoShown) {
            ImGui::PushTextWrapPos(0.f);
            ImGui::TextDisabled("%s", hack.getDesc().c_str());
            ImGui::PopTextWrapPos();
            ImGui::Spacing();
        }

        ImGui::PopID();
    }

    std::string bindText(const geode::Keybind& bind, bool recording) {
        if (recording) return "Press a key...";
        return bind.key == cocos2d::KEY_None ? std::string("None") : bind.toString();
    }

    void bindButtons(const std::string& text, bool recording, std::function<void()> onSet, std::function<void()> onClear) {
        const float gap = ImGui::GetStyle().ItemSpacing.x;
        const float clearW = ImGui::CalcTextSize("Clear").x + ImGui::GetStyle().FramePadding.x * 2.f;
        const float keyW = std::max(1.f, ImGui::GetContentRegionAvail().x - clearW - gap);

        if (tapButton(text.c_str(), ImVec2(keyW, 0.f), recording)) deferMain(std::move(onSet));
        ImGui::SameLine();
        if (tapButton("Clear", ImVec2(0.f, 0.f), false)) deferMain(std::move(onClear));
    }

    void drawKeybindRow() {
        auto& keybinds = NXR::Keybinds::get();
        const bool recording = keybinds.isRecordingCustom(kOpenMenuBind);
        ImGui::PushID("openkey");
        bindButtons(bindText(keybinds.getBind(kOpenMenuBind), recording), recording,
            [] { NXR::Keybinds::get().startRecordingCustom(kOpenMenuBind); },
            [] {
                NXR::Keybinds::get().stopRecording();
                NXR::Keybinds::get().clearCustomBind(kOpenMenuBind);
            });
        ImGui::PopID();
    }

    void drawKeybindBody() {
        auto& keybinds = NXR::Keybinds::get();

        ImGui::PushTextWrapPos(0.f);
        ImGui::TextDisabled("Tap a key button, then press a key. That key toggles the feature without opening the menu. Esc or Clear removes it");
        ImGui::PopTextWrapPos();
        ImGui::Spacing();

        for (auto& window : NXR::Gui::get().getWindows()) {
            if (window.getHacks().empty()) continue;
            const std::string name = window.getName();
            if (!ImGui::CollapsingHeader((name + "###kbwin:" + name).c_str())) continue;

            for (auto& hack : window.getHacks()) {
                const std::string hackName = hack.getName();
                const bool recording = keybinds.isRecording(name, hackName);
                ImGui::PushID(hack.getID().c_str());
                ImGui::TextUnformatted(hackName.c_str());
                bindButtons(bindText(hack.getKeybind(), recording), recording,
                    [name, hackName] { NXR::Keybinds::get().startRecording(name, hackName); },
                    [name, hackName] {
                        NXR::Keybinds::get().stopRecording();
                        NXR::Keybinds::get().clearHackBind(name, hackName);
                    });
                ImGui::PopID();
            }
        }

#ifdef GEODE_IS_DESKTOP
        const auto actions = keybinds.customActions();
        if (!actions.empty() && ImGui::CollapsingHeader("Actions###kbactions")) {
            for (const auto& [id, label] : actions) {
                const bool recording = keybinds.isRecordingCustom(id);
                ImGui::PushID(id.c_str());
                ImGui::TextUnformatted(label.c_str());
                bindButtons(bindText(keybinds.getBind(id), recording), recording,
                    [id] { NXR::Keybinds::get().startRecordingCustom(id); },
                    [id] {
                        NXR::Keybinds::get().stopRecording();
                        NXR::Keybinds::get().clearCustomBind(id);
                    });
                ImGui::PopID();
            }
        }
#endif
    }

    void drawSettingsBody() {
        auto& config = NXRConfig::get();
        NXR::Form& form = g_form;

        ImGui::TextUnformatted("Menu Layout");
        const int layout = NXR::Imgui::choice({"Panel", "Table"}, NXR::Ui::layout());
        if (layout >= 0 && layout != NXR::Ui::layout()) {
            config.set<int>(NXR::Ui::kLayoutKey, layout);
            deferMain([] { NXR::Ui::reopenMenu(); });
        }
        form.addSeparator();

        ImGui::TextUnformatted("Open Menu Key");
        drawKeybindRow();
        form.addSeparator();

        ImGui::TextUnformatted("Hack Settings Popup");
        const int popupMode = NXR::Imgui::choice({"Popup", "Clean"}, NXR::Ui::settingsPopup());
        if (popupMode >= 0 && popupMode != NXR::Ui::settingsPopup()) {
            config.set<int>(NXR::Ui::kSettingsPopupKey, popupMode);
            g.settings.clear();
        }
        ImGui::PushTextWrapPos(0.f);
        ImGui::TextDisabled("Popup: window with title bar, arrow and X. Clean: only the options, tap empty space to close");
        ImGui::PopTextWrapPos();
        form.addSeparator();

        ImGui::TextUnformatted("Theme");
        const int theme = NXR::Imgui::choice({"Basic", "Normal", "Medium", "Pro"}, NXR::Theme::current() - 1);
        if (theme >= 0) config.set<int>(NXR::Theme::kKey, theme + 1);
        form.addSeparator();

        ImGui::TextUnformatted("Toggle Style");
        const int style = NXR::Imgui::choice({"Switch", "Check"}, NXR::Ui::toggleStyle());
        if (style >= 0) config.set<int>(NXR::Ui::kToggleStyleKey, style);
        form.addSeparator();

        form.addConfigFloatInput("Table Scale (0.6 - 1.6)", NXR::Ui::kTableScaleKey, 0.6f, 1.6f, 1.f);
        form.addConfigFloatInput("Font Size (0.6 - 1.6)", NXR::Ui::kFontScaleKey, 0.6f, 1.6f, 1.f);
        form.addConfigToggle("Custom Font Color", NXR::Ui::kFontColorOnKey, true);
        form.addConfigColor3Hex("Font Color", NXR::Ui::kFontColorKey, "FFFFFF");
        form.addSeparator();

        if (NXR::Imgui::button("Reset Window Positions", -1.f)) g.resetLayout = true;
    }

    void collectUiRects() {
        g.rects.clear();
        if (!g.open) return;

        ImGuiContext* ctx = ImGui::GetCurrentContext();
        if (!ctx) return;

        const float pad = 4.f * g.uiScale;
        for (ImGuiWindow* window : ctx->Windows) {
            if (!window || !window->Active || window->Hidden) continue;
            if (window->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_Tooltip)) continue;
            g.rects.push_back({window->Pos.x - pad, window->Pos.y - pad, window->Pos.x + window->Size.x + pad, window->Pos.y + window->Size.y + pad});
        }
    }

    std::string replayInfoText(const std::string& name) {
        NXR::Bot::Macro macro;
        if (!NXR::Bot::loadMacro(macro, NXR::Bot::macroPathFor(name))) return "Failed to read this replay";
        return fmt::format("Actions: {}  |  Frames: {}  |  Super: {}  |  TPS: {:.0f}", macro.events.size(), macro.frames.size(), macro.supers.size(), macro.tps);
    }

    void drawPickerWindows() {
        const bool clean = NXR::Ui::settingsPopup() == NXR::Ui::Clean;

        for (size_t i = 0; i < g.pickers.size();) {
            Picker& picker = g.pickers[i];
            const std::string title = picker.title + "###picker:" + std::to_string(picker.serial);
            bool open = true;
            bool done = false;

            ImGuiIO& io = ImGui::GetIO();
            const float width = ImGui::GetFontSize() * kWindowEm;
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.f), ImVec2(width, io.DisplaySize.y * 0.88f));
            g.sliderActive = false;

            ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
            if (clean) flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove;

            if (ImGui::Begin(title.c_str(), clean ? nullptr : &open, flags)) {
                if (clean) {
                    ImGui::TextUnformatted(picker.title.c_str());
                    ImGui::Separator();
                }

                if (picker.allowClear) {
                    ImGui::PushTextWrapPos(0.f);
                    ImGui::TextDisabled("Tap the selected replay again to deselect");
                    ImGui::PopTextWrapPos();
                }

                if (picker.names.empty()) ImGui::TextDisabled("No replays found");

                const float h = ImGui::GetFrameHeight();
                const float gap = ImGui::GetStyle().ItemSpacing.x;
                for (const auto& name : picker.names) {
                    ImGui::PushID(name.c_str());
                    const bool selected = picker.selected == name;
                    const float nameWidth = std::max(1.f, ImGui::GetContentRegionAvail().x - h - gap);

                    if (tapButton(name.c_str(), ImVec2(nameWidth, 0.f), selected)) {
                        picker.selected = (picker.allowClear && selected) ? std::string() : name;
                    }

                    ImGui::SameLine();
                    const bool infoShown = picker.infoFor == name;
                    if (tapButton("i", ImVec2(h, h), infoShown)) {
                        if (infoShown) {
                            picker.infoFor.clear();
                            picker.info.clear();
                        } else {
                            picker.infoFor = name;
                            picker.info = replayInfoText(name);
                        }
                    }

                    if (picker.infoFor == name) {
                        ImGui::PushTextWrapPos(0.f);
                        ImGui::TextDisabled("%s", picker.info.c_str());
                        ImGui::PopTextWrapPos();
                    }
                    ImGui::PopID();
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
                if (tapButton(picker.action.c_str(), ImVec2(half, 0.f), true)) {
                    if (picker.selected.empty() && !picker.allowClear) {
                        deferMain([] { geode::Notification::create("Pick a replay first", geode::NotificationIcon::Warning)->show(); });
                    } else {
                        auto callback = picker.onPick;
                        auto chosen = picker.selected;
                        deferMain([callback, chosen] { if (callback) callback(chosen); });
                        done = true;
                    }
                }
                ImGui::SameLine();
                if (tapButton("Cancel", ImVec2(half, 0.f), false)) done = true;

                touchScroll(!clean);
            }
            ImGui::End();

            if (open && !done) {
                i++;
            } else {
                g.pickers.erase(g.pickers.begin() + static_cast<std::ptrdiff_t>(i));
            }
        }
    }

    void drawHackSettingsWindows() {
        const bool clean = NXR::Ui::settingsPopup() == NXR::Ui::Clean;

        for (size_t i = 0; i < g.settings.size();) {
            NXR::Hack* hack = g.settings[i];
            const std::string title = hack->getName() + " Settings###settings:" + hack->getID();
            bool open = true;

            ImGuiIO& io = ImGui::GetIO();
            const float width = ImGui::GetFontSize() * kWindowEm;
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.f), ImVec2(width, io.DisplaySize.y * 0.88f));
            g.sliderActive = false;

            ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
            if (clean) flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove;

            if (ImGui::Begin(title.c_str(), clean ? nullptr : &open, flags)) {
                g_form.begin();
                hack->callForm(g_form);
                touchScroll(!clean);
            }
            ImGui::End();

            if (open) {
                i++;
            } else {
                g.settings.erase(g.settings.begin() + static_cast<std::ptrdiff_t>(i));
            }
        }

    }

    void drawMenu() {
        if (!g.open) return;

        beginFrame();

        if (g.hold) {
            if (g.hold->getParent()) return;
            g.hold = nullptr;
        }

        int slot = 0;
        for (auto& window : NXR::Gui::get().getWindows()) {
            const std::string name = window.getName();
            if (name == "Settings") continue;

            if (window.hasImguiPanel()) {
                if (beginWindow((name + "###panel:" + name).c_str(), slot)) {
                    g_form.begin();
                    window.drawImguiPanel();
                    touchScroll();
                }
                titleTap();
                ImGui::End();
                slot++;
            }

            if (window.getHacks().empty()) continue;

            const std::string title = window.hasImguiPanel() ? name + " Hacks" : name;
            if (beginWindow((title + "###hacks:" + name).c_str(), slot)) {
                for (auto& hack : window.getHacks()) drawHackRow(hack);
                touchScroll();
            }
            titleTap();
            ImGui::End();
            slot++;
        }

        if (beginWindow("Keybind###keybind", slot)) {
            g_form.begin();
            drawKeybindBody();
            touchScroll();
        }
        titleTap();
        ImGui::End();
        slot++;

        if (beginWindow("Settings###settings", slot)) {
            g_form.begin();
            drawSettingsBody();
            touchScroll();
        }
        titleTap();
        ImGui::End();

        drawHackSettingsWindows();
        drawPickerWindows();

        if (NXR::Ui::settingsPopup() == NXR::Ui::Clean && (!g.settings.empty() || !g.pickers.empty())
            && ImGui::IsMouseClicked(0) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {
            g.settings.clear();
            g.pickers.clear();
        }

        g.resetLayout = false;
    }
}

bool NXR::Imgui::isOpen() {
    return g.open;
}

void NXR::Imgui::open() {
    g.open = true;
}

void NXR::Imgui::close() {
    if (!g.open) return;
    g.open = false;
    g.hold = nullptr;
    g.settings.clear();
    g.pickers.clear();
    g.rects.clear();
    NXRConfig::get().save(getFileDataPath());
}

void NXR::Imgui::holdFor(cocos2d::CCNode* node) {
    g.hold = node;
}

void NXR::Imgui::openHackSettings(NXR::Hack& hack) {
    if (!hack.hasForm()) return;
    if (!g.open) g.open = true;
    if (NXR::Ui::settingsPopup() == NXR::Ui::Clean) g.settings.clear();
    if (std::find(g.settings.begin(), g.settings.end(), &hack) == g.settings.end()) g.settings.push_back(&hack);
}

bool NXR::Imgui::touchOverUi(cocos2d::CCTouch* touch) {
    if (!touch || !g.open || g.rects.empty()) return false;

    const auto win = CCDirector::get()->getWinSize();
    if (win.width <= 0.f || win.height <= 0.f) return false;

    const auto loc = touch->getLocation();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float x = loc.x / win.width * display.x;
    const float y = (1.f - loc.y / win.height) * display.y;

    for (const auto& r : g.rects) {
        if (x >= r[0] && x <= r[2] && y >= r[1] && y <= r[3]) return true;
    }
    return false;
}

void NXR::Imgui::pickReplay(const std::string& title, const std::string& action, std::function<void(const std::string&)> onPick, bool allowClear) {
    if (!g.open) g.open = true;
    if (NXR::Ui::settingsPopup() == NXR::Ui::Clean) {
        g.settings.clear();
        g.pickers.clear();
    }

    Picker picker;
    picker.serial = ++g.pickerSerial;
    picker.title = title;
    picker.action = action;
    picker.allowClear = allowClear;
    picker.onPick = std::move(onPick);
    picker.names = NXR::Bot::listMacros();
    std::sort(picker.names.begin(), picker.names.end());

    if (allowClear) {
        const auto& current = NXR::Bot::State::get().selectedReplay;
        if (std::find(picker.names.begin(), picker.names.end(), current) != picker.names.end()) picker.selected = current;
    } else if (!picker.names.empty()) {
        picker.selected = picker.names.front();
    }

    g.pickers.push_back(std::move(picker));
}

NXR::Form& NXR::Imgui::form() {
    return g_form;
}

bool NXR::Imgui::button(const std::string& label, float width, bool active) {
    ImVec2 size(0.f, 0.f);
    if (width < 0.f) size.x = -1.f;
    else if (width > 0.f) size.x = width;
    ImGui::PushID(label.c_str());
    const bool pressed = tapButton(label.c_str(), size, active);
    ImGui::PopID();
    return pressed;
}

int NXR::Imgui::choice(const std::vector<std::string>& labels, int current, int columns) {
    const int count = static_cast<int>(labels.size());
    if (count == 0) return -1;

    const int cols = columns > 0 ? std::min(columns, count) : (count <= 3 ? count : 2);
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float width = (ImGui::GetContentRegionAvail().x - gap * static_cast<float>(cols - 1)) / static_cast<float>(cols);

    int picked = -1;
    for (int i = 0; i < count; i++) {
        if (i % cols != 0) ImGui::SameLine();
        ImGui::PushID(i);
        if (tapButton(labels[i].c_str(), ImVec2(width, 0.f), i == current)) picked = i;
        ImGui::PopID();
    }
    return picked;
}

void NXR::Imgui::later(std::function<void()> fn) {
    deferMain(std::move(fn));
}

$on_mod(Loaded) {
    ImGuiCocos::get().setup([] {
        ImGuiIO& io = ImGui::GetIO();

        g.iniPath = (geode::Mod::get()->getSaveDir() / "nxr_imgui.ini").string();
        io.IniFilename = g.iniPath.c_str();

        std::error_code ec;
        const auto font = geode::Mod::get()->getResourcesDir() / "GoogleSans-Regular.ttf";
        if (std::filesystem::exists(font, ec)) {
            const std::string path = font.string();
            g.fontLoaded = io.Fonts->AddFontFromFileTTF(path.c_str(), kFontPixels) != nullptr;
        }
        if (!g.fontLoaded) io.Fonts->AddFontDefault();
    }).draw([] {
        drawMenu();
        collectUiRects();
    });
}

$execute {
    NXR::Keybinds::get().registerAction(kOpenMenuBind, "Open Menu", geode::Keybind(cocos2d::KEY_Tab, geode::KeyboardModifier::None), [](bool repeat) {
        if (!repeat) NXR::Ui::toggleMenu();
    });
}

class $modify(NXRImguiUILayer, UILayer) {
    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
        if (NXR::Imgui::touchOverUi(touch)) return false;
        return UILayer::ccTouchBegan(touch, event);
    }
};

class $modify(NXRImguiEditorUI, EditorUI) {
    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
        if (NXR::Imgui::touchOverUi(touch)) return false;
        return EditorUI::ccTouchBegan(touch, event);
    }
};
