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
    double g_hardCooldown = 0.0;
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

    void setGroupPitch(FMOD::ChannelGroup* group, float pitch) {
        if (!group) return;
        float current = 1.f;
        if (group->getPitch(&current) == FMOD_OK && std::fabs(current - pitch) <= 0.0005f && !g_pitchDirty) return;
        group->setPitch(pitch);
    }

    void applyPitch(float pitch) {
        pitch = std::clamp(pitch, 0.01f, 10.f);

        auto* engine = FMODAudioEngine::sharedEngine();
        if (!engine) return;

        setGroupPitch(engine->m_backgroundMusicChannel, pitch);
        setGroupPitch(engine->m_globalChannel, pitch);

        g_lastPitch = pitch;
        g_pitchDirty = false;
    }

    float targetPitch() {
        const bool active = gameplayActive();
        const bool audio = NXRConfig::get().get<bool>(kSpeedAudio, true);

        if (!active) return 1.f;

        if (syncOn()) {
            const double correction = std::clamp(g_drift / 1000.0, -0.25, 0.25);
            return static_cast<float>(std::clamp(g_sync * (1.0 - correction * 2.0), 0.01, 10.0));
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
            form.addConfigFloatInput("Speed (0.01 - 1000)", speed, 0.01f, 1000.f, 1.f);
            form.addConfigToggle("Affect Audio", audio, true);
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

        if (m_gameState.m_timeWarp != 1.f || g_realDt <= 0.0005 || !gameplayActive()) {
            g_sync = 1.0;
            g_accAdvance = 0.0;
            g_accReal = 0.0;
            g_hasBase = false;
            g_drift = 0.0;
            return;
        }

        const double now = m_gameState.m_levelTime;
        const double advanced = now - before;

        if (advanced < -0.0001 || advanced > 1.0) {
            g_accAdvance = 0.0;
            g_accReal = 0.0;
            g_hasBase = false;
            g_drift = 0.0;
            return;
        }

        g_accAdvance += advanced;
        g_accReal += g_realDt;

        if (g_accReal >= 0.12) {
            double ratio = g_accAdvance / g_accReal;
            g_accAdvance = 0.0;
            g_accReal = 0.0;

            const double reference = speedOn() ? speedValue() : 1.0;
            if (std::fabs(ratio - reference) < reference * 0.02) ratio = reference;

            g_sync = g_sync * 0.5 + ratio * 0.5;
        }

        auto* engine = FMODAudioEngine::sharedEngine();
        if (!engine || now < 0.4) return;

        const double levelMs = now * 1000.0;
        const double musicMs = static_cast<double>(engine->getMusicTimeMS(0));
        const double offset = musicMs - levelMs;

        if (!g_hasBase) {
            g_baseOffset = offset;
            g_hasBase = true;
            g_drift = 0.0;
            return;
        }

        g_drift = offset - g_baseOffset;
        g_hardCooldown -= g_realDt;

        if (std::fabs(g_drift) > 250.0 && g_hardCooldown <= 0.0) {
            const double target = levelMs + g_baseOffset;
            if (target > 0.0) engine->setMusicTimeMS(static_cast<unsigned int>(target), true, 0);
            g_hardCooldown = 1.0;
            g_drift = 0.0;
        }
    }
};
