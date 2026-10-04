#pragma once
#include <Geode/Geode.hpp>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "nxr_ui_kit.hpp"
#include "../../core/nxr_form.hpp"

namespace NXR::Kit {
    class PageBuilder final : public NXR::Form {
    public:
        PageBuilder(Host* host, std::function<void()> rebuild) : m_host(host), m_rebuild(std::move(rebuild)) {}

        void clear() { m_items.clear(); }
        const std::vector<ControlPtr>& items() const { return m_items; }
        Host* host() const { return m_host; }

        void addControl(ControlPtr control) { if (control) m_items.push_back(std::move(control)); }
        void addSection(const std::string& text);
        void addText(const std::string& text, bool center = false);
        void addPadding(float heightDp);
        void addRadioRow(const std::vector<std::string>& labels, std::function<int()> getCurrent, std::function<int(int)> onSelect);
        void addSelector(const std::string& label, std::function<std::string()> getText, std::function<void(std::function<void()>)> onOpen);
        void addButtons(std::vector<std::pair<std::string, std::function<void()>>> buttons, bool danger = false);
        void addNode(cocos2d::CCNode* node, float heightDp);
        void addLink(const std::string& title, std::function<std::string()> subtitle, std::function<void()> tap);
        void addKeybind(const std::string& label, std::function<std::string()> get, std::function<void()> onSet, std::function<void()> onClear);
        void addToggle(const std::string& label, std::function<bool()> get, std::function<void(bool)> set);
        void addButtonRow(const std::string& label, geode::Function<void()> callback, const std::string& secondLabel = "", geode::Function<void()> secondCallback = nullptr);

        void addConfigToggle(const std::string& label, const std::string& key, bool defaultValue = false, geode::Function<void(bool)> callback = nullptr) override;
        void addConfigModeToggle(const std::string& key, const std::string& offText, const std::string& onText, int defaultValue = 1, geode::Function<void(int)> callback = nullptr) override;
        void addConfigSelect(const std::string& label, const std::string& key, const std::vector<std::pair<std::string, int>>& options, int defaultValue, geode::Function<void(int)> callback = nullptr) override;
        void addConfigRadio(const std::string& label, const std::string& key, const std::vector<std::pair<std::string, int>>& options, int defaultValue, geode::Function<void(int)> callback = nullptr) override;
        void addConfigIntInput(const std::string& label, const std::string& key, int min, int max, int defaultValue = 0, geode::Function<void(int)> callback = nullptr) override;
        void addConfigFloatInput(const std::string& label, const std::string& key, float min, float max, float defaultValue = 0.f, geode::Function<void(float)> callback = nullptr) override;
        void addConfigColor3Hex(const std::string& label, const std::string& key, const std::string& defaultHex) override;
        void addConfigColor4Hex(const std::string& label, const std::string& key, const std::string& defaultHex) override;
        void addConfigChoice(const std::string& label, const std::string& key, const std::vector<std::string>& options, int defaultValue = 0, geode::Function<void(int)> callback = nullptr) override;
        void addConfigSlider(const std::string& label, const std::string& key, float min, float max, float defaultValue, float step, NXR::SliderScale scale = NXR::SliderScale::Linear, const std::vector<NXR::SliderPreset>& presets = {}, geode::Function<void(float)> callback = nullptr, bool integer = false, const std::string& suffix = "") override;
        void addConfigColor(const std::string& label, const std::string& key, const std::string& defaultHex, bool alpha = false, const std::string& rainbowKey = "") override;
        void addBoundToggle(const std::string& label, bool current, geode::Function<void(bool)> setter) override;
        void addSeparator(float height = 1.f) override;
        void requestRebuild() override;

    private:
        Host* m_host;
        std::function<void()> m_rebuild;
        std::vector<ControlPtr> m_items;
    };
}
