#include "nxr_gui.hpp"
#include "nxr_utils.hpp"
#include "nxr_keybinds.hpp"
#include "nxr_config.hpp"

using namespace NXR;

FLAlertLayer* NXR::MaterialLayer(FLAlertLayer* falert) {
    if (falert == nullptr) return nullptr;

    if (auto* sprite9 = falert->m_mainLayer->getChildByType<cocos2d::extension::CCScale9Sprite>(0)) {
        auto* frame = cocos2d::CCSpriteFrame::create("NXR_square.png"_spr, {0, 0, 0, 0});
        auto size = sprite9->getContentSize();
        sprite9->setSpriteFrame(frame);
        sprite9->setContentSize(size);
    }

    if (auto* label = falert->m_mainLayer->getChildByType<cocos2d::CCLabelBMFont>(0)) {
        label->setFntFile("GoogleSans.fnt"_spr);
        label->setColor({139, 124, 246});
    }

    if (falert->m_button1 != nullptr) {
        falert->m_button1->updateBGImage("NXR_button_01.png"_spr);
        falert->m_button1->m_label->setColor(ccColor3B({255, 255, 255}));
        falert->m_button1->m_label->setFntFile("GoogleSans.fnt"_spr);
    }

    if (falert->m_button2 != nullptr) {
        falert->m_button2->updateBGImage("NXR_button_01.png"_spr);
        falert->m_button2->m_label->setColor(ccColor3B({255, 255, 255}));
        falert->m_button2->m_label->setFntFile("GoogleSans.fnt"_spr);
    }

    return falert;
}

Window& Gui::getWindow(const std::string& name) {
    auto it = std::find_if(
        m_windows.begin(),
        m_windows.end(),
        [&name](const Window& w) {
            return w.getName() == name;
        }
    );

    if (it != m_windows.end())
        return *it;

    return m_windows.emplace_back(name);
}

void Window::sortHacksAlphabetically() {
    std::sort(m_hacks.begin(), m_hacks.end(), [](const Hack& a, const Hack& b) {
        return a.getName() < b.getName();
    });
}

void Gui::sortAllHacksAlphabetically() {
    for (auto& window : m_windows) {
        window.sortHacksAlphabetically();
    }

    std::sort(m_windows.begin(), m_windows.end(), [](const Window& a, const Window& b) {
        return a.getName() < b.getName();
    });
}

std::string Window::formatID(const std::string& windowName, const std::string& hackName) {
    std::string config = fmt::format("{}.{}",
        Utils::String::replaceChar(
            Utils::String::toLowerCase(windowName),
            ' ', '_'
        ),
        Utils::String::replaceChar(
            Utils::String::toLowerCase(hackName),
            ' ', '_'
        )
    );

    return config;
}

Hack& Window::createHack(
    const std::string& name,
    const std::string& desc,
    bool cheating
) {
    auto it = std::find_if(
        m_hacks.begin(),
        m_hacks.end(),
        [&name](const Hack& h) {
            return h.getName() == name;
        }
    );

    if (it != m_hacks.end()) {
        it->setDesc(desc);
        it->setCheating(cheating);

        return *it;
    }

    std::string config = "nxr." + formatID(this->m_name, name);
    return m_hacks.emplace_back(config, name, desc, cheating);
}

Hack& Window::findHackByName(const std::string& name) {
    auto it = std::find_if(
        m_hacks.begin(),
        m_hacks.end(),
        [&name](const Hack& h) {
            return h.getName() == name;
        }
    );

    if (it != m_hacks.end())
        return *it;

    std::string config = "nxr." + formatID(this->m_name, name);
    return m_hacks.emplace_back(config, name, "", false);
}

Hack* Window::findHackByID(const std::string& ID) {
    auto it = std::find_if(
        m_hacks.begin(),
        m_hacks.end(),
        [&ID](const Hack& h) {
            return h.getID() == ID;
        }
    );

    return it != m_hacks.end() ? &(*it) : nullptr;
}

Hack* Gui::findHackByIDGlobal(const std::string& ID) {
    for (auto& window : m_windows) {
        auto& hacks = window.getHacks();
        auto it = std::find_if(hacks.begin(), hacks.end(), [&ID](const Hack& h) {
            return h.getID() == ID;
        });

        if (it != hacks.end()) {
            return &(*it);
        }
    }
    return nullptr;
}

void Gui::rescanActiveCheats() {
    m_activeCheats.clear();
    for (auto& window : m_windows) {
        for (auto& hack : window.getHacks()) {
            if (hack.getEnabled() && hack.isCheating()) {
                m_activeCheats.insert(hack.getName());
            }
        }
    }
}

void Gui::disableCheats() {
    for (auto& window : m_windows) {
        for (auto& hack : window.getHacks()) {
            if (hack.isCheating() && hack.getEnabled()) {
                hack.disable();
            }
        }
    }
    m_activeCheats.clear();
}

#include <Geode/modify/MenuLayer.hpp>
class $modify(NXRLateHacksInitMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

		static bool inited = false;
        if (!inited) {
            inited = true;

            NXR::Utils::checkModUpdate([](bool needUpdate) {
                NXRConfig::get().set<bool>("nxr.ui.need_update", needUpdate);
            });

            auto& gui = Gui::get();

            gui.sortAllHacksAlphabetically();

            auto& windows = gui.getWindows();
            size_t totalHacks = 0;
            for (auto& window : windows) {
                for (auto& hack : window.getHacks()) {
                    if (hack.getEnabled() && !hack.getEarlyInit()) {
                        hack.callHandler(true);
                    }

                    if (hack.getEnabled() && hack.isCheating()) {
                        Gui::get().getActiveCheats().insert(hack.getName());
                    }

                    totalHacks++;
                }
            }

            geode::log::debug("total loaded: {} hacks", totalHacks);

            #ifdef GEODE_IS_DESKTOP
            auto& kb = NXR::Keybinds::get();
            kb.init();
            #endif
        }

		return true;
    }
};
