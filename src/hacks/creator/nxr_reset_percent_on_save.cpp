#include <Geode/Geode.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"

using namespace geode::prelude;

NXR_HACK_CREATE("Creator", "Reset Percent on Save", "Resets the level percent to 0 every time the level is saved", false);

class $modify(NXRResetPercentEditorPauseLayer, EditorPauseLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Creator").findHackByName("Reset Percent on Save");

        NXR::tryAddHook(self, hack, "EditorPauseLayer::saveLevel");
    }

    void saveLevel() {
        if (m_editorLayer && m_editorLayer->m_level && m_editorLayer->m_level->m_levelType == GJLevelType::Editor) {
            m_editorLayer->m_level->m_normalPercent = 0;
        }

        EditorPauseLayer::saveLevel();
    }
};
