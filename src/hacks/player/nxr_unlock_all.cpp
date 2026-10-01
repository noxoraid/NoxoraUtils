#include <Geode/Geode.hpp>
#include <Geode/modify/GameManager.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_config.hpp"

NXR_HACK_CREATE("Player", "Unlock All Icon & Color", "Unlocks every icon, color, trail, death effect and item", true);

class $modify(NXRUnlockAllGameManager, GameManager) {
    static void onModify(auto& self) {
        auto& gui = NXR::Gui::get();
        auto& hack = gui.getWindow("Player").findHackByName("Unlock All Icon & Color");

        (void) self.setHookPriority("GameManager::isIconUnlocked", -30);
        (void) self.setHookPriority("GameManager::isColorUnlocked", -30);

        hack.addHookPtr(self.getHook("GameManager::isIconUnlocked").unwrap());
        hack.addHookPtr(self.getHook("GameManager::isColorUnlocked").unwrap());
    }

    bool isIconUnlocked(int id, IconType type) {
        return true;
    }

    bool isColorUnlocked(int id, UnlockType type) {
        return true;
    }
};
