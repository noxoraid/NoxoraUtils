#include <Geode/Geode.hpp>
#include <string>
#include "nxr_hacks_tab.hpp"
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_keybinds.hpp"

namespace {
    std::string bindText(geode::Keybind bind) {
        if (bind.key == cocos2d::KEY_None) return "None";
        return fmt::format("{}", bind.toString());
    }
}

void NXRBuildSettingsTab(NXRHacksTab* tab) {
    auto& kb = NXR::Keybinds::get();

    tab->addPadding(4.f);
    tab->addText("Tap a key button, then press a key on your keyboard", 0.45f);
    tab->addText("Esc or Clear removes the key", 0.42f);
    tab->addSeparator();

    const auto actions = kb.customActions();
    if (!actions.empty()) {
        tab->addText("Actions", 0.6f);

        for (const auto& [id, label] : actions) {
            const std::string actionId = id;

            tab->addKeybindRow(
                label,
                [actionId]() -> std::string {
                    auto& k = NXR::Keybinds::get();
                    if (k.isRecordingCustom(actionId)) return "...";
                    return bindText(k.getBind(actionId));
                },
                [actionId]() {
                    NXR::Keybinds::get().startRecordingCustom(actionId);
                },
                [actionId]() {
                    auto& k = NXR::Keybinds::get();
                    k.stopRecording();
                    k.clearCustomBind(actionId);
                }
            );
        }

        tab->addSeparator();
    }

    for (auto& window : NXR::Gui::get().getWindows()) {
        const std::string windowName = window.getName();
        if (windowName == "Settings" || window.getHacks().empty()) continue;

        tab->addText(windowName, 0.6f);

        for (auto& hack : window.getHacks()) {
            const std::string hackName = hack.getName();

            tab->addKeybindRow(
                hackName,
                [windowName, hackName]() -> std::string {
                    auto& k = NXR::Keybinds::get();
                    if (k.isRecording(windowName, hackName)) return "...";
                    return bindText(NXR::Gui::get().getWindow(windowName).findHackByName(hackName).getKeybind());
                },
                [windowName, hackName]() {
                    NXR::Keybinds::get().startRecording(windowName, hackName);
                },
                [windowName, hackName]() {
                    auto& k = NXR::Keybinds::get();
                    k.stopRecording();
                    k.clearHackBind(windowName, hackName);
                }
            );
        }

        tab->addSeparator();
    }

    tab->addPadding(4.f);
}
