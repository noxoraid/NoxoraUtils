#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"

using namespace geode::prelude;

NXR_HACK_CREATE("Creator", "Hide UI", "Hides the editor UI while building", false);

class $modify(NXRHideUIEditorUI, EditorUI) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Creator").findHackByName("Hide UI");

        NXR::tryAddHook(self, hack, "EditorUI::init");

        hack.setHandler([](bool enabled) {
            if (auto* lel = LevelEditorLayer::get()) {
                if (auto* ui = lel->m_editorUI) ui->setVisible(!enabled);
            }
        });
    }

    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) return false;

        this->setVisible(false);
        return true;
    }
};
