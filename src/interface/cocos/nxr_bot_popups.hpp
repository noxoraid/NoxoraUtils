#pragma once
#include <Geode/Geode.hpp>
#include "../../core/nxr_bot.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include "../../core/nxr_macro_import.hpp"

class NXRNamePopup : public geode::Popup {
protected:
    std::string m_text;
    geode::Function<void(const std::string&)> m_onConfirm;

    bool init(const std::string& title, geode::Function<void(const std::string&)> onConfirm);

public:
    static NXRNamePopup* create(const std::string& title, geode::Function<void(const std::string&)> onConfirm);
};

class NXRReplayInfoPopup : public geode::Popup {
protected:
    bool init(const std::string& name, const NXR::Bot::Macro& macro);
public:
    static NXRReplayInfoPopup* create(const std::string& name, const NXR::Bot::Macro& macro);
};

class NXRInfoPopup : public geode::Popup {
protected:
    bool init(const std::string& title, const std::string& body);

public:
    static NXRInfoPopup* create(const std::string& title, const std::string& body);

    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void registerWithTouchDispatcher() override;
};

class NXRReplayPickerPopup : public geode::Popup {
protected:
    bool m_allowClear = false;
    std::string m_selected;
    geode::Function<void(const std::string&)> m_onPick;
    std::vector<geode::Ref<CCMenuItemSpriteExtra>> m_items;
    std::vector<std::string> m_names;

    bool init(const std::string& title, const std::string& actionLabel, geode::Function<void(const std::string&)> onPick, bool allowClear);
    void refreshSelection();

public:
    static NXRReplayPickerPopup* create(const std::string& title, const std::string& actionLabel, geode::Function<void(const std::string&)> onPick, bool allowClear = false);
};

class NXRReplayBrowserPopup : public geode::Popup {
protected:
    std::vector<NXR::Bot::ReplayFile> m_files;
    size_t m_selected = static_cast<size_t>(-1);
    geode::Function<void(const std::filesystem::path&)> m_onPick;
    std::vector<geode::Ref<CCMenuItemSpriteExtra>> m_items;

    bool init(const std::string& title, geode::Function<void(const std::filesystem::path&)> onPick);
    void refreshSelection();

public:
    static NXRReplayBrowserPopup* create(const std::string& title, geode::Function<void(const std::filesystem::path&)> onPick);
};
