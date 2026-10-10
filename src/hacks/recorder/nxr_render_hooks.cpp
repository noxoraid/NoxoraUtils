#include <Geode/Geode.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/modify/CCEGLView.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/OverlayManager.hpp>
#include <vector>
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_render_keys.hpp"
#include "../../core/nxr_render_session.hpp"
#include "../../interface/cocos/nxr_overlay_button.hpp"

namespace {
    bool g_buttonHidden = false;
    bool g_buttonWasVisible = true;

    bool recorderHackEnabled(const char* name) {
        return NXR::Gui::get().getWindow(NXR::Render::Keys::window).findHackByName(name).getEnabled();
    }

    std::vector<geode::Ref<cocos2d::CCNode>> g_concealed;

    void concealChild(cocos2d::CCNode* node) {
        if (!node || !node->isVisible()) return;
        g_concealed.emplace_back(node);
        node->setVisible(false);
    }

    void concealNotifications(cocos2d::CCNode* parent) {
        if (!parent || !parent->getChildren()) return;
        for (auto* child : geode::cocos::CCArrayExt<cocos2d::CCNode*>(parent->getChildren())) {
            if (geode::cast::typeinfo_cast<geode::Notification*>(child)) concealChild(child);
        }
    }

    void concealTagged(cocos2d::CCNode* parent) {
        if (!parent || !parent->getChildren()) return;
        for (auto* child : geode::cocos::CCArrayExt<cocos2d::CCNode*>(parent->getChildren())) {
            if (child->getID() == NXR::Render::Keys::concealId) concealChild(child);
        }
    }

    void concealForRecording() {
        concealNotifications(geode::OverlayManager::get());
        concealNotifications(cocos2d::CCDirector::sharedDirector()->getRunningScene());
        if (auto* layer = PlayLayer::get()) {
            concealTagged(layer->m_uiLayer);
            concealTagged(layer->m_objectLayer);
        }
    }

    void revealAfterRecording() {
        for (auto& node : g_concealed) node->setVisible(true);
        g_concealed.clear();
    }

    void applyButtonVisibility(bool shouldHide) {
        auto* button = NXROverlayButton::instance;
        if (!button) return;

        if (shouldHide && !g_buttonHidden) {
            g_buttonWasVisible = button->isVisible();
            g_buttonHidden = true;
            button->setVisible(false);
            return;
        }

        if (!shouldHide && g_buttonHidden) {
            g_buttonHidden = false;
            button->setVisible(g_buttonWasVisible);
        }
    }
}

class $modify(NXRRenderStepScheduler, cocos2d::CCScheduler) {
    void update(float dt) {
        auto& session = NXR::Render::GameplayVideoSession::get();
        if (session.usesFixedStep()) dt = session.stepSeconds();
        CCScheduler::update(dt);
    }
};

class $modify(NXRRenderSwapView, cocos2d::CCEGLView) {
    void swapBuffers() {
        NXR::Render::GameplayVideoSession::get().captureFromBackBuffer();
        CCEGLView::swapBuffers();
    }
};

class $modify(NXRRenderDirector, cocos2d::CCDirector) {
    void drawScene() {
        auto& session = NXR::Render::GameplayVideoSession::get();
        const bool advancing = session.isAdvancing();
        session.syncRecordingClock();
        applyButtonVisibility(advancing && recorderHackEnabled(NXR::Render::Keys::hideButtonHack));
        if (advancing) concealForRecording();
        CCDirector::drawScene();
        revealAfterRecording();
    }
};

class $modify(NXRRenderPlayLayer, PlayLayer) {
    void levelComplete() {
        PlayLayer::levelComplete();

        auto& session = NXR::Render::GameplayVideoSession::get();
        if (!session.isActive()) return;
        if (!recorderHackEnabled(NXR::Render::Keys::autoStopHack)) return;

        const int tailSeconds = std::clamp(NXRConfig::get().get<int>(NXR::Render::Keys::tailSeconds, 3), 0, 30);
        session.armTail(tailSeconds * session.frameRate());
    }

    void onQuit() {
        NXR::Render::GameplayVideoSession::get().finish();
        PlayLayer::onQuit();
    }
};
