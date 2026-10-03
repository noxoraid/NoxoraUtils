#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJGameLevel.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"
#include "../../core/nxr_config.hpp"

NXR_HACK_CREATE(
    "Global", "Safe Mode",
    "Wins, progress and best percent are not counted or saved while this is on",
    false
);

namespace {
    bool safeOn() {
        return NXRConfig::get().get<bool>("nxr.global.safe_mode", false);
    }
}

class $modify(NXRSafeModePlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Global").findHackByName("Safe Mode");
        NXR::tryAddHook(self, hack, "PlayLayer::levelComplete");
    }

    void levelComplete() {
        if (!safeOn()) {
            PlayLayer::levelComplete();
            return;
        }

        const bool previous = m_isTestMode;
        m_isTestMode = true;
        PlayLayer::levelComplete();
        m_isTestMode = previous;
    }
};

class $modify(NXRSafeModeLevel, GJGameLevel) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Global").findHackByName("Safe Mode");
        NXR::tryAddHook(self, hack, "GJGameLevel::savePercentage");
    }

    void savePercentage(int percent, bool isPracticeMode, int clicks, int attempts, bool isChkValid) {
        if (safeOn()) return;
        GJGameLevel::savePercentage(percent, isPracticeMode, clicks, attempts, isChkValid);
    }
};
