#pragma once
#include "nxr_hacks.hpp"

namespace NXR {
    FLAlertLayer* MaterialLayer(FLAlertLayer* falert);

    class Window {
    public:
        explicit Window(std::string name)
            : m_name(std::move(name)) {}

        const std::string& getName() const { return m_name; }

        Hack& createHack(const std::string& name, const std::string& desc, bool cheating);

        Hack& findHackByName(const std::string& name);
        Hack* findHackByID(const std::string& ID);

        std::vector<Hack>& getHacks() { return m_hacks; }

        void setCustomWindowCocos(geode::Function<void(cocos2d::CCNode*)> func) { m_handlerCocos = std::move(func); };

        void callCustomWindowCocos(cocos2d::CCNode* tab) { if (m_handlerCocos) m_handlerCocos(tab); };

        bool avaibleCustomWindowCocos() { return m_handlerCocos != nullptr;  };

        void setImguiPanel(geode::Function<void()> func) { m_panelImgui = std::move(func); }

        bool hasImguiPanel() const { return m_panelImgui != nullptr; }

        void drawImguiPanel() { if (m_panelImgui) m_panelImgui(); }

        std::string formatID(const std::string& windowName, const std::string& hackName);

        void sortHacksAlphabetically();
    private:
        std::string m_name;
        std::vector<Hack> m_hacks;

        geode::Function<void(cocos2d::CCNode*)> m_handlerCocos = nullptr;
        geode::Function<void()> m_panelImgui = nullptr;
    };

    class Gui {
    public:
        static Gui& get() {
            static Gui instance;
            return instance;
        }

        Gui& operator=(const Gui&) = delete;
        Gui(const Gui&) = delete;

        Window& getWindow(const std::string& name);
        std::vector<Window>& getWindows() { return m_windows; };

        Hack* findHackByIDGlobal(const std::string& ID);

        void rescanActiveCheats();
        std::unordered_set<std::string>& getActiveCheats() { return m_activeCheats; }

        void disableCheats();

        void sortAllHacksAlphabetically();
    private:
        Gui() = default;

        std::vector<Window> m_windows;
        std::unordered_set<std::string> m_activeCheats;
    };
}

#define NXR_HACK_CREATE(window_name, name, desc, cheating) \
$execute \
{ \
    auto& win = NXR::Gui::get().getWindow(window_name); \
    win.createHack(name, desc, cheating); \
}
