#include "nxr_menu.hpp"
#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace {
    constexpr float kDragThreshold = 6.f;
}

NXRMenu* NXRMenu::create() {
    auto* ret = new NXRMenu();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

geode::ScrollLayer* NXRMenu::findScrollLayer() {
    for (auto* node = this->getParent(); node; node = node->getParent()) {
        if (auto* scroll = typeinfo_cast<ScrollLayer*>(node)) return scroll;
    }
    return nullptr;
}

bool NXRMenu::ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
    m_dragging = false;
    m_external = false;
    m_start = touch->getLocation();
    return cocos2d::CCMenu::ccTouchBegan(touch, event);
}

void NXRMenu::ccTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
    auto* scroll = this->findScrollLayer();
    if (!scroll) {
        cocos2d::CCMenu::ccTouchMoved(touch, event);
        return;
    }

    const auto location = touch->getLocation();

    if (!m_dragging) {
        if (cocos2d::ccpDistance(location, m_start) < kDragThreshold) {
            cocos2d::CCMenu::ccTouchMoved(touch, event);
            return;
        }

        m_dragging = true;
        m_external = false;

        if (m_pSelectedItem) {
            m_pSelectedItem->unselected();
            m_pSelectedItem = nullptr;
        }

        m_lastLocal = scroll->convertToNodeSpace(location);
        m_lastSet = scroll->m_contentLayer->getPositionY();
        return;
    }

    auto* content = scroll->m_contentLayer;
    const auto local = scroll->convertToNodeSpace(location);

    if (std::fabs(content->getPositionY() - m_lastSet) > 0.5f) m_external = true;

    if (!m_external) {
        const float viewHeight = scroll->getContentHeight();
        const float contentHeight = content->getContentHeight();

        if (contentHeight > viewHeight) {
            const float minY = viewHeight - contentHeight;
            const float peekLow = static_cast<float>(scroll->m_peekLimitBottom);
            const float peekHigh = static_cast<float>(scroll->m_peekLimitTop);
            const float next = std::clamp(content->getPositionY() + (local.y - m_lastLocal.y), minY - peekLow, peekHigh);
            content->setPositionY(next);
        }
    }

    m_lastLocal = local;
    m_lastSet = content->getPositionY();
}

void NXRMenu::finishDrag() {
    if (!m_dragging || m_external) return;

    auto* scroll = this->findScrollLayer();
    if (!scroll) return;

    auto* content = scroll->m_contentLayer;
    const float viewHeight = scroll->getContentHeight();
    const float contentHeight = content->getContentHeight();
    if (contentHeight <= viewHeight) return;

    const float minY = viewHeight - contentHeight;
    const float clamped = std::clamp(content->getPositionY(), minY, 0.f);

    if (std::fabs(clamped - content->getPositionY()) > 0.1f) {
        content->stopAllActions();
        content->runAction(cocos2d::CCEaseExponentialOut::create(
            cocos2d::CCMoveTo::create(0.2f, cocos2d::CCPoint{content->getPositionX(), clamped})
        ));
    }
}

void NXRMenu::ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
    this->finishDrag();
    m_dragging = false;
    m_external = false;
    cocos2d::CCMenu::ccTouchEnded(touch, event);
}

void NXRMenu::ccTouchCancelled(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
    m_dragging = false;
    m_external = false;
    cocos2d::CCMenu::ccTouchCancelled(touch, event);
}
