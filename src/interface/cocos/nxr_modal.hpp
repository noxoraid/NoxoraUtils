#pragma once
#include <Geode/Geode.hpp>
#include <functional>
#include <string>
#include <vector>

namespace NXR::Modal {
    struct PopupParts {
        cocos2d::CCNode* mainLayer = nullptr;
        cocos2d::CCNode* bgSprite = nullptr;
        CCMenuItemSpriteExtra* closeBtn = nullptr;
        cocos2d::CCNode* buttonMenu = nullptr;
    };

    void skin(const PopupParts& parts, const std::string& title, float width, float height);

    CCMenuItemSpriteExtra* button(const std::string& text, float width, float height, bool primary, std::function<void()> callback);

    cocos2d::CCNode* iconNode(float size);

    std::vector<std::string> wrap(const std::string& text, size_t maxChars);

    class Row : public cocos2d::CCNode {
    public:
        static Row* create(const std::string& text, float width, float height, bool selected);
        void setSelected(bool value);

    private:
        bool init(const std::string& text, float width, float height, bool selected);
        void redraw();

        cocos2d::CCDrawNode* m_bg = nullptr;
        geode::Label* m_label = nullptr;
        float m_w = 0.f;
        float m_h = 0.f;
        bool m_selected = false;
    };
}
