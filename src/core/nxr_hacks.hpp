#pragma once
#include <string>
#include <unordered_set>

#include <Geode/Geode.hpp>
#include "nxr_form.hpp"
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

        void setForm(geode::Function<void(NXR::Form&)> func) { m_form = std::move(func); }

        void setSummary(geode::Function<std::string()> func) { m_summary = std::move(func); }
        bool hasSummary() { return m_summary != nullptr; }
        std::string getSummary() { return m_summary ? m_summary() : std::string(); }

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

        void callForm(NXR::Form& form) { if (m_form) m_form(form); }

        bool hasForm() { return m_form != nullptr; }

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

        geode::Function<void(NXR::Form&)> m_form = nullptr;
        geode::Function<std::string()> m_summary = nullptr;

        bool m_earlyInit = true;
        geode::Function<bool()> m_customCheatingCheck = nullptr;
    };
}
