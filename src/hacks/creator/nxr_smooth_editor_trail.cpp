#include <Geode/Geode.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"

using namespace geode::prelude;

NXR_HACK_CREATE("Creator", "Smooth Editor Trail", "Makes the wave trail smoother in the editor", false);

class $modify(NXRSmoothTrailLevelEditorLayer, LevelEditorLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Creator").findHackByName("Smooth Editor Trail");

        NXR::tryAddHook(self, hack, "LevelEditorLayer::postUpdate");
    }

    void postUpdate(float dt) {
        m_trailTimer = 0.1f;
        LevelEditorLayer::postUpdate(dt);
    }
};
