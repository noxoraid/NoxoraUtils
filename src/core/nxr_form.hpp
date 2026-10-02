#pragma once
#include <Geode/Geode.hpp>
#include <string>
#include <utility>
#include <vector>

namespace NXR {
    class Form {
    public:
        virtual ~Form() = default;

        virtual void addConfigToggle(const std::string& label, const std::string& key, bool defaultValue = false, geode::Function<void(bool)> callback = nullptr) = 0;
        virtual void addConfigModeToggle(const std::string& key, const std::string& offText, const std::string& onText, int defaultValue = 1, geode::Function<void(int)> callback = nullptr) = 0;
        virtual void addConfigSelect(const std::string& label, const std::string& key, const std::vector<std::pair<std::string, int>>& options, int defaultValue, geode::Function<void(int)> callback = nullptr) = 0;
        virtual void addConfigRadio(const std::string& label, const std::string& key, const std::vector<std::pair<std::string, int>>& options, int defaultValue, geode::Function<void(int)> callback = nullptr) = 0;
        virtual void addConfigIntInput(const std::string& label, const std::string& key, int min, int max, int defaultValue = 0, geode::Function<void(int)> callback = nullptr) = 0;
        virtual void addConfigFloatInput(const std::string& label, const std::string& key, float min, float max, float defaultValue = 0.f, geode::Function<void(float)> callback = nullptr) = 0;
        virtual void addConfigColor3Hex(const std::string& label, const std::string& key, const std::string& defaultHex) = 0;
        virtual void addConfigColor4Hex(const std::string& label, const std::string& key, const std::string& defaultHex) = 0;
        virtual void addBoundToggle(const std::string& label, bool current, geode::Function<void(bool)> setter) = 0;
        virtual void addSeparator(float height = 1.f) = 0;
        virtual void requestRebuild() = 0;
    };
}
