#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_bot.hpp"

NXR_HACK_CREATE(
    "Global", "Click Between Frames",
    "Built-in click between frames and steps. Clicks are applied on the exact physics step they happened on instead of the start of the next frame. Needs no other mod",
    false
);

class $modify(NXRClickBetweenBaseGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Global").findHackByName("Click Between Frames");
        NXR::trySetPriority(self, "GJBaseGameLayer::update", -100);
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::update");
    }

    void update(float dt) {
        const bool botBusy = NXR::Bot::State::get().mode != NXR::Bot::Mode::Off;
        const bool previous = m_clickBetweenSteps;
        if (!botBusy) m_clickBetweenSteps = true;
        GJBaseGameLayer::update(dt);
        if (botBusy) m_clickBetweenSteps = previous;
    }
};
