#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"

using namespace geode::prelude;

NXR_HACK_CREATE("Creator", "Copy Hack", "Copy any online level, even one marked as not copyable, without its password", false);

namespace {
    void unlockCopy(GJGameLevel* level) {
        if (!level) return;
        level->m_password = 1;
        level->m_failedPasswordAttempts = 0;
    }
}

class $modify(NXRCopyHackLevelInfoLayer, LevelInfoLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Creator").findHackByName("Copy Hack");

        NXR::tryAddHook(self, hack, "LevelInfoLayer::init");
        NXR::tryAddHook(self, hack, "LevelInfoLayer::levelDownloadFinished");
        NXR::tryAddHook(self, hack, "LevelInfoLayer::updateLabelValues");
        NXR::tryAddHook(self, hack, "LevelInfoLayer::tryCloneLevel");
    }

    bool init(GJGameLevel* level, bool challenge) {
        unlockCopy(level);
        if (!LevelInfoLayer::init(level, challenge)) return false;

        unlockCopy(level);
        return true;
    }

    void levelDownloadFinished(GJGameLevel* level) {
        unlockCopy(level);
        LevelInfoLayer::levelDownloadFinished(level);
    }

    void updateLabelValues() {
        unlockCopy(m_level);
        LevelInfoLayer::updateLabelValues();
    }

    void tryCloneLevel(CCObject* sender) {
        unlockCopy(m_level);
        LevelInfoLayer::tryCloneLevel(sender);
    }
};
