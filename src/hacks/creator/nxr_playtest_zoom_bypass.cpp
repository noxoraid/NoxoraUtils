#include <Geode/Geode.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"

using namespace geode::prelude;

NXR_HACK_CREATE("Creator", "Playtest Zoom Bypass", "Disables the automatic zoom to 1x when playtesting starts", false);

namespace {
    bool g_onPlaytest = false;
}

class $modify(NXRPlaytestZoomLevelEditorLayer, LevelEditorLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Creator").findHackByName("Playtest Zoom Bypass");

        NXR::tryAddHook(self, hack, "LevelEditorLayer::onPlaytest");
    }

    void onPlaytest() {
        g_onPlaytest = true;
        LevelEditorLayer::onPlaytest();
        g_onPlaytest = false;
    }
};

class $modify(NXRPlaytestZoomGJBaseGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Creator").findHackByName("Playtest Zoom Bypass");

        NXR::tryAddHook(self, hack, "GJBaseGameLayer::updateZoom");
    }

    void updateZoom(float zoom, float duration, int easing, float rate, int uniqueID, int controlID) {
        if (g_onPlaytest) return;
        GJBaseGameLayer::updateZoom(zoom, duration, easing, rate, uniqueID, controlID);
    }
};
