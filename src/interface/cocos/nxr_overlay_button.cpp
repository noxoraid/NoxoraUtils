#include "nxr_overlay_button.hpp"
#include "../../core/nxr_config.hpp"
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/GameManager.hpp>

using namespace cocos2d;
using namespace geode::prelude;

NXROverlayButton* NXROverlayButton::instance = nullptr;

NXROverlayButton* NXROverlayButton::create(const char* file) {
    auto* ret = new NXROverlayButton();
    if (ret->init(file)) { ret->autorelease(); return ret; }
    delete ret;
    return nullptr;
}

NXROverlayButton* NXROverlayButton::get(const char* file) {
    if (!instance) {
        instance = create(file);
    }
    return instance;
}

bool NXROverlayButton::init(const char* file) {
    if (!CCMenu::init()) return false;

    auto& config = NXRConfig::get();
    m_scale = config.get<float>("nxr.ui_icon.scale", 0.5f);

    m_sprite = CCSprite::create(file);
    m_sprite->setScale(m_scale);

    float m_minOpacity = config.get<float>("nxr.ui_icon.minOpacity", 0.80f);
    m_sprite->setOpacity(m_minOpacity * 255);

    m_sprite->setPosition({
        config.get<float>("nxr.ui_icon.x", 80.f),
        config.get<float>("nxr.ui_icon.y", 80.f)
    });
    m_target = m_sprite->getPosition();
    addChild(m_sprite);

    setZOrder(1000);
    setPosition({0, 0});
    scheduleUpdate();

    bool isVisible = config.get<bool>("nxr.ui_icon.visible", true);
    this->setVisible(isVisible);

    return true;
}

void NXROverlayButton::setSizeScale(float scale) {
    m_scale = scale;
    if (m_sprite) {
        m_sprite->setScale(m_scale);
    }
    NXRConfig::get().set<float>("nxr.ui_icon.scale", m_scale);
}

void NXROverlayButton::setVisible(bool visible) {
    if (isVisible() == visible) return;

    CCMenu::setVisible(visible);

    if (auto dispatcher = CCDirector::sharedDirector()->getTouchDispatcher()) {
        if (visible) {
            dispatcher->addTargetedDelegate(this, -1000, true);
        } else {
            dispatcher->removeDelegate(this);
        }
    }

    NXRConfig::get().set<bool>("nxr.ui_icon.visible", visible);
}

void NXROverlayButton::update(float dt) {
    auto win_size = CCDirector::sharedDirector()->getWinSize();
    float r = radius();
    m_target.x = std::clamp(m_target.x, r, win_size.width - r);
    m_target.y = std::clamp(m_target.y, r, win_size.height - r);

    CCPoint pos = m_sprite->getPosition();
    if (ccpDistance(pos, m_target) < 0.5f) return;
    m_sprite->setPosition(ccpLerp(pos, m_target, 12.f * dt));
}

bool NXROverlayButton::ccTouchBegan(CCTouch* t, CCEvent*) {
    if (!isVisible()) return false;

    CCPoint tp = convertToNodeSpace(t->getLocation());
    if (ccpDistance(tp, m_sprite->getPosition()) > radius()) return false;

    m_moved = false;
    m_sprite->stopAllActions();
    m_sprite->runAction(CCScaleTo::create(0.08f, m_scale * 0.85f));
    m_sprite->runAction(CCFadeTo::create(0.08f, 255));
    return true;
}

void NXROverlayButton::ccTouchMoved(CCTouch* t, CCEvent*) {
    CCPoint tp = convertToNodeSpace(t->getLocation());
    if (!m_moved && ccpDistance(tp, m_sprite->getPosition()) < radius()) return;
    m_moved = true;

    auto win_size = CCDirector::sharedDirector()->getWinSize();
    float r = m_sprite->getContentSize().width * m_scale * 0.60f;
    m_target.x = std::clamp(tp.x, r, win_size.width - r);
    m_target.y = std::clamp(tp.y, r, win_size.height - r);
}

void NXROverlayButton::ccTouchEnded(CCTouch*, CCEvent*) {
    m_sprite->stopAllActions();
    m_sprite->runAction(CCEaseBackOut::create(CCScaleTo::create(0.25f, m_scale)));

    float m_minOpacity = NXRConfig::get().get<float>("nxr.ui_icon.minOpacity", 0.80f);
    m_sprite->runAction(CCSequence::create(
        CCDelayTime::create(0.8f),
        CCFadeTo::create(0.3f, m_minOpacity * 255),
        nullptr
    ));

    if (!m_moved && m_callback) m_callback();
    m_moved = false;
    savePos();
}

void NXROverlayButton::savePos() {
    NXRConfig::get().set<float>("nxr.ui_icon.x", m_target.x);
    NXRConfig::get().set<float>("nxr.ui_icon.y", m_target.y);
}

void NXROverlayButton::registerWithTouchDispatcher() {
    CCTouchDispatcher::get()->addTargetedDelegate(this, -1000, true);
}

#ifdef GEODE_IS_MOBILE
#include "nxr_hacks_layer.hpp"

static bool inited = false;
class $modify(NXRCocosInitMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        if (!inited) {
            inited = true;
            auto button = NXROverlayButton::get();
            button->setID("nxr.toggle.ui"_spr);
            button->setCallback([]() {
                NXRHacksLayer::isOpened() ? NXRHacksLayer::get()->onClose(nullptr) : NXRHacksLayer::get()->show();
            });
            OverlayManager::get()->addChild(button);
        }
        return true;
    }
};

class $modify(NXRRefreshSpriteHook, GameManager) {
    void reloadAll(bool switchingModes, bool toFullscreen, bool borderless, bool fix, bool unused) {
        OverlayManager::get()->removeChildByID("nxr.toggle.ui"_spr);
        NXROverlayButton::instance = nullptr;
        inited = false;
        GameManager::reloadAll(switchingModes, toFullscreen, borderless, fix, unused);
    }
};
#endif
