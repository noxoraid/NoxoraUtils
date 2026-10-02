#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"

using namespace geode::prelude;

NXR_HACK_CREATE("Creator", "Free Scroll", "Allows scrolling out of the editor", false);

class $modify(NXRFreeScrollEditorUI, EditorUI) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Creator").findHackByName("Free Scroll");

        NXR::tryAddHook(self, hack, "EditorUI::constrainGameLayerPosition");
    }

    void constrainGameLayerPosition(float width, float height) {}
};
