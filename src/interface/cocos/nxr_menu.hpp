#pragma once
#include <Geode/Geode.hpp>

class NXRMenu : public cocos2d::CCMenu {
public:
    static NXRMenu* create();

    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchCancelled(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;

private:
    geode::ScrollLayer* findScrollLayer();
    void finishDrag();

    cocos2d::CCPoint m_start;
    cocos2d::CCPoint m_lastLocal;
    float m_lastSet = 0.f;
    bool m_dragging = false;
    bool m_external = false;
};
