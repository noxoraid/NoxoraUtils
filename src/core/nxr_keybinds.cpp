#include "nxr_keybinds.hpp"
#include "nxr_gui.hpp"
#include "nxr_config.hpp"

using namespace geode::prelude;
using namespace NXR;

void Keybinds::init() {
    load();
    rebuildCache();

    auto listener = geode::KeyboardInputEvent().listen([this](geode::KeyboardInputData& event) {
        if (event.action == geode::KeyboardInputData::Action::Press ||
            event.action == geode::KeyboardInputData::Action::Repeat) {

            if (event.action == geode::KeyboardInputData::Action::Press) {
                m_pressedKeys.insert(event.key);
            }

            if (!m_recordingWindow.empty() || !m_recordingCustomId.empty()) {
                if (event.action == geode::KeyboardInputData::Action::Repeat) return ListenerResult::Stop;

                if (event.key == cocos2d::KEY_LeftControl || event.key == cocos2d::KEY_RightControl ||
                    event.key == cocos2d::KEY_LeftShift || event.key == cocos2d::KEY_RightShift ||
                    event.key == cocos2d::KEY_Alt) {
                    return ListenerResult::Stop;
                }

                auto bind = (event.key == cocos2d::KEY_Escape) ? geode::Keybind() : geode::Keybind(event.key, event.modifiers);

                if (!m_recordingCustomId.empty()) {
                    changeBind(m_recordingCustomId, bind);
                } else {
                    Gui::get().getWindow(m_recordingWindow).findHackByName(m_recordingHack).setKeybind(bind);
                }

                stopRecording();
                save();
                rebuildCache();
                return ListenerResult::Stop;
            }

            geode::Keybind pressedBind(event.key, event.modifiers);
            bool handled = false;

            for (auto& [id, customPair] : m_customBinds) {
                if (customPair.first == pressedBind && customPair.second) {
                    bool isRepeat = (event.action == geode::KeyboardInputData::Action::Repeat);
                    customPair.second(isRepeat);
                    handled = true;
                }
            }

            if (event.action == geode::KeyboardInputData::Action::Press && m_bindsMap.contains(pressedBind)) {
                for (const auto& [windowName, hackName] : m_bindsMap[pressedBind]) {
                    Gui::get().getWindow(windowName).findHackByName(hackName).toggle();
                    handled = true;
                }
            }

            if (handled) {
                return ListenerResult::Stop;
            }
        }
        else if (event.action == geode::KeyboardInputData::Action::Release) {
            m_pressedKeys.erase(event.key);

            if (!m_recordingWindow.empty() || !m_recordingCustomId.empty()) {
                return ListenerResult::Stop;
            }
        }

        return ListenerResult::Propagate;
    });
    listener.leak();

    auto mouseListener = geode::MouseInputEvent().listen([this](geode::MouseInputData& event) {
        if (event.action == geode::MouseInputData::Action::Press) {
            m_pressedMouseButtons.insert(event.button);
        } else if (event.action == geode::MouseInputData::Action::Release) {
            m_pressedMouseButtons.erase(event.button);
        }
        return ListenerResult::Propagate;
    });
    mouseListener.leak();
}

void Keybinds::rebuildCache() {
    m_bindsMap.clear();
    for (auto& window : Gui::get().getWindows()) {
        for (auto& hack : window.getHacks()) {
            if (hack.getKeybind().key != cocos2d::KEY_None) {
                m_bindsMap[hack.getKeybind()].push_back({window.getName(), hack.getName()});
            }
        }
    }
}

void Keybinds::load() {
    std::ifstream file(getKeybindsDataPath());
    if (!file.is_open()) return;

    nlohmann::json jsonObject = nlohmann::json::parse(file, nullptr, false);
    if (jsonObject.is_discarded()) return;

    if (jsonObject.contains("_custom_actions") && jsonObject["_custom_actions"].is_object()) {
        for (const auto& [id, bindValue] : jsonObject["_custom_actions"].items()) {
            if (bindValue.is_object() && bindValue.contains("key") && bindValue.contains("modifiers")) {
                auto key = static_cast<cocos2d::enumKeyCodes>(bindValue["key"].get<int>());
                auto modifiers = static_cast<geode::KeyboardModifier>(bindValue["modifiers"].get<int>());
                m_customBinds[id].first = geode::Keybind(key, modifiers);
            }
        }
    }

    for (const auto& [jsonKey, bindValue] : jsonObject.items()) {
        if (jsonKey == "_custom_actions" || !bindValue.is_object()) continue;
        size_t delimiter = jsonKey.find("::");
        if (delimiter == std::string::npos) continue;

        if (bindValue.contains("key") && bindValue.contains("modifiers")) {
            auto key = static_cast<cocos2d::enumKeyCodes>(bindValue["key"].get<int>());
            auto modifiers = static_cast<geode::KeyboardModifier>(bindValue["modifiers"].get<int>());

            std::string windowName = jsonKey.substr(0, delimiter);
            std::string hackName = jsonKey.substr(delimiter + 2);
            Gui::get().getWindow(windowName).findHackByName(hackName).setKeybind(geode::Keybind(key, modifiers));
        }
    }
}

void Keybinds::save() const {
    std::ofstream file(getKeybindsDataPath());
    if (!file.is_open()) return;

    nlohmann::json jsonObject = nlohmann::json::object();

    for (auto& window : Gui::get().getWindows()) {
        for (auto& hack : window.getHacks()) {
            auto bind = hack.getKeybind();
            if (bind.key != cocos2d::KEY_None) {
                nlohmann::json bindJson;
                bindJson["key"] = static_cast<int>(bind.key);
                bindJson["modifiers"] = static_cast<int>(bind.modifiers);
                jsonObject[fmt::format("{}::{}", window.getName(), hack.getName())] = bindJson;
            }
        }
    }

    nlohmann::json customJson = nlohmann::json::object();
    for (const auto& [id, customPair] : m_customBinds) {
        auto bind = customPair.first;
        nlohmann::json bindJson;
        bindJson["key"] = static_cast<int>(bind.key);
        bindJson["modifiers"] = static_cast<int>(bind.modifiers);
        customJson[id] = bindJson;
    }
    jsonObject["_custom_actions"] = customJson;

    file << jsonObject.dump(4);
}

void Keybinds::startRecording(const std::string& windowName, const std::string& hackName) {
    stopRecording();
    m_recordingWindow = windowName;
    m_recordingHack = hackName;
}

void Keybinds::startRecordingCustom(const std::string& id) {
    stopRecording();
    m_recordingCustomId = id;
}

bool Keybinds::isRecording(const std::string& windowName, const std::string& hackName) const {
    return m_recordingWindow == windowName && m_recordingHack == hackName;
}

bool Keybinds::isRecordingCustom(const std::string& id) const {
    return m_recordingCustomId == id;
}

void Keybinds::stopRecording() {
    m_recordingWindow = "";
    m_recordingHack = "";
    m_recordingCustomId = "";
}

bool Keybinds::isKeyDown(cocos2d::enumKeyCodes key) const {
    return m_pressedKeys.contains(key);
}

bool Keybinds::isMouseButtonDown(geode::MouseInputData::Button button) const {
    return m_pressedMouseButtons.contains(button);
}

void Keybinds::addCallback(const std::string& id, geode::Keybind defaultBind, geode::Function<void(bool)> callback) {
    if (m_customBinds.contains(id)) {
        m_customBinds[id].second = std::move(callback);
    } else {
        m_customBinds[id] = {defaultBind, std::move(callback)};
    }
}

void Keybinds::registerAction(const std::string& id, const std::string& label, geode::Keybind defaultBind, geode::Function<void(bool)> callback) {
    addCallback(id, defaultBind, std::move(callback));

    for (auto& entry : m_customList) {
        if (entry.first == id) {
            entry.second = label;
            return;
        }
    }
    m_customList.push_back({id, label});
}

std::vector<std::pair<std::string, std::string>> Keybinds::customActions() const {
    return m_customList;
}

bool Keybinds::hasCustom(const std::string& id) const {
    return m_customBinds.contains(id);
}

void Keybinds::clearHackBind(const std::string& windowName, const std::string& hackName) {
    Gui::get().getWindow(windowName).findHackByName(hackName).setKeybind(geode::Keybind());
    save();
    rebuildCache();
}

void Keybinds::clearCustomBind(const std::string& id) {
    changeBind(id, geode::Keybind());
    save();
}

void Keybinds::clearCallback(const std::string& id) {
    if (m_customBinds.contains(id)) {
        m_customBinds[id].second = nullptr;
    }
}

void Keybinds::changeBind(const std::string& id, geode::Keybind newBind) {
    m_customBinds[id].first = newBind;
}

geode::Keybind Keybinds::getBind(const std::string& id) const {
    if (m_customBinds.contains(id)) {
        return m_customBinds.at(id).first;
    }
    return geode::Keybind();
}

void Keybinds::removeCallback(const std::string& id) {
    if (m_customBinds.contains(id)) {
        m_customBinds.erase(id);
    }
}

#ifdef GEODE_IS_WINDOWS
#include <Windows.h>

int Keybinds::convertToWinAPI(cocos2d::enumKeyCodes cocosKey) {
    switch (cocosKey) {
        case KEY_Unknown:         return 0;
        case KEY_None:            return 0;
        case KEY_Backspace:       return VK_BACK;
        case KEY_Tab:             return VK_TAB;
        case KEY_Clear:           return VK_CLEAR;
        case KEY_Enter:           return VK_RETURN;
        case KEY_Shift:           return VK_SHIFT;
        case KEY_Control:         return VK_CONTROL;
        case KEY_Alt:             return VK_MENU;
        case KEY_Pause:           return VK_PAUSE;
        case KEY_CapsLock:        return VK_CAPITAL;
        case KEY_Escape:          return VK_ESCAPE;
        case KEY_Space:           return VK_SPACE;

        case KEY_PageUp:          return VK_PRIOR;
        case KEY_PageDown:        return VK_NEXT;
        case KEY_End:             return VK_END;
        case KEY_Home:            return VK_HOME;
        case KEY_Left:            return VK_LEFT;
        case KEY_Up:              return VK_UP;
        case KEY_Right:           return VK_RIGHT;
        case KEY_Down:            return VK_DOWN;

        case KEY_ArrowUp:         return VK_UP;
        case KEY_ArrowDown:       return VK_DOWN;
        case KEY_ArrowLeft:       return VK_LEFT;
        case KEY_ArrowRight:      return VK_RIGHT;

        case KEY_Select:          return VK_SELECT;
        case KEY_Print:           return VK_PRINT;
        case KEY_Execute:         return VK_EXECUTE;
        case KEY_PrintScreen:     return VK_SNAPSHOT;
        case KEY_Insert:          return VK_INSERT;
        case KEY_Delete:          return VK_DELETE;
        case KEY_Help:            return VK_HELP;

        case KEY_Zero:            return '0';
        case KEY_One:             return '1';
        case KEY_Two:             return '2';
        case KEY_Three:           return '3';
        case KEY_Four:            return '4';
        case KEY_Five:            return '5';
        case KEY_Six:             return '6';
        case KEY_Seven:           return '7';
        case KEY_Eight:           return '8';
        case KEY_Nine:            return '9';

        case KEY_A:               return 'A';
        case KEY_B:               return 'B';
        case KEY_C:               return 'C';
        case KEY_D:               return 'D';
        case KEY_E:               return 'E';
        case KEY_F:               return 'F';
        case KEY_G:               return 'G';
        case KEY_H:               return 'H';
        case KEY_I:               return 'I';
        case KEY_J:               return 'J';
        case KEY_K:               return 'K';
        case KEY_L:               return 'L';
        case KEY_M:               return 'M';
        case KEY_N:               return 'N';
        case KEY_O:               return 'O';
        case KEY_P:               return 'P';
        case KEY_Q:               return 'Q';
        case KEY_R:               return 'R';
        case KEY_S:               return 'S';
        case KEY_T:               return 'T';
        case KEY_U:               return 'U';
        case KEY_V:               return 'V';
        case KEY_W:               return 'W';
        case KEY_X:               return 'X';
        case KEY_Y:               return 'Y';
        case KEY_Z:               return 'Z';

        case KEY_LeftWindowsKey:  return VK_LWIN;
        case KEY_RightWindowsKey: return VK_RWIN;
        case KEY_ApplicationsKey: return VK_APPS;
        case KEY_Sleep:           return VK_SLEEP;

        case KEY_NumPad0:         return VK_NUMPAD0;
        case KEY_NumPad1:         return VK_NUMPAD1;
        case KEY_NumPad2:         return VK_NUMPAD2;
        case KEY_NumPad3:         return VK_NUMPAD3;
        case KEY_NumPad4:         return VK_NUMPAD4;
        case KEY_NumPad5:         return VK_NUMPAD5;
        case KEY_NumPad6:         return VK_NUMPAD6;
        case KEY_NumPad7:         return VK_NUMPAD7;
        case KEY_NumPad8:         return VK_NUMPAD8;
        case KEY_NumPad9:         return VK_NUMPAD9;
        case KEY_Multiply:        return VK_MULTIPLY;
        case KEY_Add:             return VK_ADD;
        case KEY_Seperator:       return VK_SEPARATOR;
        case KEY_Subtract:        return VK_SUBTRACT;
        case KEY_Decimal:         return VK_DECIMAL;
        case KEY_Divide:          return VK_DIVIDE;

        case KEY_F1:              return VK_F1;
        case KEY_F2:              return VK_F2;
        case KEY_F3:              return VK_F3;
        case KEY_F4:              return VK_F4;
        case KEY_F5:              return VK_F5;
        case KEY_F6:              return VK_F6;
        case KEY_F7:              return VK_F7;
        case KEY_F8:              return VK_F8;
        case KEY_F9:              return VK_F9;
        case KEY_F10:             return VK_F10;
        case KEY_F11:             return VK_F11;
        case KEY_F12:             return VK_F12;
        case KEY_F13:             return VK_F13;
        case KEY_F14:             return VK_F14;
        case KEY_F15:             return VK_F15;
        case KEY_F16:             return VK_F16;
        case KEY_F17:             return VK_F17;
        case KEY_F18:             return VK_F18;
        case KEY_F19:             return VK_F19;
        case KEY_F20:             return VK_F20;
        case KEY_F21:             return VK_F21;
        case KEY_F22:             return VK_F22;
        case KEY_F23:             return VK_F23;
        case KEY_F24:             return VK_F24;

        case KEY_Numlock:         return VK_NUMLOCK;
        case KEY_ScrollLock:      return VK_SCROLL;
        case KEY_LeftShift:       return VK_LSHIFT;
        case KEY_RightShift:      return VK_RSHIFT;
        case KEY_LeftControl:     return VK_LCONTROL;
        case KEY_RightControl:    return VK_RCONTROL;
        case KEY_LeftMenu:        return VK_LMENU;
        case KEY_RightMenu:       return VK_RMENU;

        case KEY_BrowserBack:      return VK_BROWSER_BACK;
        case KEY_BrowserForward:   return VK_BROWSER_FORWARD;
        case KEY_BrowserRefresh:   return VK_BROWSER_REFRESH;
        case KEY_BrowserStop:      return VK_BROWSER_STOP;
        case KEY_BrowserSearch:    return VK_BROWSER_SEARCH;
        case KEY_BrowserFavorites: return VK_BROWSER_FAVORITES;
        case KEY_BrowserHome:      return VK_BROWSER_HOME;
        case KEY_VolumeMute:       return VK_VOLUME_MUTE;
        case KEY_VolumeDown:       return VK_VOLUME_DOWN;
        case KEY_VolumeUp:         return VK_VOLUME_UP;
        case KEY_NextTrack:        return VK_MEDIA_NEXT_TRACK;
        case KEY_PreviousTrack:    return VK_MEDIA_PREV_TRACK;
        case KEY_StopMedia:        return VK_MEDIA_STOP;
        case KEY_PlayPause:        return VK_MEDIA_PLAY_PAUSE;
        case KEY_LaunchMail:       return VK_LAUNCH_MAIL;
        case KEY_SelectMedia:      return VK_LAUNCH_MEDIA_SELECT;
        case KEY_LaunchApp1:       return VK_LAUNCH_APP1;
        case KEY_LaunchApp2:       return VK_LAUNCH_APP2;

        case KEY_OEM1:            return VK_OEM_1;
        case KEY_OEMPlus:         return VK_OEM_PLUS;
        case KEY_OEMComma:        return VK_OEM_COMMA;
        case KEY_OEMMinus:        return VK_OEM_MINUS;
        case KEY_OEMPeriod:       return VK_OEM_PERIOD;
        case KEY_OEM2:            return VK_OEM_2;
        case KEY_OEM3:            return VK_OEM_3;
        case KEY_OEM4:            return VK_OEM_4;
        case KEY_OEM5:            return VK_OEM_5;
        case KEY_OEM6:            return VK_OEM_6;
        case KEY_OEM7:            return VK_OEM_7;
        case KEY_OEM8:            return VK_OEM_8;
        case KEY_OEM102:          return VK_OEM_102;

        case KEY_Process:         return VK_PROCESSKEY;
        case KEY_Packet:          return VK_PACKET;
        case KEY_Attn:            return 0;
        case KEY_CrSel:           return VK_CRSEL;
        case KEY_ExSel:           return VK_EXSEL;
        case KEY_EraseEOF:        return VK_EREOF;
        case KEY_Play:            return VK_PLAY;
        case KEY_Zoom:            return VK_ZOOM;
        case KEY_PA1:             return VK_PA1;
        case KEY_OEMClear:        return VK_OEM_CLEAR;

        case KEY_GraveAccent:     return VK_OEM_3;
        case KEY_OEMEqual:        return VK_OEM_PLUS;
        case KEY_LeftBracket:     return VK_OEM_4;
        case KEY_RightBracket:    return VK_OEM_6;
        case KEY_Backslash:       return VK_OEM_5;
        case KEY_Semicolon:       return VK_OEM_1;
        case KEY_Apostrophe:      return VK_OEM_7;
        case KEY_Slash:           return VK_OEM_2;
        case KEY_Equal:           return VK_OEM_PLUS;
        case KEY_NumEnter:        return VK_RETURN;
        case KEY_World1:          return 0;
        case KEY_World2:          return 0;

        case MOUSE_4:             return VK_XBUTTON1;
        case MOUSE_5:             return VK_XBUTTON2;
        case MOUSE_6:             return 0;
        case MOUSE_7:             return 0;
        case MOUSE_8:             return 0;

        case CONTROLLER_A:        return 0x58;
        case CONTROLLER_B:        return 0x59;
        case CONTROLLER_Y:        return 0x5B;
        case CONTROLLER_X:        return 0x5A;

        case CONTROLLER_Start:
        case CONTROLLER_Back:
        case CONTROLLER_RB:
        case CONTROLLER_LB:
        case CONTROLLER_RT:
        case CONTROLLER_LT:
        case CONTROLLER_Up:
        case CONTROLLER_Down:
        case CONTROLLER_Left:
        case CONTROLLER_Right:
        case CONTROLLER_LTHUMBSTICK_UP:
        case CONTROLLER_LTHUMBSTICK_DOWN:
        case CONTROLLER_LTHUMBSTICK_LEFT:
        case CONTROLLER_LTHUMBSTICK_RIGHT:
        case CONTROLLER_RTHUMBSTICK_UP:
        case CONTROLLER_RTHUMBSTICK_DOWN:
        case CONTROLLER_RTHUMBSTICK_LEFT:
        case CONTROLLER_RTHUMBSTICK_RIGHT:
        default:
            return 0;
    }
}
#endif
