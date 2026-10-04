#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <cmath>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_bot.hpp"
#include <fstream>
#include "../../core/nxr_macro_import.hpp"
#include "../../core/nxr_keybinds.hpp"
#include "../../core/nxr_ui_mode.hpp"
#include "../../interface/cocos/nxr_ui_page.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"
#include "../../interface/cocos/nxr_bot_popups.hpp"
#include "../../interface/imgui/nxr_imgui_menu.hpp"
#include <imgui.h>

namespace {
    using namespace NXR::Bot;

    constexpr const char* kIgnoreKey = "nxr.bot.bot_engine::ignore_input";
    constexpr const char* kSpeedKey = "nxr.bot.speed";
    constexpr const char* kPracticeFixKey = "nxr.bot.practice_fix";
    constexpr const char* kFixRandomKey = "nxr.bot.fix_random";
    constexpr const char* kCbfKey = "nxr.bot.cbf_bypass";
    constexpr const char* kPlaybackDeathKey = "nxr.bot.playback_death";
    constexpr const char* kDesyncRescueKey = "nxr.bot.desync_rescue";
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
    size_t g_savedEvents = 0;
    uint64_t g_savedFrames = 0;

    bool hasUnsavedRecording() {
        auto& st = State::get();
        if (st.current.events.empty()) return false;
        if (st.selectedReplay.empty()) return true;
        return st.current.events.size() != g_savedEvents
            || static_cast<uint64_t>(st.current.totalFrames) != g_savedFrames;
    }

    void markSaved() {
        auto& st = State::get();
        g_savedEvents = st.current.events.size();
        g_savedFrames = static_cast<uint64_t>(st.current.totalFrames);
    }
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

        const bool keep = hasUnsavedRecording();

        if (keep) {
            std::error_code ec;
            if (std::filesystem::exists(macroPathFor(name), ec)) {
                notify(fmt::format("\"{}\" already exists, pick another name", name), geode::NotificationIcon::Warning);
                return false;
            }
        } else {
            st.current.clear();
        }

        st.current.name = name;
        st.selectedReplay = name;
        if (!st.current.events.empty()) {
            st.current.totalFrames = std::max(st.current.totalFrames, st.current.events.back().frame());
        }

        if (!saveMacro(st.current, macroPathFor(name))) {
            st.selectedReplay.clear();
            notify("Failed to create replay file", geode::NotificationIcon::Error);
            return false;
        }
        markSaved();

        if (keep) {
            if (NXRConfig::get().get<bool>(kSaveJsonKey, false)) saveMacroJson(st.current, macroJsonPathFor(name));
            notify(fmt::format("Saved recording as \"{}\" ({} actions, {} frames)", name, st.current.events.size(), st.current.totalFrames), geode::NotificationIcon::Success);
            deleteMacro("_autosave");
        } else {
            notify(fmt::format("New replay \"{}\"", name), geode::NotificationIcon::Info);
        }
        return true;
    }

    void startNewFlow();

    void saveReplay() {
        auto& st = State::get();
        if (st.selectedReplay.empty()) {
            if (st.current.events.empty()) {
                notify("Nothing recorded yet", geode::NotificationIcon::Warning);
                return;
            }
            startNewFlow();
            return;
        }

        st.current.name = st.selectedReplay;
        if (!st.current.events.empty()) {
            st.current.totalFrames = std::max(st.current.totalFrames, st.current.events.back().frame());
        }
        const auto path = macroPathFor(st.selectedReplay);
        if (saveMacro(st.current, path)) {
            markSaved();
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
            markSaved();
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


    std::string g_lastLevel;

    std::string fileSafe(const std::string& raw) {
        std::string out;
        for (char c : raw) {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == ' ') out.push_back(c);
        }
        while (!out.empty() && (out.back() == ' ' || out.back() == '.')) out.pop_back();
        while (!out.empty() && out.front() == ' ') out.erase(out.begin());
        if (out.size() > 40) out.resize(40);
        while (!out.empty() && (out.back() == ' ' || out.back() == '.')) out.pop_back();
        return out;
    }

    std::string suggestedName() {
        std::string base;

        if (auto* pl = PlayLayer::get(); pl && pl->m_level) base = fileSafe(pl->m_level->m_levelName);
        if (base.empty()) base = fileSafe(g_lastLevel);
        if (base.empty()) base = fileSafe(State::get().current.levelName);
        if (base.empty()) base = fileSafe(NXRConfig::get().get<std::string>("nxr.bot.last_level", ""));
        if (base.empty()) base = "replay";

        std::error_code ec;
        if (!std::filesystem::exists(macroPathFor(base), ec)) return base;

        for (int i = 2; i < 1000; i++) {
            const std::string candidate = fmt::format("{}_{}", base, i);
            if (!std::filesystem::exists(macroPathFor(candidate), ec)) return candidate;
        }
        return base;
    }

    bool renameReplayByName(const std::string& oldName, const std::string& raw) {
        const std::string name = cleanName(raw);
        if (name.empty()) {
            notify("Enter a file name", geode::NotificationIcon::Warning);
            return false;
        }
        if (name == oldName) return true;

        flushAsyncSaves();

        std::error_code ec;
        const auto from = macroPathFor(oldName);
        const auto to = macroPathFor(name);

        if (!std::filesystem::exists(from, ec)) {
            notify("Replay not found", geode::NotificationIcon::Error);
            return false;
        }
        if (std::filesystem::exists(to, ec)) {
            notify(fmt::format("\"{}\" already exists", name), geode::NotificationIcon::Warning);
            return false;
        }

        std::filesystem::rename(from, to, ec);
        if (ec) {
            notify("Failed to rename replay", geode::NotificationIcon::Error);
            return false;
        }

        const auto jsonFrom = macroJsonPathFor(oldName);
        if (std::filesystem::exists(jsonFrom, ec)) std::filesystem::rename(jsonFrom, macroJsonPathFor(name), ec);

        auto& st = State::get();
        if (st.selectedReplay == oldName) st.selectedReplay = name;
        if (st.current.name == oldName) st.current.name = name;

        notify(fmt::format("Renamed to \"{}\"", name), geode::NotificationIcon::Success);
        return true;
    }

    void startNewFlow() {
        NXR::Ui::showPopup(NXRNamePopup::create("New Replay", [](const std::string& name) { createReplay(name); }, suggestedName()), "New Replay");
    }

    void startRenameFlow() {
        NXR::Ui::pickReplay("Rename Replay", "Rename", [](const std::string& oldName) {
            NXR::Ui::showPopup(NXRNamePopup::create("Rename Replay", [oldName](const std::string& name) { renameReplayByName(oldName, name); }, oldName), "Rename Replay");
        });
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
        if (st.mode == Mode::Playing && st.rescues > 0) text += fmt::format(" | Rescue {}", st.rescues);
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

    std::string infoText() {
        auto& st = State::get();
        size_t clicks = 0;
        for (auto& ev : st.current.events) {
            if (ev.down()) clicks++;
        }
        return fmt::format("Actions: {}  |  Clicks: {}  |  Frames: {}  |  Super: {}  |  Frame: {}", st.current.events.size(), clicks, st.current.frames.size(), st.current.supers.size(), st.frame);
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

    void startMergeFlow() {
        auto askName = [](const std::string& nameA, const std::string& nameB, MergeMode mode) {
            NXR::Ui::showPopup(NXRNamePopup::create("Nama hasil merge", [nameA, nameB, mode](const std::string& out) {
                mergeReplays(nameA, nameB, mode, out);
            }), "Nama hasil merge");
        };
        NXR::Ui::pickReplay("Merge: pilih macro 1", "Pilih", [askName](const std::string& nameA) {
            NXR::Ui::pickReplay("Merge: pilih macro 2", "Pilih", [askName, nameA](const std::string& nameB) {
                NXR::Ui::showChoice(
                    "Mode Merge",
                    {"Sambung: macro 2 disambung ke macro 1", "P1 + P2: P1 dari macro 1, P2 dari macro 2"},
                    {
                        {"Sambung", [askName, nameA, nameB] { askName(nameA, nameB, MergeMode::Append); }},
                        {"P1 + P2", [askName, nameA, nameB] { askName(nameA, nameB, MergeMode::Players); }}
                    }
                );
            });
        });
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

        NXR::Ui::showPopup(NXRReplayBrowserPopup::create("Open Replays", [](const std::filesystem::path& path) { loadImported(path); }), "Open Replays");
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

namespace {
    NXR::Hack g_botSettings("nxr.bot.settings", "Bot", "", false);

    void pairRow(const std::string& leftLabel, std::function<void()> left, const std::string& rightLabel, std::function<void()> right) {
        const float width = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (NXR::Imgui::button(leftLabel, width)) NXR::Imgui::later(std::move(left));
        ImGui::SameLine();
        if (NXR::Imgui::button(rightLabel, width)) NXR::Imgui::later(std::move(right));
    }

    void drawBotPanel() {
        updateRates(ImGui::GetIO().DeltaTime);

        const int mode = currentMode();
        const int pickedMode = NXR::Imgui::choice({"Disabled", "Record", "Playback"}, mode, 3);
        if (pickedMode >= 0 && pickedMode != mode) NXR::Imgui::later([pickedMode] { selectMode(pickedMode); });

        ImGui::Spacing();
        ImGui::TextUnformatted("Replays");
        if (NXR::Imgui::button(replayLabel(), -1.f)) {
            NXR::Imgui::later([] {
                NXR::Ui::pickReplay("Select Replay", "Select", [](const std::string& name) {
                    State::get().selectedReplay = name;
                }, true);
            });
        }
        if (NXR::Imgui::button("Settings", -1.f)) {
            NXR::Imgui::later([] { NXRHackSettingsPopup::open(g_botSettings, "panel:Bot"); });
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Bot Type");
        const int type = std::clamp(NXRConfig::get().get<int>("nxr.bot.type", 1), 1, 3) - 1;
        const int pickedType = NXR::Imgui::choice({"Auto", "Hold", "Click"}, type, 3);
        if (pickedType >= 0 && pickedType != type) {
            NXRConfig::get().set<int>("nxr.bot.type", pickedType + 1);
            State::get().clickCredits = 0;
        }

        auto& form = NXR::Imgui::form();
        form.addConfigToggle("Also Save As JSON", kSaveJsonKey, false);
        form.addConfigFloatInput("Speed (0.1 - 10000)", kSpeedKey, 0.1f, 10000.f, kDefaultSpeed);
        form.addConfigFloatInput("Frame Step (0.1 - 10)", "nxr.bot.frame_step", 0.1f, 10.f, 1.f);

        ImGui::Spacing();
        ImGui::TextWrapped("%s", infoText().c_str());
        ImGui::TextWrapped("%s", rateText().c_str());
        ImGui::Spacing();

        pairRow("New", [] { startNewFlow(); }, "Save", [] { saveReplay(); });

        pairRow("Load", [] {
            NXR::Ui::pickReplay("Load Replay", "Load", [](const std::string& name) { loadReplayByName(name); });
        }, "Delete", [] {
            NXR::Ui::pickReplay("Delete Replay", "Delete", [](const std::string& name) { deleteReplayByName(name); });
        });

        pairRow("Rename", [] { startRenameFlow(); }, "Restore Autosave", [] { restoreAutosave(); });

        pairRow("Browse Replays", [] { openBrowser(); }, "Merge Replays", [] { startMergeFlow(); });

        if (NXR::Imgui::button("Export JSON", -1.f)) {
            NXR::Imgui::later([] {
                NXR::Ui::pickReplay("Export JSON", "Export", [](const std::string& name) { exportReplayJson(name); });
            });
        }
    }
}

$execute {
    cleanupOrphanTemps();

    auto& win = NXR::Gui::get().getWindow("Bot");

    g_botSettings.setForm([](NXR::Form& form) {
        auto* popup = &form;
        popup->addConfigToggle("Record Without Restart", kRecordHereKey, false);
        popup->addConfigToggle("Ignore Inputs", kIgnoreKey, true, [](bool value) { State::get().ignoreInput = value; });
        popup->addConfigToggle("Practice Fixes", kPracticeFixKey, true);
        popup->addConfigToggle("Fix Random", kFixRandomKey, true);
        popup->addConfigToggle("CBF Bypass", kCbfKey, true);
        popup->addConfigToggle("Playback Death", kPlaybackDeathKey, true);
        popup->addConfigToggle("Desync Rescue", kDesyncRescueKey, true);
        popup->addConfigToggle("Wave Trail Fix", "nxr.bot.wave_trail_fix", true);
    });

    win.createHack("Click Indicator", "Shows where each click happens. It works during playback and also while you play manually: just pick a replay and turn this on. It plays a click sound at every click (res/click_indicator.mp3). Zone Follow marks every click as a box in the level with a marker at the height you need to be. Line draws the recorded path ahead with every click marked on it. Both can show Perfect. The Mirror switch adds a simple lane on the left that shows the upcoming clicks", false);
    win.createHack("Autosave", "Saves the recording in the background every few seconds and when the level is completed, and keeps timestamped backups", false);

    auto& indicator = win.findHackByName("Click Indicator");
    auto& autosave = win.findHackByName("Autosave");

    {
        auto& config = NXRConfig::get();
        const bool neverSet = config.get<bool>(kAutosaveKey, false) != config.get<bool>(kAutosaveKey, true);
        if (neverSet) config.set<bool>(kAutosaveKey, true);
    }

    indicator.setForm([](NXR::Form& form) {
        auto* popup = &form;
        popup->addConfigRadio("Effect", kIndEffectKey, {{"Zone Follow", 1}, {"Line", 2}}, 1);
        popup->addSeparator();
        popup->addConfigToggle("Mirror (Left Ramp)", kIndHudKey, true);
        popup->addConfigToggle("Ramp Info Text", kIndHudInfoKey, true);
        popup->addConfigToggle("Hide FPS/CPS Counter", kIndHideStatsKey, false);
        popup->addConfigFloatInput("Ramp Length (s)", kIndHudSecondsKey, 0.3f, 4.f, 1.2f);
        popup->addConfigFloatInput("Ramp X", kIndHudXKey, 10.f, 300.f, 70.f);
        popup->addConfigFloatInput("Mirror Size", kIndHudScaleKey, 0.5f, 3.f, 1.7f);
        popup->addConfigToggle("Player Line And Marker", kIndPlayerLineKey, true);
        popup->addConfigColor("Player 2 Color", kIndP2ColorKey, "C84DFF");
        popup->addSeparator();
        popup->addConfigColor("Line Color", kIndTrailColorKey, "39FF6E");
        popup->addConfigIntInput("Line Length (frames)", kIndTrailAheadKey, 30, 2400, 300);
        popup->addConfigFloatInput("Line Width", kIndTrailWidthKey, 0.5f, 12.f, 2.5f);
        popup->addSeparator();
        popup->addSeparator();
        popup->addConfigColor("Line Color", kIndLineKey, "FFFFFF");
        popup->addConfigColor("Background Color", kIndBodyKey, "00F0FF");
        popup->addConfigIntInput("Background Opacity", kIndBodyOpacityKey, 0, 255, 70);
        popup->addConfigFloatInput("Fade Time (s)", kIndFadeKey, 0.05f, 3.f, 0.8f);
        popup->addConfigFloatInput("Slide Distance", kIndSlideKey, 0.f, 400.f, 0.f);
        popup->addSeparator();
        popup->addConfigToggle("Show Perfect", kIndPerfectKey, true);
        popup->addConfigColor("Perfect Color", kIndPerfectColorKey, "39FF6E");
        popup->addSeparator();
        popup->addConfigToggle("Click Sound", kIndSoundKey, true);
        popup->addConfigIntInput("Sound Volume", kIndSoundVolumeKey, 0, 100, 80);
    });

    autosave.setForm([](NXR::Form& form) {
        auto* popup = &form;
        popup->addConfigToggle("Save On Win", kAutosaveWinKey, true);
        popup->addConfigIntInput("Save Every (s)", kAutosaveSecondsKey, 5, 600, 20);
        popup->addConfigIntInput("Keep Backups", kAutosaveKeepKey, 1, 100, 10);
    });

    win.setImguiPanel(drawBotPanel);

    win.setCustomWindowCocos([](NXR::Kit::PageBuilder& page) {
        page.addPadding(2.f);
        page.addRadioRow({"Disabled", "Record", "Playback"}, [] { return currentMode(); }, [](int mode) { return selectMode(mode); });
        page.addSelector("Replays", [] { return replayLabel(); }, [](std::function<void()> refresh) {
            NXR::Ui::showPopup(NXRReplayPickerPopup::create("Select Replay", "Select", [refresh](const std::string& name) {
                State::get().selectedReplay = name;
                refresh();
            }, true), "Select Replay");
        });
        page.addButtons({{"Bot Settings", [] { NXRHackSettingsPopup::open(g_botSettings, "panel:Bot"); }}});
        page.addSection("Bot Type");
        page.addRadioRow({"Auto", "Hold", "Click"},
            [] { return std::clamp(NXRConfig::get().get<int>("nxr.bot.type", 1), 1, 3) - 1; },
            [](int index) {
                NXRConfig::get().set<int>("nxr.bot.type", index + 1);
                State::get().clickCredits = 0;
                return index;
            });
        page.addConfigToggle("Also Save As JSON", kSaveJsonKey, false);
        page.addConfigFloatInput("Speed (0.1 - 10000)", kSpeedKey, 0.1f, 10000.f, kDefaultSpeed);
        page.addConfigFloatInput("Frame Step (0.1 - 10)", "nxr.bot.frame_step", 0.1f, 10.f, 1.f);
        page.addNode(BotInfoNode::create(), 30.f);
        page.addNode(BotInfoNode::create(1), 30.f);

        page.addButtons({{"New", [] { startNewFlow(); }}, {"Save", [] { saveReplay(); }}});
        page.addButtons({
            {"Load", [] {
                NXR::Ui::showPopup(NXRReplayPickerPopup::create("Load Replay", "Load", [](const std::string& name) { loadReplayByName(name); }), "Load Replay");
            }},
            {"Delete", [] {
                NXR::Ui::showPopup(NXRReplayPickerPopup::create("Delete Replay", "Delete", [](const std::string& name) { deleteReplayByName(name); }), "Delete Replay");
            }}
        });
        page.addButtons({{"Rename", [] { startRenameFlow(); }}, {"Restore Autosave", [] { restoreAutosave(); }}});
        page.addButtons({{"Browse Replays", [] { openBrowser(); }}, {"Merge Replays", [] { startMergeFlow(); }}});
        page.addButtons({{"Export JSON", [] {
            NXR::Ui::showPopup(NXRReplayPickerPopup::create("Export JSON", "Export", [](const std::string& name) { exportReplayJson(name); }), "Export JSON");
        }}});
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

class $modify(NXRBotLevelNamePlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        if (!m_level) return;
        g_lastLevel = m_level->m_levelName;
        NXRConfig::get().set<std::string>("nxr.bot.last_level", g_lastLevel);
    }
};
