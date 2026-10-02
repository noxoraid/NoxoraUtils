#include "../../core/nxr_ui_mode.hpp"
#include "nxr_hacks_layer.hpp"
#include "nxr_table_layer.hpp"

bool NXR::Ui::menuOpen() {
    return NXRHacksLayer::isOpened() || NXRTableLayer::isOpened();
}

void NXR::Ui::closeMenu() {
    if (NXRHacksLayer::isOpened()) NXRHacksLayer::get()->onClose(nullptr);
    if (NXRTableLayer::isOpened()) NXRTableLayer::close();
}

void NXR::Ui::openMenu() {
    if (menuOpen()) return;

    if (layout() == Table) NXRTableLayer::open();
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

void NXR::Ui::showPopup(geode::Popup* popup, const std::string& title) {
    if (!popup) return;
    if (NXRTableLayer::isOpened() && NXRTableLayer::hostPopup(popup, title)) return;
    popup->show();
}

void NXR::Ui::showChoice(const std::string& title, const std::vector<std::string>& notes, const std::vector<std::pair<std::string, std::function<void()>>>& choices) {
    if (NXRTableLayer::isOpened()) {
        NXRTableLayer::openChoice(title, notes, choices);
        return;
    }
    if (choices.size() < 2) return;
    std::string body;
    for (auto& note : notes) body += note + "\n";
    auto first = choices[0].second;
    auto second = choices[1].second;
    geode::createQuickPopup(title.c_str(), body, choices[0].first.c_str(), choices[1].first.c_str(), [first, second](auto*, bool secondPicked) {
        if (secondPicked) second();
        else first();
    });
}
