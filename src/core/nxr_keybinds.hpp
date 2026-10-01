#pragma once
#include <Geode/Geode.hpp>
#include <unordered_set>
#include <vector>
#include <string>
#include <unordered_map>

namespace NXR {
    class Keybinds {
    public:
        static Keybinds& get() {
            static Keybinds instance;
            return instance;
        }

        Keybinds& operator=(const Keybinds&) = delete;
        Keybinds(const Keybinds&) = delete;

        void init();
        void rebuildCache();
        void load();
        void save() const;

        void startRecording(const std::string& windowName, const std::string& hackName);
        void startRecordingCustom(const std::string& id);

        bool isRecording(const std::string& windowName, const std::string& hackName) const;
        bool isRecordingCustom(const std::string& id) const;

        void stopRecording();

        bool isKeyDown(cocos2d::enumKeyCodes key) const;
        bool isMouseButtonDown(geode::MouseInputData::Button button) const;

        void addCallback(const std::string& id, geode::Keybind defaultBind, geode::Function<void(bool)> callback);
        void changeBind(const std::string& id, geode::Keybind newBind);
        geode::Keybind getBind(const std::string& id) const;
        void removeCallback(const std::string& id);
        void clearCallback(const std::string& id);

        void registerAction(const std::string& id, const std::string& label, geode::Keybind defaultBind, geode::Function<void(bool)> callback);
        std::vector<std::pair<std::string, std::string>> customActions() const;
        bool hasCustom(const std::string& id) const;
        void clearHackBind(const std::string& windowName, const std::string& hackName);
        void clearCustomBind(const std::string& id);

        int convertToWinAPI(cocos2d::enumKeyCodes cocosKey);

        bool m_isKeybindsMode = false;
    private:
        Keybinds() = default;

        std::unordered_map<geode::Keybind, std::vector<std::pair<std::string, std::string>>> m_bindsMap;
        std::unordered_map<std::string, std::pair<geode::Keybind, geode::Function<void(bool)>>> m_customBinds;
        std::vector<std::pair<std::string, std::string>> m_customList;

        std::string m_recordingWindow = "";
        std::string m_recordingHack = "";
        std::string m_recordingCustomId = "";

        std::unordered_set<cocos2d::enumKeyCodes> m_pressedKeys;
        std::unordered_set<geode::MouseInputData::Button> m_pressedMouseButtons;
    };
}
