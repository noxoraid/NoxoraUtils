#include <Geode/Geode.hpp>
#include <algorithm>
#include <cmath>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#ifdef GEODE_IS_ANDROID
#include <dlfcn.h>
#endif
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"
#include "../../core/nxr_config.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"

NXR_HACK_CREATE(
    "Player", "FPS Limiter",
    "Sets the real frame rate and the physics tick rate (TPS). FPS 0 = uncapped. TPS 240 is the game default, other values change how the physics behave",
    false
);

namespace {
    constexpr const char* kEnabledKey = "nxr.player.fps_limiter";
    constexpr const char* kFpsKey = "nxr.player.fps_limiter::fps";
    constexpr const char* kTpsKey = "nxr.player.fps_limiter::tps";

    double g_originalInterval = 0.0;
    double g_extraDelta = 0.0;
    bool g_tpsHookMissing = false;
    bool g_tpsWarned = false;

    void setVsync(bool on) {
#ifdef GEODE_IS_ANDROID
        using GetDisplayFn = void* (*)();
        using SwapIntervalFn = unsigned (*)(void*, int);
        static auto getDisplay = reinterpret_cast<GetDisplayFn>(dlsym(RTLD_DEFAULT, "eglGetCurrentDisplay"));
        static auto swapInterval = reinterpret_cast<SwapIntervalFn>(dlsym(RTLD_DEFAULT, "eglSwapInterval"));
        if (!getDisplay || !swapInterval) return;
        void* display = getDisplay();
        if (!display) return;
        swapInterval(display, on ? 1 : 0);
#else
        (void)on;
#endif
    }

    void warnIfTpsUnavailable() {
        if (!g_tpsHookMissing || g_tpsWarned) return;
        g_tpsWarned = true;
        geode::Notification::create("TPS hook is not available on this build, only FPS is applied", geode::NotificationIcon::Warning)->show();
    }

    int wantedFps() { return std::clamp(NXRConfig::get().get<int>(kFpsKey, 240), 0, 5000000); }
    int wantedTps() { return std::clamp(NXRConfig::get().get<int>(kTpsKey, 240), 1, 5000000); }

    double intervalFor(int fps) {
        return fps <= 0 ? (1.0 / 5000000.0) : (1.0 / static_cast<double>(fps));
    }

    double realProgress(GJBaseGameLayer* layer) {
        if (!layer || !layer->m_level) return 0.0;

        const auto timestamp = layer->m_level->m_timestamp;
        double percent = 0.0;

        if (timestamp > 0) {
            percent = layer->m_gameState.m_levelTime * 240.0 / timestamp * 100.0;
        } else if (layer->m_levelLength > 0.0f && layer->m_player1) {
            percent = layer->m_player1->getPositionX() * 100.0 / layer->m_levelLength;
        }

        if (std::isnan(percent) || std::isinf(percent)) return 0.0;
        return std::clamp(percent, 0.0, 100.0);
    }

    void applyFrameRate() {
        auto* director = cocos2d::CCDirector::sharedDirector();
        if (!director) return;

        if (g_originalInterval <= 0.0) {
            g_originalInterval = director->getAnimationInterval();
            if (g_originalInterval <= 0.0) g_originalInterval = 1.0 / 60.0;
        }

        bool enabled = NXRConfig::get().get<bool>(kEnabledKey, false);

        if (enabled && wantedFps() > 0) {
            if (auto* manager = GameManager::get()) {
                manager->m_customFPSTarget = std::clamp(static_cast<float>(wantedFps()), 10.f, 10000.f);
                manager->setGameVariable("0116", true);
            }
        }

        setVsync(!enabled);
        director->setAnimationInterval(static_cast<float>(enabled ? intervalFor(wantedFps()) : g_originalInterval));
        if (enabled) warnIfTpsUnavailable();
    }
}

class $modify(NXRFpsLimiterCCDirector, cocos2d::CCDirector) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Player").findHackByName("FPS Limiter");
        NXR::tryAddHook(self, hack, "CCDirector::setAnimationInterval");

        hack.setHandler([](bool) {
            g_extraDelta = 0.0;
            geode::queueInMainThread([] { applyFrameRate(); });
        });

        hack.setForm([
            fpsKey = hack.formatAdditionalSetting("fps"),
            tpsKey = hack.formatAdditionalSetting("tps")
        ](NXR::Form& form) {
            auto* popup = &form;
            popup->addConfigIntInput("Target FPS (0 = uncapped)", fpsKey, 0, 5000000, 240, [](int) {
                applyFrameRate();
            });
            popup->addSeparator();
            popup->addConfigIntInput("Physics TPS (240 = default)", tpsKey, 1, 5000000, 240, [](int) {
                g_extraDelta = 0.0;
            });
        });
    }

    void setAnimationInterval(float interval) {
        if (!NXRConfig::get().get<bool>(kEnabledKey, false)) {
            g_originalInterval = interval;
            CCDirector::setAnimationInterval(interval);
            return;
        }

        CCDirector::setAnimationInterval(static_cast<float>(intervalFor(wantedFps())));
    }
};

class $modify(NXRTpsGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Player").findHackByName("FPS Limiter");
        if (!NXR::tryAddHook(self, hack, "GJBaseGameLayer::getModifiedDelta")) g_tpsHookMissing = true;
    }

    double getModifiedDelta(float dt) {
        const int tps = wantedTps();
        if (tps == 240) return GJBaseGameLayer::getModifiedDelta(dt);

        if (m_resumeTimer > 0) {
            --m_resumeTimer;
            dt = 0.f;
        }

        const double fixedDt = 1.0 / static_cast<double>(tps);
        const double timestep = std::min(static_cast<double>(m_gameState.m_timeWarp), 1.0) * fixedDt;
        if (timestep <= 0.0) return 0.0;

        const double total = static_cast<double>(dt) + m_extraDelta;
        const double steps = std::min(std::round(total / timestep), 20000.0);
        const double newDt = steps * timestep;

        m_extraDelta = total - newDt;
        return newDt;
    }
};

class $modify(NXRTpsPlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Player").findHackByName("FPS Limiter");
        NXR::tryAddHook(self, hack, "PlayLayer::setupHasCompleted");
        NXR::tryAddHook(self, hack, "PlayLayer::resume");
        NXR::tryAddHook(self, hack, "PlayLayer::updateProgressbar");
        NXR::tryAddHook(self, hack, "PlayLayer::destroyPlayer");
        NXR::tryAddHook(self, hack, "PlayLayer::levelComplete");
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        g_extraDelta = 0.0;
        m_extraDelta = 0.0;
        applyFrameRate();
    }

    void resume() {
        PlayLayer::resume();
        applyFrameRate();
    }

    int recalculateProgress() {
        const int current = m_gameState.m_currentProgress;

        if (wantedTps() != 240 && m_level && m_level->m_timestamp > 0) {
            m_gameState.m_currentProgress = static_cast<int>(m_level->m_timestamp * realProgress(this) * 0.02);
        }
        return current;
    }

    void updateProgressbar() {
        const int previous = recalculateProgress();
        PlayLayer::updateProgressbar();
        m_gameState.m_currentProgress = previous;
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        const int previous = recalculateProgress();
        PlayLayer::destroyPlayer(player, object);
        m_gameState.m_currentProgress = previous;
    }

    void levelComplete() {
        const auto previous = m_gameState.m_commandIndex;

        if (wantedTps() != 240) {
            m_gameState.m_commandIndex = static_cast<decltype(m_gameState.m_commandIndex)>(std::round(m_gameState.m_levelTime * 480.0));
        }

        PlayLayer::levelComplete();
        m_gameState.m_commandIndex = previous;
    }
};
