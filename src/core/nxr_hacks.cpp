#include "nxr_hacks.hpp"
#include "nxr_config.hpp"
#include "nxr_keybinds.hpp"
#include "nxr_gui.hpp"

using namespace NXR;

void Hack::callHandler(bool state) {
    if (m_handlerFunc)
        m_handlerFunc(state);

    geode::log::debug("{} - handler called", m_ID);
}

bool Hack::getEnabled() const {
    return NXRConfig::get().get<bool>(m_ID, false);
}

void Hack::setDisabled(bool value) {
    if (value) disable();
    m_disabled = value;
}

bool Hack::getDisabled() const {
    return m_disabled;
}

void Hack::setGameVariableID(const std::string& key) {
    auto& config = NXRConfig::get();
    this->m_key = key;

    std::string id = this->m_ID;
    geode::queueInMainThread([id = std::move(id), key = std::move(key), &config]() {
        bool enabled = GameManager::get()->getGameVariable(key.c_str());
        config.set<bool>(id, enabled);
    });
}

void Hack::setEarlyInit(bool value) {
    m_earlyInit = value;
}

bool Hack::getEarlyInit() const {
    return m_earlyInit;
}

void Hack::setEnabled(bool state) {
    if (getEnabled() == state) return;

    auto& config = NXRConfig::get();
    config.set(m_ID, state);
    enableHooks(state);

    if (!m_key.empty())
        GameManager::get()->setGameVariable(this->m_key.c_str(), state);

    callHandler(state);

    if (this->isCheating() && this->getName() != "Auto Safe Mode") {
        auto& activeCheats = Gui::get().getActiveCheats();
        if (state) {
            activeCheats.insert(m_name);
        } else {
            activeCheats.erase(m_name);
        }
    }
}

void Hack::enable() {
    setEnabled(true);
}

void Hack::disable() {
    setEnabled(false);
}

void Hack::toggle() {
    setEnabled(!getEnabled());
}

void Hack::setHandler(geode::Function<void(bool)> func) {
    auto& config = NXRConfig::get();
    m_handlerFunc = std::move(func);

    if (m_earlyInit && config.get(m_ID, false))
        callHandler(true);
}

void Hack::addHookPtr(geode::Hook* ptr) {
    auto& config = NXRConfig::get();
    m_hooksPtr.insert(ptr);
    (void)ptr->setAutoEnable(config.get(m_ID, false));
}

void Hack::enableHooks(bool state) {
    for (geode::Hook* hookPtr : m_hooksPtr) {
        (void)hookPtr->toggle(state);
    }
}

void NXR::Hack::setKeybind(geode::Keybind const& keybind) {
    m_keybind = keybind;
    NXR::Keybinds::get().rebuildCache();
}

void Hack::setCustomCheatingCheck(geode::Function<bool()> func) {
    m_customCheatingCheck = std::move(func);
}

bool Hack::isCheating() {
    if (m_customCheatingCheck) {
        return m_customCheatingCheck();
    }
    return m_cheating;
}
