#pragma once
#include <Geode/Geode.hpp>
#include <vector>
#include <utility>
#include "../../core/nxr_hacks.hpp"
#include "../../core/nxr_form.hpp"

class NXROptionPopup : public geode::Popup {
protected:
    std::vector<std::pair<std::string, int>> m_options;
    int m_current = 0;
    geode::Function<void(int)> m_onPick;
    bool init(const std::string& title, const std::vector<std::pair<std::string, int>>& options, int current, geode::Function<void(int)> onPick);
public:
    static NXROptionPopup* create(const std::string& title, const std::vector<std::pair<std::string, int>>& options, int current, geode::Function<void(int)> onPick);
};

class NXRHackSettingsPopup : public geode::Popup, public NXR::Form {
protected:
    NXR::Hack* m_hack;
    bool init(NXR::Hack& hack);
public:
    geode::prelude::ScrollLayer* m_scrollLayer;
    cocos2d::CCMenu* m_currentRow = nullptr;

    static NXRHackSettingsPopup* create(NXR::Hack& hack);
    static void open(NXR::Hack& hack, const std::string& origin = "");

    void prepareNewRow();
    void rebuild();

    void addConfigToggle(const std::string& labelText, const std::string& key, bool defaultValue = false, geode::Function<void(bool)> callback = nullptr) override;
    void addConfigModeToggle(const std::string& key, const std::string& offText, const std::string& onText, int defaultValue = 1, geode::Function<void(int)> callback = nullptr) override;
    void addConfigSelect(const std::string& labelText, const std::string& key, const std::vector<std::pair<std::string, int>>& options, int defaultValue, geode::Function<void(int)> callback = nullptr) override;
    void addConfigRadio(const std::string& labelText, const std::string& key, const std::vector<std::pair<std::string, int>>& options, int defaultValue, geode::Function<void(int)> callback = nullptr) override;
    void addConfigIntInput(const std::string& labelText, const std::string& key, int min, int max, int defaultValue = 0, geode::Function<void(int)> callback = nullptr) override;
    void addConfigFloatInput(const std::string& labelText, const std::string& key, float min, float max, float defaultValue = 0.f, geode::Function<void(float)> callback = nullptr) override;
    void addConfigColor3Hex(const std::string& labelText, const std::string& key, const std::string& defaultHex) override;
    void addConfigColor4Hex(const std::string& labelText, const std::string& key, const std::string& defaultHex) override;
    void addBoundToggle(const std::string& labelText, bool current, geode::Function<void(bool)> setter) override;
    void addSeparator(float height = 1.f) override;
    void requestRebuild() override;
};

class NXRColorPopup : public geode::ColorPickPopup {
public:
    static NXRColorPopup* create(cocos2d::ccColor4B const& color, bool isRGBA) {
        auto ret = new NXRColorPopup();
        if (ret->init(color, isRGBA)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};
