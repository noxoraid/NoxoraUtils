#include <Geode/Geode.hpp>
#include <Geode/binding/EditorPauseLayer.hpp>
#include <Geode/binding/EditorUI.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include "../../core/nxr_config.hpp"
#include "nxr_overlay_button.hpp"
#include "../../core/nxr_ui_mode.hpp"

class $modify(NXROverlayButtonVisiblityPL, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        NXR::Ui::closeMenu();

        if (NXRConfig::get().get<bool>("nxr.ui_icon.hide_on_game", true))
            NXROverlayButton::get()->setVisible(false);

        return true;
    }

    void pauseGame(bool unfocused) {
        PlayLayer::pauseGame(unfocused);

        NXROverlayButton::get()->setVisible(true);
    }

    void resume() {
        PlayLayer::resume();

        NXR::Ui::closeMenu();

        if (NXRConfig::get().get<bool>("nxr.ui_icon.hide_on_game", true))
            NXROverlayButton::get()->setVisible(false);
    }

    void resumeAndRestart(bool fromStart) {
        PlayLayer::resumeAndRestart(fromStart);

        NXR::Ui::closeMenu();

        if (NXRConfig::get().get<bool>("nxr.ui_icon.hide_on_game", true))
            NXROverlayButton::get()->setVisible(false);
    }

    void onQuit() {
        NXR::Ui::closeMenu();
        PlayLayer::onQuit();
        NXROverlayButton::get()->setVisible(true);
    }

    void showEndLayer() {
        PlayLayer::showEndLayer();
        NXROverlayButton::get()->setVisible(true);
    }
};

class $modify(NXROverlayButtonVisiblityLEL, LevelEditorLayer) {
    bool init(GJGameLevel* level, bool noUI) {
        if (!LevelEditorLayer::init(level, noUI)) return false;

        NXR::Ui::closeMenu();

        if (NXRConfig::get().get<bool>("nxr.ui_icon.hide_on_editor", false))
            NXROverlayButton::get()->setVisible(false);

        return true;
    }
};

class $modify(NXROverlayButtonVisiblityEUI, EditorUI) {
    void onPause(cocos2d::CCObject *sender) {
        EditorUI::onPause(sender);

        NXROverlayButton::get()->setVisible(true);
    }
};

class $modify(NXROverlayButtonVisiblityEPL, EditorPauseLayer) {
    void onResume(cocos2d::CCObject *sender) {
        EditorPauseLayer::onResume(sender);

        NXR::Ui::closeMenu();

        if (NXRConfig::get().get<bool>("nxr.ui_icon.hide_on_editor", false))
            NXROverlayButton::get()->setVisible(false);
    }
};

class $modify(NXROverlayButtonVisiblityELL, EndLevelLayer) {
    void onRestartCheckpoint(cocos2d::CCObject *sender) {
        EndLevelLayer::onRestartCheckpoint(sender);

        if (NXRConfig::get().get<bool>("nxr.ui_icon.hide_on_game", true) && !this->m_exiting)
            NXROverlayButton::get()->setVisible(false);
    }

    void onReplay(cocos2d::CCObject *sender) {
        EndLevelLayer::onRestartCheckpoint(sender);

        if (NXRConfig::get().get<bool>("nxr.ui_icon.hide_on_game", true) && !this->m_exiting)
            NXROverlayButton::get()->setVisible(false);
    }
};
