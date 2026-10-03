#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJGameLevel.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"
#include "../../core/nxr_config.hpp"

NXR_HACK_CREATE(
    "Global", "Safe Mode",
    "Wins, progress and best percent are not counted or saved while this is on",
    false
);

namespace {
    bool safeOn() {
        return NXRConfig::get().get<bool>("nxr.global.safe_mode", false);
    }
}

class $modify(NXRSafeModePlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Global").findHackByName("Safe Mode");
        NXR::tryAddHook(self, hack, "PlayLayer::levelComplete");
    }

    void levelComplete() {
        if (!safeOn()) {
            PlayLayer::levelComplete();
            return;
        }

        const bool previous = m_isTestMode;
        m_isTestMode = true;
        PlayLayer::levelComplete();
        m_isTestMode = previous;

        showBanner();
    }

    void showBanner() {
        auto* scene = cocos2d::CCDirector::sharedDirector()->getRunningScene();
        if (!scene) return;

        scene->removeChildByID("nxr-safe-mode-banner"_spr);

        const auto win = cocos2d::CCDirector::sharedDirector()->getWinSize();

        auto* label = cocos2d::CCLabelBMFont::create("It's safe mode", "bigFont.fnt");
        label->setID("nxr-safe-mode-banner"_spr);
        label->setScale(0.9f);
        label->setColor({255, 90, 90});
        label->setPosition({win.width * 0.5f, win.height - 40.f});
        label->setOpacity(0);
        label->runAction(cocos2d::CCSequence::create(
            cocos2d::CCFadeTo::create(0.2f, 255),
            cocos2d::CCDelayTime::create(5.f),
            cocos2d::CCFadeTo::create(0.5f, 0),
            cocos2d::CCRemoveSelf::create(),
            nullptr
        ));
        scene->addChild(label, 100000);
    }
};

class $modify(NXRSafeModeLevel, GJGameLevel) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Global").findHackByName("Safe Mode");
        NXR::tryAddHook(self, hack, "GJGameLevel::savePercentage");
    }

    void savePercentage(int percent, bool isPracticeMode, int clicks, int attempts, bool isChkValid) {
        if (safeOn()) return;
        GJGameLevel::savePercentage(percent, isPracticeMode, clicks, attempts, isChkValid);
    }
};
