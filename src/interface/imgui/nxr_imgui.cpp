#ifdef GEODE_IS_DESKTOP
#include "imgui.h"
#include <Geode/Geode.hpp>
#include <imgui-cocos.hpp>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include "nxr_layout.hpp"

#include "nxr_widget.hpp"
#include "nxr_widget_helper.hpp"
#include "nxr_font.hpp"

#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/CCEGLView.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>

using namespace geode::prelude;

#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_utils.hpp"
#include "../../core/nxr_keybinds.hpp"

static bool g_show = false;

static bool g_showThemeEditor = false;
static bool g_reopenAfterTheme = false;
static bool g_showUpdatePopup = false;

static bool g_resetLayoutCalled = false;
static bool g_hardRecalculation = false;
static bool g_reopenAfterRecalc = false;

static bool g_isAnimating = false;
static bool g_isFadingIn = false;
static float g_animTime = 0.0f;

static bool g_inited = false;
static std::string g_search_text = "";

static std::vector<std::vector<std::string>> g_layout = {
    {"Player"},
    {"Utils"},
    {"Level"},
    {"Creator"},
    {"Bot"},
    {"Keybinds"},
    {"NXR Settings"}
};

static std::vector<NXR::Layout::WindowInfo> g_fixedWindowSizes = {
    {"Keybinds", 0.f, 90.f},
    {"NXR Settings", 225.f, 0.f},
};

void onOpen() {
    NXR::Utils::updateCursorState(g_show);

    auto& config = NXRConfig::get();
    if (config.get<bool>("nxr.ui.need_update", false) && config.get<bool>("nxr.ui.notify_updates", true)) {
        g_showUpdatePopup = true;
        config.set<bool>("nxr.ui.need_update", false);
    }
}

void onClose() {
    NXR::Utils::updateCursorState(g_show);
    NXRConfig::get().save(getFileDataPath());
    NXR::Keybinds::get().save();
    NXRWidget::SaveTheme();
    g_search_text = "";
}

void ToggleUI()
{
    if (g_isAnimating)
        return;

    g_isFadingIn = !g_show;
    g_isAnimating = true;
    g_animTime = 0.0f;

    if (g_isFadingIn) {
        g_show = true;
        onOpen();
    }
}

void animateAlpha()
{
    if (!g_isAnimating)
        return;

    ImGuiStyle& style = ImGui::GetStyle();
    float deltaTime = ImGui::GetIO().DeltaTime;

    float duration = NXRConfig::get().get<int>("nxr.gui::anim_durr", 100) / 1000.0f;
    g_animTime += deltaTime;

    float t = g_animTime / duration;
    if (t >= 1.0f)
    {
        style.Alpha = g_isFadingIn ? 1.0f : 0.0f;
        g_isAnimating = false;

        if (!g_isFadingIn)
        {
            g_show = false;
            onClose();

            if (g_reopenAfterRecalc || g_reopenAfterTheme) {
                if (g_reopenAfterRecalc) {
                    g_reopenAfterRecalc = false;
                    g_hardRecalculation = true;
                }
                if (g_reopenAfterTheme) {
                    g_reopenAfterTheme = false;
                    g_showThemeEditor = !g_showThemeEditor;
                }
                ToggleUI();
            }
        }

        return;
    }

    style.Alpha = g_isFadingIn ? t : 1.0f - t;
}

void PushAnimateFoundColor(const std::string& hackName) {
    static std::unordered_map<std::string, float> anim;

    std::string search_name = hackName;
    std::string search_item = g_search_text;

    std::transform(search_item.begin(), search_item.end(), search_item.begin(), ::tolower);
    std::transform(search_name.begin(), search_name.end(), search_name.begin(), ::tolower);

    bool founded = search_item.empty() ? true : (search_name.find(search_item) != std::string::npos);

    float& t = anim[hackName];

    float speed = 16.0f;
    float target = founded ? 1.0f : 0.20f;

    t = ImLerp(t, target, ImGui::GetIO().DeltaTime * speed);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * t);
}

void SettingsRender() {
    auto& layoutManager = NXR::Layout::Manager::get();
    auto& config = NXRConfig::get();

    std::string windowName = "NXR Settings";
    layoutManager.applyWindowTransform(windowName);

    if (g_showThemeEditor) {
        ImGui::SetNextWindowPos({layoutManager.multipleScale(10.f), layoutManager.multipleScale(10.f)});
    }

    ImGui::Begin(windowName.c_str());

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::InputTextWithHint("##Search", "Search:", &g_search_text);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

    int duration = config.get<int>("nxr.gui::anim_durr", 100);
    if (NXRWidget::DragInt("##gui_anim_durr", &duration, 50.f, 0, 500, "Duration Anim: %dms")) {
        config.set<int>("nxr.gui::anim_durr", duration);
    }

    bool horizontal_center = config.get<bool>("nxr.gui::horizontal_center", false);
    if (NXRWidget::Checkbox("Horizontal Center", &horizontal_center)) {
        config.set<bool>("nxr.gui::horizontal_center", horizontal_center);
        g_reopenAfterRecalc = true;
        ToggleUI();
    }

    bool notify_updates = config.get<bool>("nxr.ui.notify_updates", true);
    if (NXRWidget::Checkbox("Notify about updates", &notify_updates)) {
        config.set<bool>("nxr.ui.notify_updates", notify_updates);
    }

    if (NXRWidget::Button("Show Cheat Hack List", {ImGui::GetContentRegionAvail().x, 0})) {
        auto& activeCheats = NXR::Gui::get().getActiveCheats();

        std::string text = "";
        for (const auto& hackID : activeCheats)
            text += hackID + "\n";

        if (text.empty())
            text += "No cheat hacks enabled";

        NXRWidget::AddPopup(text);
    }

    if (NXRWidget::Button("Disable cheating hacks", {ImGui::GetContentRegionAvail().x, 0})) {
        ImGui::OpenPopup("Disable cheating hacks##Confirm");
    }

    if (ImGui::BeginPopupModal("Disable cheating hacks##Confirm", 0, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto& layout = NXR::Layout::Manager::get();
        auto glow_in = ImGui::ColorConvertFloat4ToU32(NXRWidget::colorTable[NXRWidget::Glow_Popup_Warning_In]);
        auto glow_out = ImGui::ColorConvertFloat4ToU32(NXRWidget::colorTable[NXRWidget::Glow_Popup_Warning_Out]);

        NXRWidget::GlowWindow(glow_in, glow_out, layout.multipleScale(500.f));
        ImGui::Text("Are you sure you want to turn off cheating hacks?");

        if (NXRWidget::Button("Cancel", {ImGui::GetContentRegionAvail().x, 0})) {
            ImGui::CloseCurrentPopup();
        }

        if (NXRWidget::Button("OK", {ImGui::GetContentRegionAvail().x, 0})) {
            NXR::Gui::get().disableCheats();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    NXRWidget::SpaceSeparator();

    if (NXRWidget::Button("Refresh Layout", {ImGui::GetContentRegionAvail().x, 0})) {
        g_resetLayoutCalled = true;
    }

    if (NXRWidget::Button(g_showThemeEditor ? "Disable Theme Editor" : "Theme Editor", {ImGui::GetContentRegionAvail().x, 0})) {
        g_reopenAfterTheme = true;
        if (g_showThemeEditor) g_reopenAfterRecalc = true;
        ToggleUI();
    }

    if (layoutManager.isCollecting()) {
        auto size = ImGui::GetWindowSize();
        layoutManager.addWindowInfo(windowName, size.x, size.y);
    }

    ImGui::End();
}

void KeybindsRender() {
    auto& layoutManager = NXR::Layout::Manager::get();
    auto& config = NXRConfig::get();
    auto& kb = NXR::Keybinds::get();

    std::string windowName = "Keybinds";
    layoutManager.applyWindowTransform(windowName);

    ImGui::Begin(windowName.c_str());

    NXRWidget::Checkbox("Keybinds Mode", &kb.m_isKeybindsMode);

    for (const auto& [actionId, actionLabel] : kb.customActions()) {
        NXRWidgetConfig::DrawCustomKeybindButton(actionId, actionLabel, geode::Keybind());
    }

    if (layoutManager.isCollecting()) {
        auto size = ImGui::GetWindowSize();
        layoutManager.addWindowInfo(windowName, size.x, size.y);
    }

    ImGui::End();
}

void RenderVersionBadge() {
    ImGuiIO& io = ImGui::GetIO();
    auto& layout = NXR::Layout::Manager::get();
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    ImFont* font = ImGui::GetFont();

    float fontSize = layout.multipleScale(32);
    std::string versionText = geode::Mod::get()->getVersion().toVString();

    ImVec2 labelSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "NXR");
    ImVec2 versionSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, versionText.c_str());

    float pillWidth = layout.multipleScale(24.0f) + labelSize.x;
    float pillHeight = labelSize.y + layout.multipleScale(10.0f);
    float sidePadding = layout.multipleScale(12.0f);
    float totalWidth = sidePadding + pillWidth + layout.multipleScale(12.0f) + versionSize.x + sidePadding;
    float totalHeight = pillHeight + layout.multipleScale(16.0f);

    float screenPadding = layout.multipleScale(10.0f);
    ImVec2 badgePosMin = ImVec2(io.DisplaySize.x - totalWidth - screenPadding, io.DisplaySize.y - totalHeight - screenPadding);
    ImVec2 badgePosMax = ImVec2(io.DisplaySize.x - screenPadding, io.DisplaySize.y - screenPadding);

    ImVec2 pillPosMin = ImVec2(badgePosMin.x + sidePadding, badgePosMin.y + layout.multipleScale(8.0f));
    ImVec2 pillPosMax = ImVec2(pillPosMin.x + pillWidth, pillPosMin.y + pillHeight);
    drawList->AddRectFilled(badgePosMin, badgePosMax, ImGui::GetColorU32(IM_COL32(0, 0, 0, 125)), 999.f);
    drawList->AddRectFilled(pillPosMin, pillPosMax, ImGui::GetColorU32(IM_COL32(242, 128, 43, 225)), 999.f);

    ImVec2 labelTextPos = ImVec2(pillPosMin.x + layout.multipleScale(12.0f), pillPosMin.y + layout.multipleScale(5.0f));
    ImVec2 versionTextPos = ImVec2(pillPosMax.x + layout.multipleScale(12.0f), badgePosMin.y + layout.multipleScale(13.0f));
    drawList->AddText(font, fontSize, labelTextPos, ImGui::GetColorU32(IM_COL32(51, 37, 30, 225)), "NXR");
    drawList->AddText(font, fontSize, versionTextPos, ImGui::GetColorU32(IM_COL32(242, 128, 43, 225)), versionText.c_str());
}

void notifyUpdate() {
    if (g_showUpdatePopup) {
        ImGui::OpenPopup("Update Available##Popup");
        g_showUpdatePopup = false;
    }

    if (ImGui::BeginPopupModal("Update Available##Popup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto& layout = NXR::Layout::Manager::get();
        auto& config = NXRConfig::get();
        bool dont_show_again = !config.get<bool>("nxr.ui.notify_updates", true);

        auto glow_in = ImGui::ColorConvertFloat4ToU32(NXRWidget::colorTable[NXRWidget::Glow_Popup_Update_In]);
        auto glow_out = ImGui::ColorConvertFloat4ToU32(NXRWidget::colorTable[NXRWidget::Glow_Popup_Update_Out]);
        NXRWidget::GlowWindow(glow_in, glow_out, layout.multipleScale(500.f));

        float oldScale = ImGui::GetFont()->Scale;
        ImGui::GetFont()->Scale = layout.multipleScale(32.0f) / ImGui::GetFontSize();
        ImGui::PushFont(ImGui::GetFont());

        ImGui::Text("A new update is available!\nPlease open the Geode menu and download the latest NXR update to get new features and bug fixes");
        if (dont_show_again) {
            ImGui::TextColored(ImColor(255, 128, 128).Value, "\nEven if you turn off this popup, please keep NXR updated\nIt's still in beta, so updating helps avoid bugs and issues from older versions :(");
        }

        ImGui::GetFont()->Scale = oldScale;
        ImGui::PopFont();

        if (NXRWidget::Checkbox("Don't show again", &dont_show_again)) {
            config.set<bool>("nxr.ui.notify_updates", !dont_show_again);
        }

        if (NXRWidget::Button("OK", {ImGui::GetContentRegionAvail().x, 0})) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void RenderMain() {
    auto& gui = NXR::Gui::get();
    auto& windows = gui.getWindows();
    auto& config = NXRConfig::get();
    auto& layoutManager = NXR::Layout::Manager::get();
    auto& kb = NXR::Keybinds::get();
    ImGuiIO &io = ImGui::GetIO();

    animateAlpha();

    NXRWidget::RenderPopups();

    if (!g_show) return;

    static bool lastRightPressed = false;
    bool isRightPressed = NXR::Keybinds::get().isMouseButtonDown(geode::MouseInputData::Button::Right);

    if (isRightPressed != lastRightPressed) {
        io.AddMouseButtonEvent(1, isRightPressed);
        lastRightPressed = isRightPressed;
    }

    RenderVersionBadge();
    SettingsRender();
    notifyUpdate();

    if (g_showThemeEditor) {
        NXRWidget::DrawColorEditor();
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.f);
    }

    KeybindsRender();

    for (auto& window : windows) {
        std::string windowName = window.getName();
        if (windowName == "Settings") continue;

        layoutManager.applyWindowTransform(windowName);

        NXRWidget::BeginSmoothScroll(windowName.c_str());

        if (window.avaibleCustomWindowImGui()) window.callCustomWindowImGui();

        for (auto& hack : window.getHacks()) {
            if (kb.m_isKeybindsMode) {
                NXRWidgetConfig::DrawKeybindButton(windowName, hack);
                continue;
            }

            std::string id = hack.getID();
            std::string hackName = hack.getName();

            bool state = config.get(id, false);

            PushAnimateFoundColor(hackName);
            if (hack.isCheating()) ImGui::PushStyleColor(ImGuiCol_Text, ImColor(255, 128, 128).Value);

            ImGui::BeginDisabled(hack.getDisabled());
            if (NXRWidget::Checkbox(hackName.c_str(), &state)) {
                hack.toggle();
            }
            ImGui::EndDisabled();

            if (hack.isCheating()) ImGui::PopStyleColor();

            NXRWidget::Tooltip(hack.getDesc().c_str(), !hack.getDesc().empty() && ImGui::IsItemHovered());

            if (hack.avaibleCustomWindowImGui()) {
                ImGui::SameLine();
                if (NXRWidget::ArrowButton(fmt::format("{} Settings", hackName).c_str(), ImGuiDir_Right)) {
                    ImGui::OpenPopup(fmt::format("{} Settings##Popup", hackName).c_str());
                }

                if (ImGui::BeginPopup(fmt::format("{} Settings##Popup", hackName).c_str(), NULL)) {
                    hack.callCustomWindowImGui();
                    ImGui::EndPopup();
                }
            }
            ImGui::PopStyleVar();
        }

        if (layoutManager.isCollecting()) {
            auto size = ImGui::GetWindowSize();
            layoutManager.addWindowInfo(windowName, size.x, size.y);
        }

        NXRWidget::EndSmoothScroll();
    }

    if (g_showThemeEditor) ImGui::PopStyleVar();

    if (layoutManager.isCollecting()) layoutManager.finishCollecting();
    else if (layoutManager.isApplying()) layoutManager.finishApplying();

    static bool g_inited = false;

    if (g_inited) {
        static ImVec2 lastSize = ImVec2(0, 0);
        ImVec2 currentSize = io.DisplaySize;

        if (g_hardRecalculation || currentSize.x != lastSize.x || currentSize.y != lastSize.y) {
            g_hardRecalculation = false;
            layoutManager.startCollecting();
            lastSize = currentSize;
        }

        if (g_resetLayoutCalled) {
            g_resetLayoutCalled = false;
            layoutManager.startApplying();
        }
    }
    else {
        layoutManager.setLayout(g_layout);
        layoutManager.setFixedWindowSizeInfo(g_fixedWindowSizes);

        layoutManager.startCollecting();
        g_inited = true;
    }
}

class $modify(NXRImGuiInitMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

		static bool inited = false;
        if (!inited) {
            NXR::Keybinds::get().registerAction("nxr.gui::toggle_ui", "Toggle UI", geode::Keybind(cocos2d::KEY_Z, geode::KeyboardModifier::None),
                [](bool repeat) {
                    if (repeat || ImGui::GetIO().WantTextInput) return;
                    ToggleUI();
                }
            );

            auto mod = geode::Mod::get();
            ImGuiCocos::get().setForceLegacy(mod->getSettingValue<bool>("nxr-legacy-render"));

            ImGuiCocos::get().setup([] {
                auto& config = NXRConfig::get();
                ImGuiIO &io = ImGui::GetIO();
                io.IniFilename = NULL;

                NXRWidget::ProccessOriginalTheme(true);
                NXRWidget::ApplyNativeGuiColors();
                NXRWidget::ApplyStyle(1.f);

                NXRWidget::LoadTheme();

                io.Fonts->AddFontFromMemoryCompressedTTF(roboto_font_data, roboto_font_size, 18.f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
            }).draw([] {
                RenderMain();
            });
            inited = true;
        }

		return true;
    }
};

#endif
