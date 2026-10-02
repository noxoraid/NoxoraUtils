#pragma once
#include <Geode/Geode.hpp>
#include <vector>
#include <functional>
#include <string>
#include "../../core/nxr_hacks.hpp"

class NXRHacksTab : public cocos2d::CCMenu {
public:
    static NXRHacksTab* create();
    void addToggle(NXR::Hack& hck);

    void addHackToggle(const std::string& label, const std::string& key, bool defaultValue = false, geode::Function<void(bool)> callback = nullptr);
    void addConfigToggle(const std::string& label, const std::string& key, bool defaultValue = false, geode::Function<void(bool)> callback = nullptr);
    void addConfigIntInput(const std::string& label, const std::string& key, int defaultValue = 0, int min = 0, int max = 100, geode::Function<void(int)> callback = nullptr);
    void addConfigFloatInput(const std::string& label, const std::string& key, float defaultValue = 0.f, float min = 0.f, float max = 100.f, geode::Function<void(float)> callback = nullptr);
    void addConfigColor3Hex(const std::string& labelText, const std::string& key, const std::string& defaultHex);
    void addConfigButton(const std::string& labelText, geode::Function<void()> callback, const std::string& secondLabelText = "", geode::Function<void()> secondCallback = nullptr);
    void addRadioRow(const std::vector<std::string>& labels, geode::Function<int()> getCurrent, geode::Function<int(int)> onSelect);
    void addSelector(const std::string& title, geode::Function<std::string()> getText, geode::Function<void(std::function<void()>)> onOpen);
    void addKeybindRow(const std::string& label, geode::Function<std::string()> getText, geode::Function<void()> onSet, geode::Function<void()> onClear);
    geode::Label* AddTextToToggle(const char *str, CCMenuItemToggler* toggler, float x_space = 22.f);

    void addText(const std::string& text, float scale = 0.5f);
    void prepareNewRow();

    geode::prelude::ScrollLayer* m_scrollLayer;
    cocos2d::CCMenu* m_currentRow = nullptr;

    void addPadding(float height = 0.f);
    void addSeparator(float height = 1.f);
private:
    NXRHacksTab() = default;
    bool init();
};
