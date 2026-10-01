#include <Geode/Geode.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"

using namespace geode::prelude;

NXR_HACK_CREATE("Creator", "Verification Bypass", "Publish a level without verifying it first", false);

class $modify(NXRVerifyEditLevelLayer, EditLevelLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Creator").findHackByName("Verification Bypass");

        NXR::tryAddHook(self, hack, "EditLevelLayer::init");
    }

    bool init(GJGameLevel* level) {
        if (level) level->m_isVerified = true;
        return EditLevelLayer::init(level);
    }
};
