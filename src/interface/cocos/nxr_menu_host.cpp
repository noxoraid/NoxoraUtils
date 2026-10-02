#include "../../core/nxr_ui_mode.hpp"
#include "nxr_hacks_layer.hpp"
#include "nxr_table_layer.hpp"

// One entry point for both layouts so the floating button and the keybind do not care which one is active.
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
