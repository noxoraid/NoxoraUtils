#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"

using namespace geode::prelude;

NXR_HACK_CREATE("Creator", "Custom Object Bypass", "Removes the limit of 1000 objects on custom objects", false);

class $modify(NXRCustomObjectBypassEditorUI, EditorUI) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Creator").findHackByName("Custom Object Bypass");

        NXR::tryAddHook(self, hack, "EditorUI::onNewCustomItem");
    }

    void onNewCustomItem(CCObject* sender) {
        auto* gm = GameManager::get();
        if (!gm) return;

        CCArray* objects = nullptr;

        if (m_selectedObjects && m_selectedObjects->count()) {
            objects = m_selectedObjects;
        } else if (m_selectedObject) {
            objects = CCArray::create();
            objects->addObject(m_selectedObject);
        } else {
            return;
        }

        gm->addNewCustomObject(copyObjects(objects, false, false));
        m_selectedObjectIndex = 0;
        reloadCustomItems();
    }
};
