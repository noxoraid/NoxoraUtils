#include "nxr_ui_page.hpp"
#include "../../core/nxr_config.hpp"

using namespace geode::prelude;

namespace NXR::Kit {
    namespace {
        template <class... Args>
        std::function<void(Args...)> share(geode::Function<void(Args...)> fn) {
            auto holder = std::make_shared<geode::Function<void(Args...)>>(std::move(fn));
            return [holder](Args... args) {
                if (*holder) (*holder)(args...);
            };
        }

        int indexOfValue(const std::vector<std::pair<std::string, int>>& options, int value) {
            for (size_t i = 0; i < options.size(); i++) {
                if (options[i].second == value) return static_cast<int>(i);
            }
            return 0;
        }
    }

    void PageBuilder::addSection(const std::string& text) { addControl(makeSection(m_host, text)); }
    void PageBuilder::addText(const std::string& text, bool center) { addControl(makeText(m_host, text, 14.f, center, false)); }
    void PageBuilder::addPadding(float heightDp) { addControl(makeSpacer(m_host, heightDp)); }

    void PageBuilder::addRadioRow(const std::vector<std::string>& labels, std::function<int()> getCurrent, std::function<int(int)> onSelect) {
        addControl(makeChoice(m_host, "", labels, std::move(getCurrent), std::move(onSelect)));
    }

    void PageBuilder::addSelector(const std::string& label, std::function<std::string()> getText, std::function<void(std::function<void()>)> onOpen) {
        addControl(makeSelector(m_host, label, std::move(getText), std::move(onOpen)));
    }

    void PageBuilder::addButtons(std::vector<std::pair<std::string, std::function<void()>>> buttons, bool danger) {
        addControl(makeButtons(m_host, std::move(buttons), danger));
    }

    void PageBuilder::addNode(cocos2d::CCNode* node, float heightDp) { addControl(makeNodeRow(m_host, node, heightDp)); }

    void PageBuilder::addLink(const std::string& title, std::function<std::string()> subtitle, std::function<void()> tap) {
        addControl(makeLink(m_host, title, std::move(subtitle), std::move(tap)));
    }

    void PageBuilder::addKeybind(const std::string& label, std::function<std::string()> get, std::function<void()> onSet, std::function<void()> onClear) {
        addControl(makeKeybind(m_host, label, std::move(get), std::move(onSet), std::move(onClear)));
    }

    void PageBuilder::addToggle(const std::string& label, std::function<bool()> get, std::function<void(bool)> set) {
        addControl(makeToggle(m_host, label, std::move(get), std::move(set)));
    }

    void PageBuilder::addButtonRow(const std::string& label, geode::Function<void()> callback, const std::string& secondLabel, geode::Function<void()> secondCallback) {
        auto first = share<>(std::move(callback));
        std::vector<std::pair<std::string, std::function<void()>>> buttons;
        buttons.emplace_back(label, first);
        if (!secondLabel.empty() && secondCallback != nullptr) {
            buttons.emplace_back(secondLabel, share<>(std::move(secondCallback)));
        }
        addControl(makeButtons(m_host, std::move(buttons), false));
    }

    void PageBuilder::addConfigToggle(const std::string& label, const std::string& key, bool defaultValue, geode::Function<void(bool)> callback) {
        auto cb = share<bool>(std::move(callback));
        addControl(makeToggle(m_host, label,
            [key, defaultValue] { return NXRConfig::get().get<bool>(key, defaultValue); },
            [key, cb](bool value) {
                NXRConfig::get().set<bool>(key, value);
                cb(value);
            }));
    }

    void PageBuilder::addConfigModeToggle(const std::string& key, const std::string& offText, const std::string& onText, int defaultValue, geode::Function<void(int)> callback) {
        addConfigSelect("Mode", key, {{offText, 1}, {onText, 2}}, defaultValue, std::move(callback));
    }

    void PageBuilder::addConfigSelect(const std::string& label, const std::string& key, const std::vector<std::pair<std::string, int>>& options, int defaultValue, geode::Function<void(int)> callback) {
        if (options.empty()) return;
        auto cb = share<int>(std::move(callback));
        std::vector<std::string> names;
        for (const auto& option : options) names.push_back(option.first);
        addControl(makeChoice(m_host, label, names,
            [key, options, defaultValue] { return indexOfValue(options, NXRConfig::get().get<int>(key, defaultValue)); },
            [key, options, cb](int index) {
                index = std::clamp(index, 0, static_cast<int>(options.size()) - 1);
                const int value = options[static_cast<size_t>(index)].second;
                NXRConfig::get().set<int>(key, value);
                cb(value);
                return index;
            }));
    }

    void PageBuilder::addConfigRadio(const std::string& label, const std::string& key, const std::vector<std::pair<std::string, int>>& options, int defaultValue, geode::Function<void(int)> callback) {
        addConfigSelect(label, key, options, defaultValue, std::move(callback));
    }

    void PageBuilder::addConfigIntInput(const std::string& label, const std::string& key, int min, int max, int defaultValue, geode::Function<void(int)> callback) {
        auto cb = share<int>(std::move(callback));
        addControl(makeNumberRow(m_host, label, true, static_cast<float>(min), static_cast<float>(max),
            [key, defaultValue] { return static_cast<float>(NXRConfig::get().get<int>(key, defaultValue)); },
            [key, min, max, cb](float value) {
                const int rounded = std::clamp(static_cast<int>(std::lround(value)), min, max);
                NXRConfig::get().set<int>(key, rounded);
                cb(rounded);
            }));
    }

    void PageBuilder::addConfigFloatInput(const std::string& label, const std::string& key, float min, float max, float defaultValue, geode::Function<void(float)> callback) {
        auto cb = share<float>(std::move(callback));
        addControl(makeNumberRow(m_host, label, false, min, max,
            [key, defaultValue] { return NXRConfig::get().get<float>(key, defaultValue); },
            [key, min, max, cb](float value) {
                const float clamped = std::clamp(value, min, max);
                NXRConfig::get().set<float>(key, clamped);
                cb(clamped);
            }));
    }

    void PageBuilder::addConfigColor3Hex(const std::string& label, const std::string& key, const std::string& defaultHex) {
        addConfigColor(label, key, defaultHex, false, "");
    }

    void PageBuilder::addConfigColor4Hex(const std::string& label, const std::string& key, const std::string& defaultHex) {
        addConfigColor(label, key, defaultHex, true, "");
    }

    void PageBuilder::addConfigChoice(const std::string& label, const std::string& key, const std::vector<std::string>& options, int defaultValue, geode::Function<void(int)> callback) {
        if (options.empty()) return;
        auto cb = share<int>(std::move(callback));
        const int count = static_cast<int>(options.size());
        addControl(makeChoice(m_host, label, options,
            [key, defaultValue, count] { return std::clamp(NXRConfig::get().get<int>(key, defaultValue), 0, count - 1); },
            [key, count, cb](int index) {
                index = std::clamp(index, 0, count - 1);
                NXRConfig::get().set<int>(key, index);
                cb(index);
                return index;
            }));
    }

    void PageBuilder::addConfigSlider(const std::string& label, const std::string& key, float min, float max, float defaultValue, float step, NXR::SliderScale scale, const std::vector<NXR::SliderPreset>& presets, geode::Function<void(float)> callback, bool integer, const std::string& suffix) {
        SliderSpec spec;
        spec.label = label;
        spec.key = key;
        spec.suffix = suffix;
        spec.min = min;
        spec.max = max;
        spec.def = defaultValue;
        spec.step = step;
        spec.scale = scale;
        spec.presets = presets;
        spec.integer = integer;
        spec.callback = share<float>(std::move(callback));
        addControl(makeSlider(m_host, std::move(spec)));
    }

    void PageBuilder::addConfigColor(const std::string& label, const std::string& key, const std::string& defaultHex, bool alpha, const std::string& rainbowKey) {
        ColorSpec spec;
        spec.label = label;
        spec.key = key;
        spec.defaultHex = defaultHex;
        spec.alpha = alpha;
        spec.rainbowKey = rainbowKey;
        addControl(makeColorRow(m_host, std::move(spec)));
    }

    void PageBuilder::addBoundToggle(const std::string& label, bool current, geode::Function<void(bool)> setter) {
        auto state = std::make_shared<bool>(current);
        auto cb = share<bool>(std::move(setter));
        addControl(makeToggle(m_host, label,
            [state] { return *state; },
            [state, cb](bool value) {
                *state = value;
                cb(value);
            }));
    }

    void PageBuilder::addSeparator(float) {
        addControl(makeSeparator(m_host, 14.f));
    }

    void PageBuilder::requestRebuild() {
        if (m_rebuild) m_rebuild();
    }
}
