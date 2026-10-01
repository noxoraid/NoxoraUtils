#include <Geode/Geode.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"

using namespace geode::prelude;

NXR_HACK_CREATE("Creator", "No (C) Mark", "Removes the copyright mark on copied levels", false);

class $modify(NXRNoCMarkEditLevelLayer, EditLevelLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Creator").findHackByName("No (C) Mark");

        NXR::tryAddHook(self, hack, "EditLevelLayer::onShare");
    }

    void onShare(CCObject* sender) {
        if (m_level) m_level->m_originalLevel = 0;
        EditLevelLayer::onShare(sender);
    }
};
