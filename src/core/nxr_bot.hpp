#pragma once
#include <Geode/Geode.hpp>
#include <vector>
#include <string>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <array>
#include <algorithm>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "nxr_player_state.hpp"
#include "nxr_level_stats.hpp"

namespace NXR::Bot {

    void resetControls();
    void syncControls();
    void resetAttemptControls();
    void indicatorUpdate();
    void indicatorReset(uint64_t frame);
    void indicatorClear();
    void indicatorManualTick(bool counted);
    void botSessionBegin();
    void botSessionEnd();
    void botReanchor();

    enum class Mode : int {
        Off = 0,
        Recording = 1,
        Playing = 2,
    };

    struct InputEvent {
        static constexpr uint32_t kMaxFrame = (1u << 28) - 1;

        uint32_t packed = 0;

        static InputEvent make(uint64_t frame, uint8_t player, uint8_t button, bool down) {
            InputEvent ev;
            ev.packed = (static_cast<uint32_t>(frame) << 4)
                | (player == 2 ? 8u : 0u)
                | ((static_cast<uint32_t>(button) & 3u) << 1)
                | (down ? 1u : 0u);
            return ev;
        }

        uint64_t frame() const { return packed >> 4; }
        uint8_t player() const { return (packed & 8u) ? 2 : 1; }
        uint8_t button() const { return static_cast<uint8_t>((packed >> 1) & 3u); }
        bool down() const { return (packed & 1u) != 0; }
    };

    struct MacroFrame {
        uint32_t frame = 0;
        NXR::Capture::PlayerState p1;
        NXR::Capture::PlayerState p2;
        uint8_t hold = 0;
        bool full = true;
    };

    struct SuperFrame {
        uint32_t frame = 0;
        std::vector<uint8_t> p1;
        std::vector<uint8_t> p2;
    };

    inline uint8_t holdBit(int slot, int button) {
        if (slot < 0 || slot > 1 || button < 1 || button > 3) return 0;
        return static_cast<uint8_t>(1u << (slot * 3 + (button - 1)));
    }

    struct Macro {
        std::string name;
        std::string levelName;
        int32_t levelId = 0;
        std::string version;
        float fps = 0.f;
        float tps = 240.f;
        uint64_t totalFrames = 0;
        uint32_t layout = 0;
        bool noclip = false;
        uint64_t levelHash = 0;
        bool hasStats = false;
        NXR::Stats::LevelStats stats;
        std::vector<InputEvent> events;
        std::vector<MacroFrame> frames;
        std::vector<SuperFrame> supers;

        void clear() {
            name.clear();
            levelName.clear();
            levelId = 0;
            version.clear();
            fps = 0.f;
            tps = 240.f;
            totalFrames = 0;
            layout = 0;
            noclip = false;
            levelHash = 0;
            hasStats = false;
            stats = {};
            events.clear();
            frames.clear();
            supers.clear();
        }

        uint64_t endFrame() const {
            uint64_t end = totalFrames;
            if (!events.empty()) end = std::max<uint64_t>(end, events.back().frame());
            if (!frames.empty()) end = std::max<uint64_t>(end, frames.back().frame);
            return end;
        }

        const MacroFrame* rowAt(uint64_t frame) const {
            auto it = std::partition_point(frames.begin(), frames.end(), [frame](const MacroFrame& row) {
                return static_cast<uint64_t>(row.frame) < frame;
            });
            if (it == frames.end() || static_cast<uint64_t>(it->frame) != frame) return nullptr;
            return &*it;
        }

        const SuperFrame* superAt(uint64_t frame) const {
            auto it = std::partition_point(supers.begin(), supers.end(), [frame](const SuperFrame& sf) {
                return static_cast<uint64_t>(sf.frame) < frame;
            });
            if (it == supers.end() || static_cast<uint64_t>(it->frame) != frame) return nullptr;
            return &*it;
        }

        void rebuildHold() {
            std::array<std::array<bool, 4>, 2> held{};
            size_t next = 0;

            for (auto& row : frames) {
                while (next < events.size() && events[next].frame() <= row.frame) {
                    const auto& ev = events[next++];
                    const int button = ev.button();
                    if (button >= 1 && button <= 3) held[ev.player() == 2 ? 1 : 0][button] = ev.down();
                }

                uint8_t mask = 0;
                for (int slot = 0; slot < 2; slot++) {
                    for (int button = 1; button <= 3; button++) {
                        if (held[slot][button]) mask |= holdBit(slot, button);
                    }
                }
                row.hold = mask;
            }
        }

        void finalize(bool recomputeHold) {
            std::stable_sort(events.begin(), events.end(),
                [](const InputEvent& a, const InputEvent& b) { return a.frame() < b.frame(); });
            std::stable_sort(frames.begin(), frames.end(),
                [](const MacroFrame& a, const MacroFrame& b) { return a.frame < b.frame; });
            std::stable_sort(supers.begin(), supers.end(),
                [](const SuperFrame& a, const SuperFrame& b) { return a.frame < b.frame; });

            std::reverse(frames.begin(), frames.end());
            frames.erase(std::unique(frames.begin(), frames.end(),
                [](const MacroFrame& a, const MacroFrame& b) { return a.frame == b.frame; }), frames.end());
            std::reverse(frames.begin(), frames.end());

            std::reverse(supers.begin(), supers.end());
            supers.erase(std::unique(supers.begin(), supers.end(),
                [](const SuperFrame& a, const SuperFrame& b) { return a.frame == b.frame; }), supers.end());
            std::reverse(supers.begin(), supers.end());

            if (recomputeHold) rebuildHold();
            totalFrames = std::max(totalFrames, endFrame());
        }
    };

    float effectiveTps();

    bool saveMacro(const Macro& macro, const std::filesystem::path& path);
    bool loadMacro(Macro& macro, const std::filesystem::path& path);
    bool loadMacroLegacy(Macro& macro, const std::filesystem::path& path);
    bool saveMacroJson(const Macro& macro, const std::filesystem::path& path);

    std::vector<std::string> listMacros();
    bool deleteMacro(const std::string& name);
    std::filesystem::path macroPathFor(const std::string& name);
    std::filesystem::path macroJsonPathFor(const std::string& name);

    void saveMacroAsync(Macro macro, std::filesystem::path path);
    void flushAsyncSaves();
    bool hasBackup(const std::string& name);
    bool restoreBackup(const std::string& name);
    std::string backupNameNow(const std::string& reason);
    void pruneBackups(size_t keep);
    void cleanupOrphanTemps();

    enum class MergeMode : int {
        Append = 0,
        Players = 1,
    };

    struct MergeReport {
        uint64_t cutFrame = 0;
        int64_t shift = 0;
        size_t events = 0;
        size_t frames = 0;
        std::string note;
    };

    bool mergeMacros(const Macro& a, const Macro& b, MergeMode mode, Macro& out, MergeReport& report, std::string& error);

    class State {
    public:
        static State& get() {
            static State instance;
            return instance;
        }

        State(const State&) = delete;
        State& operator=(const State&) = delete;

        Mode mode = Mode::Off;
        Macro current;
        uint64_t frame = 0;
        size_t playIndex = 0;
        bool ignoreInput = false;
        std::string selectedReplay;

        std::unordered_map<const void*, uint64_t> checkpointFrames;

        bool frozen = false;
        int stepRequests = 0;
        bool slowActive = false;
        uint64_t ticks = 0;
        int stepParticles = 0;

        bool waiting = false;
        bool prevTouch = false;
        int clickCredits = 0;
        size_t lastPlayIndex = 0;
        std::array<std::array<bool, 8>, 2> held{};
        bool levelWasEnding = false;
        uint64_t lastAutosaveFrame = 0;
        bool autosaveDirty = false;
        bool pendingRestart = false;
        bool pendingHere = false;

        uint32_t rescues = 0;
        uint64_t firstRescueFrame = 0;
        uint64_t lastRescueFrame = 0;

        void resetRescues() {
            rescues = 0;
            firstRescueFrame = 0;
            lastRescueFrame = 0;
        }

        void resetRun() {
            frame = 0;
            playIndex = 0;
            held = {};
            levelWasEnding = false;
            lastAutosaveFrame = 0;
        }

        void startRecording() {
            const std::string keepName = selectedReplay;
            mode = Mode::Recording;
            pendingRestart = true;
            pendingHere = false;
            botSessionBegin();
            indicatorClear();
            current.clear();
            current.name = keepName;
            checkpointFrames.clear();
            resetRun();
        }

        void startRecordingHere() {
            const std::string keepName = selectedReplay;
            mode = Mode::Recording;
            pendingRestart = false;
            pendingHere = true;
            botSessionBegin();
            indicatorClear();
            current.clear();
            current.name = keepName;
            checkpointFrames.clear();
            resetRun();
            botReanchor();
        }

        void startPlaying(Macro macro) {
            mode = Mode::Playing;
            pendingRestart = true;
            pendingHere = false;
            botSessionBegin();
            indicatorClear();
            current = std::move(macro);
            checkpointFrames.clear();
            resetRescues();
            resetRun();
        }

        void stop() {
            mode = Mode::Off;
            pendingRestart = false;
            pendingHere = false;
            botSessionEnd();
            indicatorClear();
            checkpointFrames.clear();
            resetRun();
            resetControls();
        }

    private:
        State() = default;
    };
}
