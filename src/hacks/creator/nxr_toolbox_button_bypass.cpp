#include <Geode/Geode.hpp>
#include <Geode/modify/EditorOptionsLayer.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"

using namespace geode::prelude;

NXR_HACK_CREATE("Creator", "Toolbox Button Bypass", "Allows more rows and buttons per row in the editor toolbox", false);

class $modify(NXRToolboxBypassEditorOptionsLayer, EditorOptionsLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Creator").findHackByName("Toolbox Button Bypass");

        NXR::tryAddHook(self, hack, "EditorOptionsLayer::onButtonRows");
        NXR::tryAddHook(self, hack, "EditorOptionsLayer::onButtonsPerRow");
    }

    void onButtonRows(CCObject* sender) {
        m_buttonRows += sender->getTag() ? 1 : -1;
        m_buttonRows = std::max(1, m_buttonRows);
        m_buttonRowsLabel->setString(fmt::format("{}", m_buttonRows).c_str());
    }

    void onButtonsPerRow(CCObject* sender) {
        m_buttonsPerRow += sender->getTag() ? 1 : -1;
        m_buttonsPerRow = std::max(1, m_buttonsPerRow);
        m_buttonsPerRowLabel->setString(fmt::format("{}", m_buttonsPerRow).c_str());
    }
};
