#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <cmath>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_keybinds.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"

NXR_HACK_CREATE(
    "Global", "Super Fast Practice Click",
    "In Practice mode, press 1 to spam checkpoints very fast. Delay 0 = every frame, higher = slower (0 - 10000 ms). Press 1 again to stop. Also has a switch in the settings for devices without a keyboard",
    false
);

namespace {
    constexpr const char* kEnabled = "nxr.global.super_fast_practice_click";
    constexpr const char* kActive = "nxr.global.super_fast_practice_click::active";
    constexpr const char* kDelay = "nxr.global.super_fast_practice_click::delay";

    double g_accumulated = 0.0;

    bool enabled() { return NXRConfig::get().get<bool>(kEnabled, false); }
    bool active() { return NXRConfig::get().get<bool>(kActive, false); }
}

class $modify(NXRPracticeClickPlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Global").findHackByName("Super Fast Practice Click");

        hack.setForm([
            activeKey = hack.formatAdditionalSetting("active"),
            delayKey = hack.formatAdditionalSetting("delay")
        ](NXR::Form& form) {
            form.addConfigToggle("Spam Active (key 1)", activeKey, false);
            form.addConfigIntInput("Delay ms (0 - 10000)", delayKey, 0, 10000, 100);
        });

        hack.setHandler([](bool state) {
            g_accumulated = 0.0;
            if (!state) NXRConfig::get().set<bool>(kActive, false);
        });
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        if (!enabled() || !active()) return;

        if (!m_isPracticeMode) {
            NXRConfig::get().set<bool>(kActive, false);
            return;
        }

        if (m_isPaused || m_levelEndAnimationStarted || !m_player1 || m_player1->m_isDead) return;

        const int delayMs = std::clamp(NXRConfig::get().get<int>(kDelay, 100), 0, 10000);

        int count = 1;
        if (delayMs > 0) {
            g_accumulated += static_cast<double>(dt);
            const double interval = static_cast<double>(delayMs) / 1000.0;
            count = std::min(static_cast<int>(g_accumulated / interval), 50);
            if (count <= 0) return;
            g_accumulated = std::fmod(g_accumulated, interval);
        }

        for (int i = 0; i < count; i++) this->markCheckpoint();
    }
};

$execute {
    NXR::Keybinds::get().registerAction(
        "nxr.global::practice_click", "Super Fast Practice Click: Toggle",
        geode::Keybind(cocos2d::KEY_One, geode::KeyboardModifier::None),
        [](bool repeat) {
            if (repeat || !enabled()) return;
            auto* pl = PlayLayer::get();
            if (!pl || !pl->m_isPracticeMode) return;
            NXRConfig::get().set<bool>(kActive, !active());
            g_accumulated = 0.0;
        }
    );
}
