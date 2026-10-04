#include "nxr_hack_settings_popup.hpp"
#include "nxr_hacks_layer.hpp"
#include "../imgui/nxr_imgui_menu.hpp"
#include "../../core/nxr_ui_mode.hpp"

void NXRHackSettingsPopup::open(NXR::Hack& hack, const std::string&) {
    if (NXR::Ui::layout() == NXR::Ui::Table) {
        NXR::Imgui::openHackSettings(hack);
        return;
    }
    NXRHacksLayer::openHack(hack);
}
