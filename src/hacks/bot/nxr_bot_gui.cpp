#include <Geode/Geode.hpp>
#include <algorithm>
#include <cmath>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_bot.hpp"
#include <fstream>
#include "../../core/nxr_macro_import.hpp"
#include "../../core/nxr_keybinds.hpp"
#include "../../interface/cocos/nxr_hacks_tab.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"
#include "../../interface/cocos/nxr_bot_popups.hpp"
#include "../../interface/imgui/nxr_widget_helper.hpp"
#include "../../interface/imgui/nxr_widget.hpp"
#include "imgui.h"

namespace {
    using namespace NXR::Bot;

    constexpr const char* kIgnoreKey = "nxr.bot.bot_engine::ignore_input";
    constexpr const char* kSpeedKey = "nxr.bot.speed";
    constexpr const char* kPracticeFixKey = "nxr.bot.practice_fix";
    constexpr const char* kFixRandomKey = "nxr.bot.fix_random";
    constexpr const char* kCbfKey = "nxr.bot.cbf_bypass";
    constexpr const char* kIndLineKey = "nxr.bot.click_indicator::line_color";
    constexpr const char* kIndBodyKey = "nxr.bot.click_indicator::body_color";
    constexpr const char* kIndBodyOpacityKey = "nxr.bot.click_indicator::body_opacity";
    constexpr const char* kIndPerfectKey = "nxr.bot.click_indicator::perfect";
    constexpr const char* kIndPerfectColorKey = "nxr.bot.click_indicator::perfect_color";
    constexpr const char* kIndFadeKey = "nxr.bot.click_indicator::fade_time";
    constexpr const char* kIndSlideKey = "nxr.bot.click_indicator::slide";
    constexpr const char* kIndHudKey = "nxr.bot.click_indicator::hud";
    constexpr const char* kIndHudSecondsKey = "nxr.bot.click_indicator::hud_seconds";
    constexpr const char* kIndHudXKey = "nxr.bot.click_indicator::hud_x";
    constexpr const char* kIndSoundKey = "nxr.bot.click_indicator::sound";
    constexpr const char* kIndSoundVolumeKey = "nxr.bot.click_indicator::sound_volume";
    constexpr const char* kIndEffectKey = "nxr.bot.click_indicator::effect";
    constexpr const char* kIndTrailColorKey = "nxr.bot.click_indicator::trail_color";
    constexpr const char* kIndTrailAheadKey = "nxr.bot.click_indicator::trail_ahead";
    constexpr const char* kIndTrailWidthKey = "nxr.bot.click_indicator::trail_width";
    constexpr const char* kIndP2ColorKey = "nxr.bot.click_indicator::p2_color";
    constexpr const char* kIndPlayerLineKey = "nxr.bot.click_indicator::player_line";
    constexpr const char* kIndHudScaleKey = "nxr.bot.click_indicator::hud_scale";
    constexpr const char* kIndHudInfoKey = "nxr.bot.click_indicator::hud_info";
    constexpr const char* kIndHideStatsKey = "nxr.bot.click_indicator::hide_stats";
    constexpr const char* kSaveJsonKey = "nxr.bot.save_json";
    constexpr const char* kRecordHereKey = "nxr.bot.record_here";
    constexpr const char* kAutosaveKey = "nxr.bot.autosave";
    constexpr const char* kAutosaveWinKey = "nxr.bot.autosave_win";
    constexpr const char* kAutosaveSecondsKey = "nxr.bot.autosave_seconds";
    constexpr const char* kAutosaveKeepKey = "nxr.bot.autosave_keep";
    constexpr float kDefaultSpeed = 0.5f;

    void notify(const std::string& text, geode::NotificationIcon icon) {
        geode::Notification::create(text, icon)->show();
    }

    int currentMode() {
        switch (State::get().mode) {
            case Mode::Recording: return 1;
            case Mode::Playing: return 2;
            case Mode::Off:
            default: return 0;
        }
    }

    bool startPlayback() {
        auto& st = State::get();
        if (st.selectedReplay.empty()) {
            notify("Select a replay first", geode::NotificationIcon::Warning);
            return false;
        }

        flushAsyncSaves();

        Macro macro;
        const bool useMemory = !st.current.events.empty() && st.current.name == st.selectedReplay;
        if (useMemory) {
            macro = st.current;
        } else if (!loadMacro(macro, macroPathFor(st.selectedReplay))) {
            notify("Failed to load replay", geode::NotificationIcon::Error);
            return false;
        }


        if (std::abs(macro.tps - effectiveTps()) > 0.5f) {
            notify(fmt::format("Replay TPS {:.0f} but current TPS {:.0f}. Set Physics TPS in FPS Limiter to {:.0f} for exact timing", macro.tps, effectiveTps(), macro.tps), geode::NotificationIcon::Warning);
        }

        if (auto* pl = PlayLayer::get(); pl && pl->m_level && macro.levelId != 0 && macro.levelId != static_cast<int32_t>(pl->m_level->m_levelID.value())) {
            notify(fmt::format("Replay is for level '{}' (ID {}), not this level", macro.levelName, macro.levelId), geode::NotificationIcon::Warning);
        }

        st.ignoreInput = NXRConfig::get().get<bool>(kIgnoreKey, true);
        st.startPlaying(std::move(macro));
        return true;
    }

    int selectMode(int mode) {
        auto& st = State::get();
        switch (mode) {
            case 1:
                if (NXRConfig::get().get<bool>(kRecordHereKey, false) && PlayLayer::get()) st.startRecordingHere();
                else st.startRecording();
                break;
            case 2:
                if (!startPlayback()) return currentMode();
                break;
            case 0:
            default:
                st.stop();
                break;
        }
        return currentMode();
    }

    void stepReplay(int dir) {
        auto& st = State::get();
        auto names = listMacros();
        if (names.empty()) return;

        auto it = std::find(names.begin(), names.end(), st.selectedReplay);
        int index = it == names.end() ? (dir > 0 ? -1 : 0) : static_cast<int>(it - names.begin());
        const int count = static_cast<int>(names.size());
        index = ((index + dir) % count + count) % count;
        st.selectedReplay = names[index];
    }

    std::string replayLabel() {
        auto& st = State::get();
        return st.selectedReplay.empty() ? "(none)" : st.selectedReplay;
    }

    std::string cleanName(const std::string& raw) {
        std::string out;
        for (char c : raw) {
            if (c >= 32 && std::string("\\/:*?\"<>|").find(c) == std::string::npos) out.push_back(c);
        }
        while (!out.empty() && out.back() == ' ') out.pop_back();
        while (!out.empty() && out.front() == ' ') out.erase(out.begin());
        return out;
    }

    bool createReplay(const std::string& raw) {
        std::string name = cleanName(raw);
        if (name.empty()) {
            notify("Enter a file name", geode::NotificationIcon::Warning);
            return false;
        }

        auto& st = State::get();
        st.stop();
        st.current.clear();
        st.current.name = name;
        st.selectedReplay = name;
        if (!saveMacro(st.current, macroPathFor(name))) {
            st.selectedReplay.clear();
            notify("Failed to create replay file", geode::NotificationIcon::Error);
            return false;
        }
        notify(fmt::format("New replay \"{}\"", name), geode::NotificationIcon::Info);
        return true;
    }

    void saveReplay() {
        auto& st = State::get();
        if (st.selectedReplay.empty()) {
            notify("Press New and enter a file name first", geode::NotificationIcon::Warning);
            return;
        }

        st.current.name = st.selectedReplay;
        if (!st.current.events.empty()) {
            st.current.totalFrames = std::max(st.current.totalFrames, st.current.events.back().frame());
        }
        const auto path = macroPathFor(st.selectedReplay);
        if (saveMacro(st.current, path)) {
            if (NXRConfig::get().get<bool>(kSaveJsonKey, false)) saveMacroJson(st.current, macroJsonPathFor(st.selectedReplay));
            notify(fmt::format("Saved \"{}\" ({} actions, {} frames)", st.selectedReplay, st.current.events.size(), st.current.totalFrames), geode::NotificationIcon::Success);

            deleteMacro("_autosave");
        } else {
            notify("Failed to save replay", geode::NotificationIcon::Error);
        }
    }

    void loadReplayByName(const std::string& name) {
        auto& st = State::get();
        st.stop();
        Macro macro;
        if (loadMacro(macro, macroPathFor(name))) {
            st.current = std::move(macro);
            st.current.name = name;
            st.selectedReplay = name;
            st.resetRun();
            notify(fmt::format("Loaded \"{}\" ({} actions, {} frames)", name, st.current.events.size(), st.current.totalFrames), geode::NotificationIcon::Success);
        } else {
            notify("Failed to load replay", geode::NotificationIcon::Error);
        }
    }

    void exportReplayJson(const std::string& name) {
        auto& st = State::get();
        flushAsyncSaves();

        Macro macro;
        if (!st.current.events.empty() && st.current.name == name) {
            macro = st.current;
        } else if (!loadMacro(macro, macroPathFor(name))) {
            notify("Failed to load replay", geode::NotificationIcon::Error);
            return;
        }

        macro.name = name;
        if (saveMacroJson(macro, macroJsonPathFor(name))) {
            notify(fmt::format("Exported \"{}.gdr.json\" ({} actions)", name, macro.events.size()), geode::NotificationIcon::Success);
        } else {
            notify("Failed to export JSON", geode::NotificationIcon::Error);
        }
    }

    void deleteReplayByName(const std::string& name) {
        auto& st = State::get();
        if (deleteMacro(name)) {
            if (st.selectedReplay == name) st.selectedReplay.clear();
            notify(fmt::format("Deleted \"{}\"", name), geode::NotificationIcon::Success);
        } else {
            notify("Failed to delete replay", geode::NotificationIcon::Error);
        }
    }

    struct RateTracker {
        float fps = 60.f;
        float tps = 240.f;
        uint64_t lastTicks = 0;
        float accumulated = 0.f;
    };

    RateTracker g_rate;

    void updateRates(float dt) {
        auto* director = cocos2d::CCDirector::sharedDirector();
        const float real = director ? director->getDeltaTime() : dt;
        if (real <= 0.f) return;

        g_rate.fps = g_rate.fps * 0.9f + (1.f / real) * 0.1f;
        g_rate.accumulated += real;

        if (g_rate.accumulated >= 0.5f) {
            const uint64_t ticks = State::get().ticks;
            g_rate.tps = static_cast<float>(ticks - g_rate.lastTicks) / g_rate.accumulated;
            g_rate.lastTicks = ticks;
            g_rate.accumulated = 0.f;
        }
    }

    std::string rateText() {
        auto& st = State::get();
        const float shownTps = (g_rate.tps > 1.f && PlayLayer::get()) ? g_rate.tps : effectiveTps();
        std::string text = fmt::format("FPS {:.0f} | TPS {:.0f}", g_rate.fps, shownTps);
        if (!st.current.events.empty() && std::abs(st.current.tps - effectiveTps()) > 0.5f) {
            text += fmt::format(" | Replay TPS {:.0f}", st.current.tps);
        }
        return text;
    }

    std::string uniqueReplayName(const std::string& base) {
        std::string name = cleanName(base);
        if (name.empty()) name = "imported";
        return name;
    }

    void loadImported(const std::filesystem::path& path) {
        auto& st = State::get();
        Macro macro;
        std::string format;

        if (!importReplay(path, macro, format)) {
            notify("Unsupported or corrupted replay", geode::NotificationIcon::Error);
            return;
        }

        st.stop();

        const std::string name = uniqueReplayName(macro.name.empty() ? path.stem().string() : macro.name);
        macro.name = name;

        std::error_code ec;
        const auto target = macroPathFor(name);
        const bool same = std::filesystem::exists(target, ec) && std::filesystem::equivalent(target, path, ec);
        if (!same) {
            if (std::filesystem::exists(target, ec)) {
                std::filesystem::copy_file(target, macroPathFor(backupNameNow("import")), std::filesystem::copy_options::overwrite_existing, ec);
                pruneBackups(10);
            }
            saveMacro(macro, target);
        }

        const size_t actions = macro.events.size();
        st.current = std::move(macro);
        st.selectedReplay = name;
        st.resetRun();
        notify(fmt::format("Loaded \"{}\" as {} ({} actions)", name, format, actions), geode::NotificationIcon::Success);
    }

    std::vector<ReplayFile> g_browseFiles;
    std::string g_browsePick;

    void drawBrowser() {
        if (!ImGui::BeginPopupModal("Browse Replays##nxr_bot", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

        if (ImGui::BeginListBox("##nxr_bot_browse", ImVec2(360.f, 180.f))) {
            for (auto& file : g_browseFiles) {
                const std::string label = file.label + "  [" + file.ext + "]";
                const std::string id = file.path.string();
                if (ImGui::Selectable(label.c_str(), g_browsePick == id)) g_browsePick = id;
            }
            ImGui::EndListBox();
        }

        if (NXRWidget::Button("Load")) {
            if (g_browsePick.empty()) {
                notify("Pick a replay first", geode::NotificationIcon::Warning);
            } else {
                loadImported(std::filesystem::path(g_browsePick));
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (NXRWidget::Button("Cancel")) ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    std::string infoText() {
        auto& st = State::get();
        size_t clicks = 0;
        for (auto& ev : st.current.events) {
            if (ev.down()) clicks++;
        }
        return fmt::format("Actions: {}  |  Clicks: {}  |  Frames: {}  |  Super: {}  |  Frame: {}", st.current.events.size(), clicks, st.current.frames.size(), st.current.supers.size(), st.frame);
    }

    char g_nameBuf[64] = "";
    std::string g_pickSelected;

    void drawPicker(const char* id, const char* actionLabel, void (*action)(const std::string&)) {
        if (!ImGui::BeginPopupModal(id, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

        auto names = listMacros();
        if (ImGui::BeginListBox("##nxr_bot_pick", ImVec2(260.f, 140.f))) {
            for (auto& name : names) {
                if (ImGui::Selectable(name.c_str(), g_pickSelected == name)) g_pickSelected = name;
            }
            ImGui::EndListBox();
        }

        if (NXRWidget::Button(actionLabel)) {
            if (g_pickSelected.empty()) {
                notify("Pick a replay first", geode::NotificationIcon::Warning);
            } else {
                action(g_pickSelected);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (NXRWidget::Button("Cancel")) ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    bool mergeReplays(const std::string& nameA, const std::string& nameB, MergeMode mode, const std::string& outName) {
        flushAsyncSaves();

        Macro a, b, merged;
        if (!loadMacro(a, macroPathFor(nameA))) { notify("Gagal load macro 1", geode::NotificationIcon::Error); return false; }
        if (!loadMacro(b, macroPathFor(nameB))) { notify("Gagal load macro 2", geode::NotificationIcon::Error); return false; }

        MergeReport report;
        std::string error;
        if (!mergeMacros(a, b, mode, merged, report, error)) {
            notify(error, geode::NotificationIcon::Error);
            return false;
        }

        const std::string name = cleanName(outName);
        if (name.empty()) { notify("Enter a file name", geode::NotificationIcon::Warning); return false; }

        State::get().stop();
        merged.name = name;
        if (!saveMacro(merged, macroPathFor(name))) {
            notify("Gagal menyimpan hasil merge", geode::NotificationIcon::Error);
            return false;
        }

        auto& st = State::get();
        st.current = std::move(merged);
        st.selectedReplay = name;
        st.resetRun();
        notify(fmt::format("Merged \"{}\" ({}, {} actions)", name, report.note, report.events), geode::NotificationIcon::Success);
        return true;
    }

    std::string g_mergeA, g_mergeB;
    int g_mergeMode = 0;
    char g_mergeName[64] = "";

    void drawMergePopup() {
        if (!ImGui::BeginPopupModal("Merge Replays##nxr_bot", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

        auto names = listMacros();
        auto combo = [&](const char* label, const char* id, std::string& value) {
            ImGui::TextUnformatted(label);
            ImGui::SetNextItemWidth(240.f);
            if (ImGui::BeginCombo(id, value.empty() ? "(pilih)" : value.c_str())) {
                for (auto& n : names) {
                    if (ImGui::Selectable(n.c_str(), n == value)) value = n;
                }
                ImGui::EndCombo();
            }
        };

        combo("Macro 1 (utama / awal)", "##nxr_merge_a", g_mergeA);
        combo("Macro 2 (sambungan / P2)", "##nxr_merge_b", g_mergeB);

        if (NXRWidget::RadioButton("Sambung waktu (startpos)", g_mergeMode == 0)) g_mergeMode = 0;
        if (NXRWidget::RadioButton("Gabung P1 + P2", g_mergeMode == 1)) g_mergeMode = 1;

        ImGui::InputTextWithHint("##nxr_merge_name", "Nama hasil", g_mergeName, sizeof(g_mergeName));

        if (NXRWidget::Button("Merge")) {
            if (g_mergeA.empty() || g_mergeB.empty()) {
                notify("Pilih kedua macro dulu", geode::NotificationIcon::Warning);
            } else if (mergeReplays(g_mergeA, g_mergeB, g_mergeMode == 1 ? MergeMode::Players : MergeMode::Append, g_mergeName)) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (NXRWidget::Button("Cancel")) ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    void startMergeFlow() {
        auto* first = NXRReplayPickerPopup::create("Merge: pilih macro 1", "Pilih", [](const std::string& nameA) {
            auto* second = NXRReplayPickerPopup::create("Merge: pilih macro 2", "Pilih", [nameA](const std::string& nameB) {
                geode::createQuickPopup(
                    "Mode Merge",
                    "<cy>Sambung</c>: macro 2 direkam dari startpos, checkpoint, atau Record Without Restart, lalu disambung ke macro 1.\n"
                    "<cg>P1 + P2</c>: P1 dari macro 1, P2 dari macro 2 (yang lain noclip).",
                    "Sambung", "P1 + P2",
                    [nameA, nameB](auto*, bool players) {
                        auto mode = players ? MergeMode::Players : MergeMode::Append;
                        if (auto* namePopup = NXRNamePopup::create("Nama hasil merge", [nameA, nameB, mode](const std::string& out) {
                            mergeReplays(nameA, nameB, mode, out);
                        })) namePopup->show();
                    }
                );
            });
            if (second) second->show();
        });
        if (first) first->show();
    }

    void restoreAutosave() {
        auto& st = State::get();
        std::error_code ec;
        auto dir = getFolderMacroPath();

        std::filesystem::path newest;
        std::filesystem::file_time_type newestTime{};
        bool found = false;

        if (std::filesystem::is_directory(dir, ec)) {
            for (auto& entry : std::filesystem::directory_iterator(dir, ec)) {
                if (ec) break;
                if (!entry.is_regular_file()) continue;
                const auto& p = entry.path();
                if (p.extension() != ".nxr") continue;
                const auto stem = p.stem().string();
                if (stem != "_autosave" && stem.rfind("_backup_", 0) != 0) continue;

                const auto t = entry.last_write_time(ec);
                if (ec) { ec.clear(); continue; }
                if (!found || t > newestTime) {
                    newest = p;
                    newestTime = t;
                    found = true;
                }
            }
        }

        if (!found) {
            notify("No auto save found", geode::NotificationIcon::Warning);
            return;
        }

        flushAsyncSaves();
        st.stop();
        Macro macro;
        if (!loadMacro(macro, newest)) {
            notify("Failed to load auto save", geode::NotificationIcon::Error);
            return;
        }

        const std::string label = "restored_" + newest.stem().string();
        macro.name = label;
        st.current = std::move(macro);
        st.resetRun();
        st.selectedReplay = label;
        if (saveMacro(st.current, macroPathFor(label))) {
            notify(fmt::format("Restored \"{}\" ({} actions)", label, st.current.events.size()), geode::NotificationIcon::Success);
        } else {
            notify("Restored in memory, press Save to keep it", geode::NotificationIcon::Info);
        }
    }

    void openBrowser() {
        std::error_code ec;
        std::filesystem::create_directories(getFolderMacroPath(), ec);

        if (auto* popup = NXRReplayBrowserPopup::create("Open Replays", [](const std::filesystem::path& path) { loadImported(path); })) popup->show();
    }
}

class BotInfoNode : public cocos2d::CCNode {
protected:
    geode::Label* m_label = nullptr;
    int m_kind = 0;

    bool init() override {
        if (!cocos2d::CCNode::init()) return false;
        setContentSize({355.f, 22.f});
        m_label = geode::Label::create("", "GoogleSans.fnt"_spr);
        m_label->setScale(0.5f);
        m_label->setPosition({177.5f, 11.f});
        addChild(m_label);
        scheduleUpdate();
        refresh();
        return true;
    }

    void refresh() {
        m_label->setString((m_kind == 1 ? rateText() : infoText()).c_str());
    }

    void update(float dt) override {
        if (m_kind == 1) updateRates(dt);
        refresh();
    }

public:
    static BotInfoNode* create(int kind = 0) {
        auto* ret = new BotInfoNode();
        ret->m_kind = kind;
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

$execute {
    cleanupOrphanTemps();

    auto& win = NXR::Gui::get().getWindow("Bot");

    win.createHack("Click Indicator", "Shows where each click happens. It works during playback and also while you play manually: just pick a replay and turn this on. It plays a click sound at every click (res/click_indicator.mp3). Zone Follow marks every click as a box in the level with a marker at the height you need to be. Line draws the recorded path ahead with every click marked on it. Both can show Perfect. The Left Ramp switch adds a simple lane on the left that shows the upcoming clicks", false);
    win.createHack("Autosave", "Saves the recording in the background every few seconds and when the level is completed, and keeps timestamped backups", false);

    auto& indicator = win.findHackByName("Click Indicator");
    auto& autosave = win.findHackByName("Autosave");

    {
        auto& config = NXRConfig::get();
        const bool neverSet = config.get<bool>(kAutosaveKey, false) != config.get<bool>(kAutosaveKey, true);
        if (neverSet) config.set<bool>(kAutosaveKey, true);
    }

    indicator.setCustomWindowImGui([]{
        NXRWidgetConfig::RadioInt(kIndEffectKey, 1, {{"Zone Follow", 1}, {"Line", 2}});
        ImGui::Separator();
        NXRWidgetConfig::Checkbox("Left Ramp", kIndHudKey, true);
        NXRWidgetConfig::Checkbox("Ramp Info Text", kIndHudInfoKey, true);
        NXRWidgetConfig::Checkbox("Hide FPS/CPS Counter", kIndHideStatsKey, false);
        NXRWidgetConfig::DragFloat("##nxr_ind_hudsec", kIndHudSecondsKey, 0.05f, 0.3f, 4.f, 1.2f, "Ramp Length: %.2fs");
        NXRWidgetConfig::DragFloat("##nxr_ind_hudx", kIndHudXKey, 1.f, 10.f, 300.f, 70.f, "Ramp X: %.0f");
        NXRWidgetConfig::DragFloat("##nxr_ind_hudscale", kIndHudScaleKey, 0.05f, 0.5f, 2.5f, 1.f, "Ramp Size: %.2f");
        NXRWidgetConfig::Checkbox("Player Line And Marker", kIndPlayerLineKey, true);
        NXRWidgetConfig::ColorEdit3Hex("Player 2 Color", kIndP2ColorKey, "C84DFF");
        ImGui::Separator();
        NXRWidgetConfig::ColorEdit3Hex("Line Color", kIndTrailColorKey, "39FF6E");
        NXRWidgetConfig::DragInt("##nxr_ind_trailahead", kIndTrailAheadKey, 1.f, 30, 2400, 300, "Line Length: %d frames");
        NXRWidgetConfig::DragFloat("##nxr_ind_trailwidth", kIndTrailWidthKey, 0.1f, 0.5f, 12.f, 2.5f, "Line Width: %.1f");
        ImGui::Separator();
        ImGui::Separator();
        NXRWidgetConfig::ColorEdit3Hex("Line Color", kIndLineKey, "FFFFFF");
        NXRWidgetConfig::ColorEdit3Hex("Background Color", kIndBodyKey, "00F0FF");
        NXRWidgetConfig::DragInt("##nxr_ind_bodyop", kIndBodyOpacityKey, 1.f, 0, 255, 70, "Background Opacity: %d");
        NXRWidgetConfig::DragFloat("##nxr_ind_fade", kIndFadeKey, 0.01f, 0.05f, 3.f, 0.8f, "Fade Time: %.2fs");
        NXRWidgetConfig::DragFloat("##nxr_ind_slide", kIndSlideKey, 1.f, 0.f, 400.f, 0.f, "Slide Distance: %.0f");
        ImGui::Separator();
        NXRWidgetConfig::Checkbox("Show Perfect", kIndPerfectKey, true);
        NXRWidgetConfig::ColorEdit3Hex("Perfect Color", kIndPerfectColorKey, "39FF6E");
        ImGui::Separator();
        NXRWidgetConfig::Checkbox("Click Sound", kIndSoundKey, true);
        NXRWidgetConfig::DragInt("##nxr_ind_soundvol", kIndSoundVolumeKey, 1.f, 0, 100, 80, "Sound Volume: %d");
    });

    indicator.setCustomWindowCocos([](cocos2d::CCNode* node) {
        auto* popup = static_cast<NXRHackSettingsPopup*>(node);
        popup->addConfigRadio("Effect", kIndEffectKey, {{"Zone Follow", 1}, {"Line", 2}}, 1);
        popup->addSeparator();
        popup->addConfigToggle("Left Ramp", kIndHudKey, true);
        popup->addConfigToggle("Ramp Info Text", kIndHudInfoKey, true);
        popup->addConfigToggle("Hide FPS/CPS Counter", kIndHideStatsKey, false);
        popup->addConfigFloatInput("Ramp Length (s)", kIndHudSecondsKey, 0.3f, 4.f, 1.2f);
        popup->addConfigFloatInput("Ramp X", kIndHudXKey, 10.f, 300.f, 70.f);
        popup->addConfigFloatInput("Ramp Size", kIndHudScaleKey, 0.5f, 2.5f, 1.f);
        popup->addConfigToggle("Player Line And Marker", kIndPlayerLineKey, true);
        popup->addConfigColor3Hex("Player 2 Color", kIndP2ColorKey, "C84DFF");
        popup->addSeparator();
        popup->addConfigColor3Hex("Line Color", kIndTrailColorKey, "39FF6E");
        popup->addConfigIntInput("Line Length (frames)", kIndTrailAheadKey, 30, 2400, 300);
        popup->addConfigFloatInput("Line Width", kIndTrailWidthKey, 0.5f, 12.f, 2.5f);
        popup->addSeparator();
        popup->addSeparator();
        popup->addConfigColor3Hex("Line Color", kIndLineKey, "FFFFFF");
        popup->addConfigColor3Hex("Background Color", kIndBodyKey, "00F0FF");
        popup->addConfigIntInput("Background Opacity", kIndBodyOpacityKey, 0, 255, 70);
        popup->addConfigFloatInput("Fade Time (s)", kIndFadeKey, 0.05f, 3.f, 0.8f);
        popup->addConfigFloatInput("Slide Distance", kIndSlideKey, 0.f, 400.f, 0.f);
        popup->addSeparator();
        popup->addConfigToggle("Show Perfect", kIndPerfectKey, true);
        popup->addConfigColor3Hex("Perfect Color", kIndPerfectColorKey, "39FF6E");
        popup->addSeparator();
        popup->addConfigToggle("Click Sound", kIndSoundKey, true);
        popup->addConfigIntInput("Sound Volume", kIndSoundVolumeKey, 0, 100, 80);
    });

    autosave.setCustomWindowImGui([]{
        NXRWidgetConfig::Checkbox("Save On Win", kAutosaveWinKey, true);
        NXRWidgetConfig::DragInt("##nxr_as_sec", kAutosaveSecondsKey, 1.f, 5, 600, 20, "Save Every: %ds");
        NXRWidgetConfig::DragInt("##nxr_as_keep", kAutosaveKeepKey, 1.f, 1, 100, 10, "Keep Backups: %d");
    });

    autosave.setCustomWindowCocos([](cocos2d::CCNode* node) {
        auto* popup = static_cast<NXRHackSettingsPopup*>(node);
        popup->addConfigToggle("Save On Win", kAutosaveWinKey, true);
        popup->addConfigIntInput("Save Every (s)", kAutosaveSecondsKey, 5, 600, 20);
        popup->addConfigIntInput("Keep Backups", kAutosaveKeepKey, 1, 100, 10);
    });

    win.setCustomWindowImGui([]{
        auto& st = State::get();
        int mode = currentMode();

        if (NXRWidget::RadioButton("Disabled", mode == 0)) selectMode(0);
        ImGui::SameLine();
        if (NXRWidget::RadioButton("Record", mode == 1)) selectMode(1);
        ImGui::SameLine();
        if (NXRWidget::RadioButton("Playback", mode == 2)) selectMode(2);

        ImGui::Text("Replays");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.f);
        if (ImGui::BeginCombo("##nxr_bot_replays", replayLabel().c_str())) {
            if (ImGui::Selectable("(none)", st.selectedReplay.empty())) st.selectedReplay.clear();
            for (auto& name : listMacros()) {
                if (ImGui::Selectable(name.c_str(), name == st.selectedReplay)) st.selectedReplay = name;
            }
            ImGui::EndCombo();
        }

        NXRWidgetConfig::Checkbox("Record Without Restart", kRecordHereKey, false);
        if (NXRWidgetConfig::Checkbox("Ignore Inputs", kIgnoreKey, true)) {
            st.ignoreInput = NXRConfig::get().get<bool>(kIgnoreKey, true);
        }
        NXRWidgetConfig::Checkbox("Practice Fixes", kPracticeFixKey, true);
        NXRWidgetConfig::Checkbox("Fix Random", kFixRandomKey, true);
        NXRWidgetConfig::Checkbox("CBF Bypass", kCbfKey, true);
        NXRWidgetConfig::Checkbox("Also Save As JSON", kSaveJsonKey, false);

        if (NXRWidgetConfig::InputFloat("Speed (0.1 - 10000)", kSpeedKey, kDefaultSpeed)) {
            auto& config = NXRConfig::get();
            config.set<float>(kSpeedKey, std::clamp(config.get<float>(kSpeedKey, 0.5f), 0.1f, 10000.f));
        }

        ImGui::TextUnformatted(infoText().c_str());
        {
            static bool s_rateInit = false;
            if (!s_rateInit) {
                s_rateInit = true;
                g_rate.fps = ImGui::GetIO().Framerate;
            }
            updateRates(ImGui::GetIO().DeltaTime);
            ImGui::TextUnformatted(rateText().c_str());
        }

        const ImVec2 full(-1.f, 0.f);
        if (NXRWidget::Button("Restore Last Autosave", full)) restoreAutosave();
        if (NXRWidget::Button("New", full)) {
            g_nameBuf[0] = '\0';
            ImGui::OpenPopup("New Replay##nxr_bot");
        }
        if (NXRWidget::Button("Save", full)) saveReplay();
        if (NXRWidget::Button("Load", full)) {
            g_pickSelected.clear();
            ImGui::OpenPopup("Load Replay##nxr_bot");
        }
        if (NXRWidget::Button("Delete", full)) {
            g_pickSelected.clear();
            ImGui::OpenPopup("Delete Replay##nxr_bot");
        }
        if (NXRWidget::Button("Export JSON", full)) {
            g_pickSelected.clear();
            ImGui::OpenPopup("Export JSON##nxr_bot");
        }
        if (NXRWidget::Button("Merge Replays", full)) {
            g_mergeA.clear();
            g_mergeB.clear();
            g_mergeName[0] = '\0';
            ImGui::OpenPopup("Merge Replays##nxr_bot");
        }
        if (NXRWidget::Button("Browse Replays", full)) {
            g_browseFiles = scanReplayFiles();
            g_browsePick.clear();
            ImGui::OpenPopup("Browse Replays##nxr_bot");
        }

        if (ImGui::BeginPopupModal("New Replay##nxr_bot", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputTextWithHint("##nxr_bot_name", "File name", g_nameBuf, sizeof(g_nameBuf));
            if (NXRWidget::Button("OK")) {
                if (createReplay(g_nameBuf)) ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (NXRWidget::Button("Cancel")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        drawBrowser();
        drawMergePopup();
        drawPicker("Load Replay##nxr_bot", "Load", [](const std::string& name) { loadReplayByName(name); });
        drawPicker("Delete Replay##nxr_bot", "Delete", [](const std::string& name) { deleteReplayByName(name); });
        drawPicker("Export JSON##nxr_bot", "Export", [](const std::string& name) { exportReplayJson(name); });
    });

    win.setCustomWindowCocos([](cocos2d::CCNode* node) {
        auto* tab = static_cast<NXRHacksTab*>(node);

        tab->addPadding(6.f);
        tab->addRadioRow({"Disabled", "Record", "Playback"}, []{ return currentMode(); }, [](int mode) { return selectMode(mode); });
        tab->addSelector("Replays", []{ return replayLabel(); }, [](std::function<void()> refresh) {
            if (auto* popup = NXRReplayPickerPopup::create("Select Replay", "Select", [refresh](const std::string& name) {
                State::get().selectedReplay = name;
                refresh();
            }, true)) popup->show();
        });
        tab->addConfigToggle("Record Without Restart", kRecordHereKey, false);
        tab->addConfigToggle("Ignore Inputs", kIgnoreKey, true, [](bool value) { State::get().ignoreInput = value; });
        tab->addConfigToggle("Practice Fixes", kPracticeFixKey, true);
        tab->addConfigToggle("Fix Random", kFixRandomKey, true);
        tab->addConfigToggle("CBF Bypass", kCbfKey, true);
        tab->addConfigToggle("Also Save As JSON", kSaveJsonKey, false);
        tab->addConfigFloatInput("Speed (0.1 - 10000)", kSpeedKey, kDefaultSpeed, 0.1f, 10000.f);
        tab->prepareNewRow();
        tab->m_currentRow->addChild(BotInfoNode::create());
        tab->m_currentRow->updateLayout();
        tab->prepareNewRow();
        tab->m_currentRow->addChild(BotInfoNode::create(1));
        tab->m_currentRow->updateLayout();

        tab->addConfigButton(
            "New", []{
                if (auto* popup = NXRNamePopup::create("New Replay", [](const std::string& name) { createReplay(name); })) popup->show();
            },
            "Save", []{ saveReplay(); }
        );
        tab->addConfigButton(
            "Load", []{
                if (auto* popup = NXRReplayPickerPopup::create("Load Replay", "Load", [](const std::string& name) { loadReplayByName(name); })) popup->show();
            },
            "Delete", []{
                if (auto* popup = NXRReplayPickerPopup::create("Delete Replay", "Delete", [](const std::string& name) { deleteReplayByName(name); })) popup->show();
            }
        );
        tab->addConfigButton("Restore Last Autosave", []{ restoreAutosave(); }, "Browse Replays", []{ openBrowser(); });
        tab->addConfigButton(
            "Merge Replays", []{ startMergeFlow(); },
            "Export JSON", []{
                if (auto* popup = NXRReplayPickerPopup::create("Export JSON", "Export", [](const std::string& name) { exportReplayJson(name); })) popup->show();
            }
        );
        tab->addPadding(6.f);
    });
    auto& keybinds = NXR::Keybinds::get();
    keybinds.registerAction("nxr.bot::disable", "Bot: Disabled", geode::Keybind(), [](bool repeat) {
        if (!repeat) selectMode(0);
    });
    keybinds.registerAction("nxr.bot::record", "Bot: Record", geode::Keybind(), [](bool repeat) {
        if (!repeat) selectMode(currentMode() == 1 ? 0 : 1);
    });
    keybinds.registerAction("nxr.bot::playback", "Bot: Playback", geode::Keybind(), [](bool repeat) {
        if (!repeat) selectMode(currentMode() == 2 ? 0 : 2);
    });
    keybinds.registerAction("nxr.bot::prev_replay", "Bot: Previous Replay", geode::Keybind(), [](bool repeat) {
        if (!repeat) stepReplay(-1);
    });
    keybinds.registerAction("nxr.bot::next_replay", "Bot: Next Replay", geode::Keybind(), [](bool repeat) {
        if (!repeat) stepReplay(1);
    });
    keybinds.registerAction("nxr.bot::save", "Bot: Save Replay", geode::Keybind(), [](bool repeat) {
        if (!repeat) saveReplay();
    });

}
