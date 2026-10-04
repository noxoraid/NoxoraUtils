#include "../../core/nxr_ui_mode.hpp"
#include "nxr_hacks_layer.hpp"
#include "../imgui/nxr_imgui_menu.hpp"
#include "nxr_bot_popups.hpp"

bool NXR::Ui::menuOpen() {
    return NXRHacksLayer::isOpened() || NXR::Imgui::isOpen();
}

void NXR::Ui::closeMenu() {
    if (NXRHacksLayer::isOpened()) NXRHacksLayer::get()->onClose(nullptr);
    if (NXR::Imgui::isOpen()) NXR::Imgui::close();
}

void NXR::Ui::openMenu() {
    if (menuOpen()) return;

    if (layout() == Table) NXR::Imgui::open();
    else NXRHacksLayer::get()->show();
}

void NXR::Ui::toggleMenu() {
    if (menuOpen()) closeMenu();
    else openMenu();
}

void NXR::Ui::reopenMenu() {
    closeMenu();
    geode::queueInMainThread([] { openMenu(); });
}

void NXR::Ui::showPopup(geode::Popup* popup, const std::string&) {
    if (!popup) return;
    popup->show();
    if (NXR::Imgui::isOpen()) NXR::Imgui::holdFor(popup);
}

void NXR::Ui::pickReplay(const std::string& title, const std::string& action, std::function<void(const std::string&)> onPick, bool allowClear) {
    if (NXR::Imgui::isOpen()) {
        NXR::Imgui::pickReplay(title, action, std::move(onPick), allowClear);
        return;
    }
    showPopup(NXRReplayPickerPopup::create(title, action, std::move(onPick), allowClear), title);
}

void NXR::Ui::showChoice(const std::string& title, const std::vector<std::string>& notes, const std::vector<std::pair<std::string, std::function<void()>>>& choices) {
    if (choices.size() < 2) return;
    std::string body;
    for (auto& note : notes) body += note + "\n";
    showPopup(NXRChoicePopup::create(title, body, choices), title);
}
