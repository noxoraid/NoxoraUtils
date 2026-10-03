#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <algorithm>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_bot.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"

NXR_HACK_CREATE(
    "Global", "Click Between Frames",
    "Built-in click between frames and steps, no other mod needed. Modes: Low, High, Extreme. Higher modes apply clicks as early as possible inside the frame",
    false
);

namespace {
    constexpr const char* kModeKey = "nxr.global.click_between_frames::mode";

    int mode() {
        return std::clamp(NXRConfig::get().get<int>(kModeKey, 1), 0, 2);
    }

    bool botBusy() {
        return NXR::Bot::State::get().mode != NXR::Bot::Mode::Off;
    }
}

class $modify(NXRClickBetweenBaseGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Global").findHackByName("Click Between Frames");

        hack.setForm([modeKey = hack.formatAdditionalSetting("mode")](NXR::Form& form) {
            form.addConfigIntInput("Mode (0 Low, 1 High, 2 Extreme)", modeKey, 0, 2, 1);
        });

        NXR::trySetPriority(self, "GJBaseGameLayer::update", -100);
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::update");
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::processQueuedButtons");
    }

    void update(float dt) {
        if (botBusy()) {
            GJBaseGameLayer::update(dt);
            return;
        }

        const bool previous = m_clickBetweenSteps;
        m_clickBetweenSteps = true;

        if (mode() >= 2 && !m_queuedButtons.empty()) {
            for (auto& command : m_queuedButtons) command.m_step = std::min(command.m_step, m_currentStep);
        }

        GJBaseGameLayer::update(dt);
        m_clickBetweenSteps = previous;
    }

    void processQueuedButtons(float dt, bool clearInputQueue) {
        if (!botBusy() && mode() >= 1) {
            for (auto& command : m_queuedButtons) command.m_step = std::min(command.m_step, m_currentStep);
        }
        GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
    }
};
