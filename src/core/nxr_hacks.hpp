#pragma once
#include <string>
#include <unordered_set>

#include <Geode/Geode.hpp>
using namespace geode::prelude;

namespace NXR {
    class Hack {
    public:
        Hack() = default;

        Hack(std::string id, std::string name, std::string desc, bool cheating)
            : m_ID(std::move(id)),
            m_name(std::move(name)),
            m_desc(std::move(desc)),
            m_cheating(cheating) {}

        void setID(const std::string& id) { m_ID = id; }
        void setName(const std::string& n) { m_name = n; }
        void setDesc(const std::string& d) { m_desc = d; }
        void setCheating(bool value) { m_cheating = value; }
        void setDisabled(bool value);
        void setHandler(geode::Function<void(bool)> func);
        void setEarlyInit(bool value);
        void addHookPtr(geode::Hook* ptr);
        void setGameVariableID(const std::string& key);

        void setCustomWindowImGui(geode::Function<void()> func) { m_handlerImGui = std::move(func); };
        void setCustomWindowCocos(geode::Function<void(cocos2d::CCNode*)> func) { m_handlerCocos = std::move(func); };

        bool getEnabled() const;
        bool getDisabled() const;
        bool getEarlyInit() const;
        void setEnabled(bool state);
        void enable();
        void disable();
        void toggle();

        const std::string& getID() const { return m_ID; }
        const std::string& getName() const { return m_name; }
        const std::string& getDesc() const { return m_desc; }
        bool isCheating();

        void setCustomCheatingCheck(geode::Function<bool()> func);

        void setKeybind(geode::Keybind const& keybind);
        geode::Keybind getKeybind() const { return m_keybind; }

        void callCustomWindowImGui() { if (m_handlerImGui) m_handlerImGui(); };
        void callCustomWindowCocos(cocos2d::CCNode* popupContent) { if (m_handlerCocos) m_handlerCocos(popupContent); };

        bool avaibleCustomWindowImGui() { return m_handlerImGui != nullptr; };
        bool avaibleCustomWindowCocos() { return m_handlerCocos != nullptr;  };

        void callHandler(bool state);
        void enableHooks(bool state);

        std::string formatID();

        std::string formatAdditionalSetting(const std::string& setting) {
            return fmt::format("{}::{}", m_ID, setting);
        }

    private:
        std::string m_ID;
        std::string m_name;
        std::string m_desc;
        bool m_cheating = false;

        std::string m_key = "";

        bool m_disabled = false;

        geode::Function<void(bool)> m_handlerFunc = nullptr;
        std::unordered_set<geode::Hook*> m_hooksPtr;
        geode::Keybind m_keybind;

        geode::Function<void()> m_handlerImGui = nullptr;
        geode::Function<void(cocos2d::CCNode*)> m_handlerCocos = nullptr;

        bool m_earlyInit = true;
        geode::Function<bool()> m_customCheatingCheck = nullptr;
    };
}
