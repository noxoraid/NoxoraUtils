#include <Geode/Geode.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <algorithm>
#include <cmath>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_config.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"

NXR_HACK_CREATE(
    "Global", "Speedhack",
    "Changes the game speed from 0.01 to 1000. Works on gameplay and audio. The mod menu is not affected",
    true
);

NXR_HACK_CREATE(
    "Global", "Auto Sync Music",
    "Makes the music follow the gameplay. If the game lags or runs slower, the music slows down with it instead of running ahead",
    false
);

namespace {
    constexpr const char* kSpeedOn = "nxr.global.speedhack";
    constexpr const char* kSpeedValue = "nxr.global.speedhack::speed";
    constexpr const char* kSpeedAudio = "nxr.global.speedhack::audio";
    constexpr const char* kSyncOn = "nxr.global.auto_sync_music";

    double g_realDt = 0.0;
    double g_sync = 1.0;
    double g_accAdvance = 0.0;
    double g_accReal = 0.0;
    double g_baseOffset = 0.0;
    bool g_hasBase = false;
    double g_drift = 0.0;
    float g_lastPitch = 1.f;
    bool g_pitchDirty = false;

    bool speedOn() { return NXRConfig::get().get<bool>(kSpeedOn, false); }
    bool syncOn() { return NXRConfig::get().get<bool>(kSyncOn, false); }

    double speedValue() {
        return std::clamp(static_cast<double>(NXRConfig::get().get<float>(kSpeedValue, 1.f)), 0.01, 1000.0);
    }

    bool gameplayActive() {
        auto* pl = PlayLayer::get();
        return pl && !pl->m_isPaused && !pl->m_levelEndAnimationStarted;
    }

    float g_applied = 1.f;

    void applyPitch(float pitch) {
        pitch = std::clamp(pitch, 0.01f, 10.f);

        auto* engine = FMODAudioEngine::sharedEngine();
        if (!engine || !engine->m_backgroundMusicChannel) return;

        auto* group = engine->m_backgroundMusicChannel;

        float current = 1.f;
        const bool known = group->getPitch(&current) == FMOD_OK;
        const bool changed = !known || std::fabs(current - pitch) > 0.002f;

        if (changed || g_pitchDirty) {
            group->setPitch(pitch);
            g_applied = pitch;
        }

        g_lastPitch = pitch;
        g_pitchDirty = false;
    }

    float targetPitch() {
        if (!gameplayActive()) return 1.f;

        const bool audio = NXRConfig::get().get<bool>(kSpeedAudio, true);

        if (syncOn()) {
            const double drift = std::clamp(g_drift / 1000.0, -0.1, 0.1);
            return static_cast<float>(std::clamp(g_sync * (1.0 - drift), 0.01, 10.0));
        }

        if (speedOn() && audio) return static_cast<float>(speedValue());
        return 1.f;
    }
}

class $modify(NXRSpeedScheduler, cocos2d::CCScheduler) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Global").findHackByName("Speedhack");

        hack.setForm([
            speed = hack.formatAdditionalSetting("speed"),
            audio = hack.formatAdditionalSetting("audio")
        ](NXR::Form& form) {
            form.addConfigSlider("Speed", speed, 0.01f, 1000.f, 1.f, 0.01f, NXR::SliderScale::Log, {
                {"0.1x", 0.1f},
                {"0.5x", 0.5f},
                {"1x", 1.f},
                {"2x", 2.f},
                {"10x", 10.f},
                {"100x", 100.f}
            }, nullptr, false, "x");
            form.addConfigToggle("Affect Audio", audio, true);
        });

        hack.setSummary([speed = hack.formatAdditionalSetting("speed")]() {
            const float value = NXRConfig::get().get<float>(speed, 1.f);
            return value >= 100.f ? fmt::format("{:.0f}x", value) : fmt::format("{:.2f}x", value);
        });

        auto& sync = NXR::Gui::get().getWindow("Global").findHackByName("Auto Sync Music");
        sync.setHandler([](bool) {
            g_sync = 1.0;
            g_accAdvance = 0.0;
            g_accReal = 0.0;
            g_hasBase = false;
            g_drift = 0.0;
            g_pitchDirty = true;
        });
    }

    void update(float dt) {
        g_realDt = dt;

        float scaled = dt;
        if (speedOn() && gameplayActive()) {
            scaled = static_cast<float>(static_cast<double>(dt) * speedValue());
        }

        CCScheduler::update(scaled);

        if (speedOn() || syncOn() || std::fabs(g_lastPitch - 1.f) > 0.0005f) {
            applyPitch(targetPitch());
        }
    }
};

class $modify(NXRSyncBaseGameLayer, GJBaseGameLayer) {
    void update(float dt) {
        const double before = m_gameState.m_levelTime;
        GJBaseGameLayer::update(dt);

        if (!syncOn()) return;

        auto* pl = PlayLayer::get();
        if (!pl || static_cast<GJBaseGameLayer*>(pl) != this) return;

        if (m_gameState.m_timeWarp != 1.f || g_realDt <= 0.0005 || !gameplayActive()) {
            g_sync = 1.0;
            g_accAdvance = 0.0;
            g_accReal = 0.0;
            g_hasBase = false;
            g_drift = 0.0;
            return;
        }

        const double advanced = m_gameState.m_levelTime - before;

        if (advanced < -0.0001 || advanced > 1.0) {
            g_accAdvance = 0.0;
            g_accReal = 0.0;
            g_hasBase = false;
            g_drift = 0.0;
            return;
        }

        g_accAdvance += advanced;
        g_accReal += g_realDt;

        if (g_accReal < 0.25) return;

        double ratio = g_accAdvance / g_accReal;
        g_accAdvance = 0.0;
        g_accReal = 0.0;

        const double reference = speedOn() ? speedValue() : 1.0;
        if (std::fabs(ratio - reference) < reference * 0.04) ratio = reference;

        const double smoothed = g_sync * 0.7 + ratio * 0.3;
        g_sync = std::fabs(smoothed - g_sync) < 0.01 * g_sync ? g_sync : smoothed;

        auto* engine = FMODAudioEngine::sharedEngine();
        if (!engine) return;

        const double levelMs = m_gameState.m_levelTime * 1000.0;
        const double offset = static_cast<double>(engine->getMusicTimeMS(0)) - levelMs;

        if (!g_hasBase) {
            g_baseOffset = offset;
            g_hasBase = true;
            g_drift = 0.0;
            return;
        }

        const double drift = offset - g_baseOffset;
        g_drift = std::fabs(drift) < 40.0 ? 0.0 : drift;
    }
};
