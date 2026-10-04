#pragma once
#include <Geode/Geode.hpp>

class NXROverlayButton : public cocos2d::CCMenu {
    geode::Function<void()> m_callback;
    cocos2d::CCSprite* m_sprite = nullptr;
    cocos2d::CCPoint m_target;
    float m_scale = 0.60f;
    bool m_moved = false;

    bool init(const char* file);
    void update(float dt) override;

    void savePos();
    float radius() const { return m_sprite->getScaledContentSize().width * .5f; }

    bool ccTouchBegan(cocos2d::CCTouch*, cocos2d::CCEvent*) override;
    void ccTouchMoved(cocos2d::CCTouch*, cocos2d::CCEvent*) override;
    void ccTouchEnded(cocos2d::CCTouch*, cocos2d::CCEvent*) override;
    void ccTouchCancelled(cocos2d::CCTouch* t, cocos2d::CCEvent* e) override { ccTouchEnded(t, e); }
    void registerWithTouchDispatcher() override;

public:
    static NXROverlayButton* instance;
    static NXROverlayButton* create(const char* file);
    static NXROverlayButton* get(const char* file = "NXR_buttonUI.png"_spr);
    void setCallback(geode::Function<void()>&& cb) { m_callback = std::move(cb); }
    void setVisible(bool visible) override;

    void setSizeScale(float scale);
    float getSizeScale() const { return m_scale; }

    void moveTo(float x, float y);
    void moveToCorner(int corner);
    cocos2d::CCPoint getTarget() const { return m_target; }
};
