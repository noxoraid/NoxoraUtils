#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <Geode/modify/LevelTools.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"

using namespace geode::prelude;

NXR_HACK_CREATE("Creator", "Level Edit", "Edit any online level", false);

class $modify(NXRLevelEditPauseLayer, PauseLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Creator").findHackByName("Level Edit");

        NXR::tryAddHook(self, hack, "PauseLayer::customSetup");
        NXR::tryAddHook(self, hack, "PauseLayer::onTryEdit");
    }

    void customSetup() {
        auto* pl = PlayLayer::get();
        if (!pl || !pl->m_level) {
            PauseLayer::customSetup();
            return;
        }

        auto levelType = pl->m_level->m_levelType;
        pl->m_level->m_levelType = GJLevelType::Editor;
        PauseLayer::customSetup();
        pl->m_level->m_levelType = levelType;
    }

    void onTryEdit(CCObject* sender) {
        auto* pl = PlayLayer::get();
        if (!pl || !pl->m_level) {
            PauseLayer::onTryEdit(sender);
            return;
        }

        auto levelType = pl->m_level->m_levelType;
        pl->m_level->m_levelType = GJLevelType::Editor;
        PauseLayer::onTryEdit(sender);
        pl->m_level->m_levelType = levelType;
    }
};

class $modify(NXRLevelEditEditorUI, EditorUI) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Creator").findHackByName("Level Edit");

        NXR::tryAddHook(self, hack, "EditorUI::onSettings");
    }

    void onSettings(CCObject* sender) {
        auto* lel = LevelEditorLayer::get();
        if (!lel || !lel->m_level) {
            EditorUI::onSettings(sender);
            return;
        }

        auto levelType = lel->m_level->m_levelType;
        lel->m_level->m_levelType = GJLevelType::Editor;
        EditorUI::onSettings(sender);
        lel->m_level->m_levelType = levelType;
    }
};

class $modify(NXRLevelEditEditorPauseLayer, EditorPauseLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Creator").findHackByName("Level Edit");

        NXR::tryAddHook(self, hack, "EditorPauseLayer::init");
    }

    bool init(LevelEditorLayer* editorLayer) {
        if (!editorLayer || !editorLayer->m_level) return EditorPauseLayer::init(editorLayer);

        auto levelType = editorLayer->m_level->m_levelType;
        editorLayer->m_level->m_levelType = GJLevelType::Editor;
        bool ret = EditorPauseLayer::init(editorLayer);
        editorLayer->m_level->m_levelType = levelType;

        return ret;
    }
};

class $modify(NXRLevelEditLevelTools, LevelTools) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Creator").findHackByName("Level Edit");

        NXR::tryAddHook(self, hack, "LevelTools::verifyLevelIntegrity");
    }

    static bool verifyLevelIntegrity(gd::string str, int id) {
        LevelTools::verifyLevelIntegrity(str, id);
        return true;
    }
};
