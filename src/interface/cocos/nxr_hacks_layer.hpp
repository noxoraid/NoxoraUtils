#pragma once
#include <Geode/Geode.hpp>
#include "nxr_hacks_tab.hpp"

class NXRHacksLayer : public geode::Popup {
private:
    bool init();
    static NXRHacksLayer* instance;

    std::vector<NXRHacksTab*> m_tabs;
    std::vector<CCMenuItemSpriteExtra*> m_buttonTabs;
    int m_index = 0;
    int m_lastIndexScroll = -1;

    geode::ScrollLayer* m_tabsScrollLayer = nullptr;

    void switchTab(int newIndex);
public:
    ~NXRHacksLayer();

    static NXRHacksLayer* create();
    static NXRHacksLayer* get();

    static bool isOpened();
    void onClose(CCObject* object);
};
