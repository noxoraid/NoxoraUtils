#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"

using namespace geode::prelude;

NXR_HACK_CREATE("Creator", "Zoom Bypass", "Lets you zoom an infinite amount in the editor", false);

class $modify(NXRZoomBypassEditorUI, EditorUI) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Creator").findHackByName("Zoom Bypass");

        NXR::trySetPriority(self, "EditorUI::zoomGameLayer", geode::Priority::Early);
        NXR::tryAddHook(self, hack, "EditorUI::zoomGameLayer");
    }

    void zoomBypass(bool in) {
        float scale = m_editorLayer->m_groundLayer->getScale();
        float step = (!in && scale <= 0.105f) || (in && scale < 0.095f) ? 0.01f : 0.1f;

        scale += in ? step : -step;
        this->updateZoom(std::max(scale, 0.01f));
    }

    void zoomGameLayer(bool zoomingIn) {
        zoomBypass(zoomingIn);
    }
};
