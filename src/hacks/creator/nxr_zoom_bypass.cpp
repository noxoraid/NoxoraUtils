#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"

using namespace geode::prelude;

NXR_HACK_CREATE("Creator", "Zoom Bypass", "Lets you zoom an infinite amount in the editor", false);

class $modify(NXRZoomBypassEditorUI, EditorUI) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Creator").findHackByName("Zoom Bypass");

#if defined(GEODE_IS_WINDOWS)
        NXR::trySetPriority(self, "EditorUI::zoomIn", geode::Priority::Early);
        NXR::trySetPriority(self, "EditorUI::zoomOut", geode::Priority::Early);
        NXR::trySetPriority(self, "EditorUI::scrollWheel", geode::Priority::Early);

        NXR::tryAddHook(self, hack, "EditorUI::zoomIn");
        NXR::tryAddHook(self, hack, "EditorUI::zoomOut");
        NXR::tryAddHook(self, hack, "EditorUI::scrollWheel");
#else
        NXR::trySetPriority(self, "EditorUI::zoomGameLayer", geode::Priority::Early);
        NXR::tryAddHook(self, hack, "EditorUI::zoomGameLayer");
#endif
    }

    void zoomBypass(bool in) {
        float scale = m_editorLayer->m_groundLayer->getScale();
        float step = (!in && scale <= 0.105f) || (in && scale < 0.095f) ? 0.01f : 0.1f;

        scale += in ? step : -step;
        this->updateZoom(std::max(scale, 0.01f));
    }

#if defined(GEODE_IS_WINDOWS)
    void zoomIn(CCObject* sender) {
        zoomBypass(true);
    }

    void zoomOut(CCObject* sender) {
        zoomBypass(false);
    }

    void scrollWheel(float y, float x) {
        auto scale = m_editorLayer->m_groundLayer->getScale();

        EditorUI::scrollWheel(y, x);

        if (m_editorLayer->m_playbackMode != PlaybackMode::Playing && CCKeyboardDispatcher::get()->getControlKeyPressed()) {
            m_editorLayer->m_groundLayer->setScale(scale);

            if (y <= 0.0 && x <= 0.0) zoomBypass(true);
            else zoomBypass(false);
        }
    }
#else
    void zoomGameLayer(bool zoomingIn) {
        zoomBypass(zoomingIn);
    }
#endif
};
