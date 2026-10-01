#include <Geode/Geode.hpp>
#include <Geode/binding/PlayerButtonCommand.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_bot.hpp"
#include "../../core/nxr_nox_utils.hpp"
#include "../../interface/imgui/nxr_widget_helper.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"
#include "imgui.h"

NXR_HACK_CREATE(
    "Level", "Nox Utils",
    "Nox Utils: auto clicker (hold / release / CPS), flip input on death, orb and pad clicks, maintain gravity, auto swift and extra clicks. "
    "Open the panel from the button under the gamemode swapper in the pause menu",
    true
);

namespace {
    constexpr const char* kWindow = "Level";
    constexpr const char* kName = "Nox Utils";
    constexpr const char* kPrefix = "nxr.nox_utils::";

    struct Opts {
        bool flipDeath = false;
        bool flipP1 = true;
        bool flipP2 = true;
        bool flipBoth = false;
        bool flipSwift = false;
        bool dashOrbs = false;
        bool blackOrbs = false;
        bool blackOrbUfo = false;
        bool jumpPads = false;
        bool gravityPads = false;
        bool maintainGravity = false;
        bool autoSwift = false;
        bool extraClicks = false;
        int extraAmount = 1;
    };

    struct Flag {
        const char* key;
        const char* label;
        bool Opts::* member;
        bool fallback;
        bool primary;
    };

    constexpr Flag kFlags[] = {
        {"flip_death", "Flip Input On Death", &Opts::flipDeath, false, true},
        {"flip_p1", "Flip: Player 1", &Opts::flipP1, true, false},
        {"flip_p2", "Flip: Player 2", &Opts::flipP2, true, false},
        {"flip_both", "Flip: Click Both", &Opts::flipBoth, false, false},
        {"flip_swift", "Flip: Swift", &Opts::flipSwift, false, false},
        {"dash_orbs", "Click Green Dash Orbs", &Opts::dashOrbs, false, true},
        {"black_orbs", "Click Black Orbs", &Opts::blackOrbs, false, true},
        {"black_orb_ufo", "Easy Black Orb UFO", &Opts::blackOrbUfo, false, true},
        {"jump_pads", "Click Jump Pads", &Opts::jumpPads, false, true},
        {"gravity_pads", "Click Gravity Pads", &Opts::gravityPads, false, true},
        {"maintain_gravity", "Maintain Gravity", &Opts::maintainGravity, false, true},
        {"auto_swift", "Auto Swift", &Opts::autoSwift, false, true},
        {"extra_clicks", "Extra Clicks", &Opts::extraClicks, false, true}
    };

    std::string keyOf(const char* name) {
        return std::string(kPrefix) + name;
    }

    Opts loadOpts() {
        Opts loaded;
        auto& config = NXRConfig::get();
        for (const auto& flag : kFlags) loaded.*(flag.member) = config.get<bool>(keyOf(flag.key), flag.fallback);
        loaded.extraAmount = std::clamp(config.get<int>(keyOf("extra_amount"), 1), 1, 50);
        return loaded;
    }

    Opts& opts() {
        static Opts instance = loadOpts();
        return instance;
    }

    const Flag* findFlag(std::string_view key) {
        for (const auto& flag : kFlags) {
            if (key == flag.key) return &flag;
        }
        return nullptr;
    }

    bool anyActive() {
        const auto& current = opts();
        for (const auto& flag : kFlags) {
            if (flag.primary && current.*(flag.member)) return true;
        }
        return false;
    }

    NXR::Hack& hackOf(const char* window, const char* name) {
        return NXR::Gui::get().getWindow(window).findHackByName(name);
    }

    void syncMaster() {
        hackOf(kWindow, kName).setEnabled(anyActive());
    }

    void clearPrimary() {
        auto& config = NXRConfig::get();
        auto& current = opts();
        for (const auto& flag : kFlags) {
            if (!flag.primary) continue;
            current.*(flag.member) = false;
            config.set<bool>(keyOf(flag.key), false);
        }
    }

    void setFlag(const Flag& flag, bool value) {
        opts().*(flag.member) = value;
        NXRConfig::get().set<bool>(keyOf(flag.key), value);
        syncMaster();
    }

    bool g_padClicked = false;
    bool g_dropReleases = false;
    bool g_logicP1 = false;
    bool g_logicP2 = false;
    int g_deadPlayer = 0;

    bool controlsFlipped() {
        auto* pl = PlayLayer::get();
        if (pl && pl->m_levelSettings && pl->m_levelSettings->m_platformerMode) return false;
        return GameManager::get()->getGameVariable("0010");
    }

    bool assistActive(GJBaseGameLayer* layer) {
        auto* pl = PlayLayer::get();
        if (!pl || static_cast<GJBaseGameLayer*>(pl) != layer) return false;
        if (pl->m_isPaused || pl->m_levelEndAnimationStarted) return false;
        return NXR::Bot::State::get().mode != NXR::Bot::Mode::Playing;
    }

    bool isHeld(PlayerObject* player) {
        auto it = player->m_holdingButtons.find(1);
        return it != player->m_holdingButtons.end() && it->second;
    }

    bool descending(PlayerObject* player) {
        return (player->m_yVelocity <= 0 && !player->m_isUpsideDown)
            || (player->m_yVelocity >= 0 && player->m_isUpsideDown);
    }

    template <class Queue>
    void pushFront(Queue& queue, const PlayerButtonCommand& command) {
#ifndef GEODE_IS_ANDROID
        queue.insert(queue.begin(), command);
#else
        queue.push_back(command);
        for (size_t i = queue.size() - 1; i > 0; --i) std::swap(queue[i], queue[i - 1]);
#endif
    }

    template <class Queue>
    void tapFront(Queue& queue, PlayerButtonCommand command) {
        command.m_isPush = false;
        pushFront(queue, command);
        command.m_isPush = true;
        pushFront(queue, command);
    }

    void queueTap(GJBaseGameLayer* layer, bool player2) {
        const bool slot = player2 != controlsFlipped();
        layer->queueButton(static_cast<int>(PlayerButton::Jump), true, slot, 0.0);
        layer->queueButton(static_cast<int>(PlayerButton::Jump), false, slot, 0.0);
    }

    void syncGravityHold(GJBaseGameLayer* layer) {
        auto* first = layer->m_player1;
        auto* second = layer->m_player2;
        auto* ui = layer->m_uiLayer;
        if (!first || !second || !ui) return;

        const bool flipped = controlsFlipped();
        const bool firstMaintain = isHeld(first) != first->m_isUpsideDown;
        const bool secondMaintain = isHeld(second) != second->m_isUpsideDown;

        bool firstWanted = ui->m_p1Jumping || ui->m_p1TouchId != -1;
        bool secondWanted = ui->m_p2Jumping || ui->m_p2TouchId != -1;
        if (flipped) std::swap(firstWanted, secondWanted);

        layer->m_queuedButtons.clear();

        if (firstWanted != firstMaintain) {
            layer->queueButton(static_cast<int>(PlayerButton::Jump), !isHeld(first), flipped, 0.0);
        }

        const bool twoPlayer = layer->m_levelSettings && layer->m_levelSettings->m_twoPlayerMode;
        if (secondWanted != secondMaintain && layer->m_gameState.m_isDualMode && twoPlayer) {
            layer->queueButton(static_cast<int>(PlayerButton::Jump), !isHeld(second), !flipped, 0.0);
        }
    }

    void tapForPad(PlayerObject* player, bool enabled) {
        if (!enabled || g_padClicked) return;
        auto* layer = GJBaseGameLayer::get();
        if (!layer || !assistActive(layer)) return;
        g_padClicked = true;
        queueTap(layer, player->m_isSecondPlayer);
    }

    void addBoundRow(NXRHackSettingsPopup* popup, const std::string& label, bool initial, std::function<void(bool)> setter) {
        popup->prepareNewRow();

        auto toggle = CCMenuItemExt::createTogglerWithFilename(
            "NXR_togglerOn.png"_spr, "NXR_togglerOff.png"_spr, 0.8f,
            [setter = std::move(setter)](CCMenuItemToggler* sender) {
                setter(!sender->isOn());
            }
        );
        toggle->toggle(initial);
        popup->m_currentRow->addChild(toggle);

        auto text = geode::Label::create(label, "GoogleSans.fnt"_spr);
        text->setScale(0.55f);
        if (text->getScaledContentSize().width > 160.f) text->setScale(160.f / text->getContentSize().width);
        popup->m_currentRow->addChild(text);

        popup->m_currentRow->updateLayout();
    }

    void addFlagRow(NXRHackSettingsPopup* popup, const char* key) {
        const Flag* flag = findFlag(key);
        if (!flag) return;
        addBoundRow(popup, flag->label, opts().*(flag->member), [flag](bool value) { setFlag(*flag, value); });
    }

    void addHackRow(NXRHackSettingsPopup* popup, const char* label, const char* window, const char* name) {
        addBoundRow(popup, label, hackOf(window, name).getEnabled(), [window, name](bool value) {
            hackOf(window, name).setEnabled(value);
        });
    }

    void addHackSettingRow(NXRHackSettingsPopup* popup, const char* label, const char* window, const char* name, const char* setting, bool fallback) {
        const std::string key = hackOf(window, name).formatAdditionalSetting(setting);
        addBoundRow(popup, label, NXRConfig::get().get<bool>(key, fallback), [key](bool value) {
            NXRConfig::get().set<bool>(key, value);
        });
    }

    void addAutoClickerRow(NXRHackSettingsPopup* popup, const char* label, bool second) {
        auto& hack = hackOf("Utils", "Auto Clicker");
        const bool initial = hack.getEnabled()
            && NXRConfig::get().get<bool>(hack.formatAdditionalSetting(second ? "p2" : "p1"), !second);

        addBoundRow(popup, label, initial, [second](bool value) {
            auto& target = hackOf("Utils", "Auto Clicker");
            auto& config = NXRConfig::get();
            const std::string mine = target.formatAdditionalSetting(second ? "p2" : "p1");
            const std::string other = target.formatAdditionalSetting(second ? "p1" : "p2");
            if (!target.getEnabled()) config.set<bool>(other, false);
            config.set<bool>(mine, value);
            target.setEnabled(value || config.get<bool>(other, second));
        });
    }

    void buildAutoClickerCocos(NXRHackSettingsPopup* popup) {
        auto& hack = hackOf("Utils", "Auto Clicker");
        const std::string mode = hack.formatAdditionalSetting("mode");
        const std::string p1Hold = hack.formatAdditionalSetting("p1_hold");
        const std::string p1Release = hack.formatAdditionalSetting("p1_release");
        const std::string p1Cps = hack.formatAdditionalSetting("p1_cps");
        const std::string p2Hold = hack.formatAdditionalSetting("p2_hold");
        const std::string p2Release = hack.formatAdditionalSetting("p2_release");
        const std::string p2Cps = hack.formatAdditionalSetting("p2_cps");
        const std::string onlyHold = hack.formatAdditionalSetting("only_hold");

        addAutoClickerRow(popup, "Auto Clicker P1", false);
        addAutoClickerRow(popup, "Auto Clicker P2", true);

        popup->addConfigModeToggle(mode, "Normal", "Super", 1, [weak = geode::WeakRef(popup)](int) {
            geode::queueInMainThread([weak] {
                if (auto popup = weak.lock()) popup->rebuild();
            });
        });

        if (NXRConfig::get().get<int>(mode, 1) == 2) {
            popup->addConfigIntInput("P1 CPS", p1Cps, 1, 5000000, 240);
            popup->addConfigIntInput("P2 CPS", p2Cps, 1, 5000000, 240);
        } else {
            popup->addConfigIntInput("P1 Hold (frames)", p1Hold, 1, 999, 1);
            popup->addConfigIntInput("P1 Release (frames)", p1Release, 1, 999, 1);
            popup->addConfigIntInput("P2 Hold (frames)", p2Hold, 1, 999, 1);
            popup->addConfigIntInput("P2 Release (frames)", p2Release, 1, 999, 1);
        }
        popup->addConfigToggle("Only While Holding", onlyHold, false);
    }

    void buildAutoClickerImGui() {
        auto& hack = hackOf("Utils", "Auto Clicker");
        const std::string mode = hack.formatAdditionalSetting("mode");
        const std::string p1 = hack.formatAdditionalSetting("p1");
        const std::string p2 = hack.formatAdditionalSetting("p2");
        const std::string p1Hold = hack.formatAdditionalSetting("p1_hold");
        const std::string p1Release = hack.formatAdditionalSetting("p1_release");
        const std::string p1Cps = hack.formatAdditionalSetting("p1_cps");
        const std::string p2Hold = hack.formatAdditionalSetting("p2_hold");
        const std::string p2Release = hack.formatAdditionalSetting("p2_release");
        const std::string p2Cps = hack.formatAdditionalSetting("p2_cps");
        const std::string onlyHold = hack.formatAdditionalSetting("only_hold");

        ImGui::TextDisabled("Auto Clicker");
        bool enabled = hack.getEnabled();
        if (ImGui::Checkbox("Enabled##nox_ac", &enabled)) hack.setEnabled(enabled);

        NXRWidgetConfig::ModeSwitch(mode, "Normal", "Super");
        NXRWidgetConfig::Checkbox("Player 1##nox_ac", p1, true);
        NXRWidgetConfig::Checkbox("Player 2##nox_ac", p2, false);

        if (NXRConfig::get().get<int>(mode, 1) == 2) {
            NXRWidgetConfig::InputInt("P1 CPS", p1Cps, 240);
            NXRWidgetConfig::InputInt("P2 CPS", p2Cps, 240);
        } else {
            NXRWidgetConfig::DragInt("##nox_p1_hold", p1Hold, 1.f, 1, 999, 1, "P1 Hold: %d");
            NXRWidgetConfig::DragInt("##nox_p1_release", p1Release, 1.f, 1, 999, 1, "P1 Release: %d");
            NXRWidgetConfig::DragInt("##nox_p2_hold", p2Hold, 1.f, 1, 999, 1, "P2 Hold: %d");
            NXRWidgetConfig::DragInt("##nox_p2_release", p2Release, 1.f, 1, 999, 1, "P2 Release: %d");
        }
        NXRWidgetConfig::Checkbox("Only While Holding##nox_ac", onlyHold, false);
        ImGui::Separator();
    }

    void buildCocosPanel(CCNode* node) {
        auto* popup = static_cast<NXRHackSettingsPopup*>(node);

        addFlagRow(popup, "flip_death");
        addFlagRow(popup, "flip_p1");
        addFlagRow(popup, "flip_p2");
        addFlagRow(popup, "flip_both");
        addFlagRow(popup, "flip_swift");
        popup->addSeparator();

        addFlagRow(popup, "dash_orbs");
        addFlagRow(popup, "black_orbs");
        addFlagRow(popup, "black_orb_ufo");
        addFlagRow(popup, "jump_pads");
        addFlagRow(popup, "gravity_pads");
        addFlagRow(popup, "maintain_gravity");
        popup->addSeparator();

        addHackRow(popup, "Auto Straight Fly", "Utils", "Ship Straight");
        addHackRow(popup, "Auto Straight Ufo", "Utils", "UFO Straight");
        popup->addSeparator();

        buildAutoClickerCocos(popup);
        popup->addSeparator();

        addHackRow(popup, "Noclip", "Player", "Noclip");
        addHackSettingRow(popup, "Noclip: Player 1", "Player", "Noclip", "p1", true);
        addHackSettingRow(popup, "Noclip: Player 2", "Player", "Noclip", "p2", true);
        popup->addSeparator();

        addFlagRow(popup, "auto_swift");
        addFlagRow(popup, "extra_clicks");
        popup->addConfigIntInput("Extra Click Amount", keyOf("extra_amount"), 1, 50, 1, [](int value) {
            opts().extraAmount = std::clamp(value, 1, 50);
        });
    }

    void drawImGuiPanel() {
        bool changed = false;

        buildAutoClickerImGui();

        for (const auto& flag : kFlags) {
            if (NXRWidgetConfig::Checkbox(flag.label, keyOf(flag.key), flag.fallback)) changed = true;
            if (std::string_view(flag.key) == "flip_swift" || std::string_view(flag.key) == "maintain_gravity") ImGui::Separator();
        }

        if (NXRWidgetConfig::DragInt("##nox_extra_amount", keyOf("extra_amount"), 1.f, 1, 50, 1, "Extra Click Amount: %d")) changed = true;

        if (changed) {
            opts() = loadOpts();
            syncMaster();
        }
    }
}

CCMenuItemSpriteExtra* NXR::NoxUtils::makePauseButton() {
    auto icon = CCSprite::create("NXR_noxUtilsBtn.png"_spr);
    auto button = CCMenuItemExt::createSpriteExtra(icon, [](CCMenuItemSpriteExtra*) {
        if (!PlayLayer::get()) return;
        if (auto* popup = NXRHackSettingsPopup::create(hackOf(kWindow, kName))) popup->show();
    });
    button->setID("nox-utils-button"_spr);
    return button;
}

$execute {
    auto& hack = hackOf(kWindow, kName);
    hack.setHandler([](bool state) {
        if (!state) clearPrimary();
    });
    hack.setCustomWindowCocos(buildCocosPanel);
    hack.setCustomWindowImGui(drawImGuiPanel);
}

class $modify(NXRNoxUtilsBaseGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        auto& hack = hackOf(kWindow, kName);
        NXR::trySetPriority(self, "GJBaseGameLayer::processQueuedButtons", -50);
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::processQueuedButtons");
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::processCommands");
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        g_padClicked = false;
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }

    void processQueuedButtons(float dt, bool clearInputQueue) {
        if (!assistActive(this)) {
            GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
            return;
        }

        const auto& current = opts();

        if (current.maintainGravity) syncGravityHold(this);

        if (current.autoSwift || g_dropReleases) {
            m_queuedButtons.erase(
                std::remove_if(m_queuedButtons.begin(), m_queuedButtons.end(), [](const PlayerButtonCommand& command) {
                    return !command.m_isPush;
                }),
                m_queuedButtons.end()
            );
            g_dropReleases = false;
        }

        const auto pending = m_queuedButtons;
        const bool flipped = controlsFlipped();
        bool gravityReleased = false;

        for (const auto& command : pending) {
            if (!command.m_isPush) continue;

            auto* player = (command.m_isPlayer2 != flipped) ? m_player2 : m_player1;
            if (!player) continue;

            PlayerButtonCommand fake;
            fake.m_isPlayer2 = command.m_isPlayer2;
            fake.m_button = PlayerButton::Jump;
            fake.m_step = 0;
            fake.m_timestamp = 0.0;

            if (player->m_touchingRings) {
                for (auto* ring : CCArrayExt<GameObject*>(player->m_touchingRings)) {
                    if (!ring) continue;
                    const auto type = ring->m_objectType;

                    if (current.dashOrbs && type == GameObjectType::DashRing) tapFront(m_queuedButtons, fake);

                    if (type == GameObjectType::DropRing && (current.blackOrbs || (current.blackOrbUfo && descending(player)))) {
                        tapFront(m_queuedButtons, fake);
                    }

                    const bool gravityOrb = type == GameObjectType::GravityDashRing
                        || type == GameObjectType::GravityRing
                        || type == GameObjectType::GreenRing;
                    if (current.maintainGravity && gravityOrb && !gravityReleased) {
                        auto release = fake;
                        release.m_isPush = false;
                        m_queuedButtons.push_back(release);
                        gravityReleased = true;
                        g_dropReleases = true;
                    }
                }
            }

            if (current.extraClicks) {
                for (int i = 0; i < current.extraAmount; i++) tapFront(m_queuedButtons, fake);
            }

            if (current.autoSwift && !m_queuedButtons.empty() && m_queuedButtons.back().m_isPush) {
                auto release = fake;
                release.m_isPush = false;
                m_queuedButtons.push_back(release);
            }
        }

        GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
    }
};

class $modify(NXRNoxUtilsPlayerObject, PlayerObject) {
    static void onModify(auto& self) {
        auto& hack = hackOf(kWindow, kName);
        NXR::tryAddHook(self, hack, "PlayerObject::propellPlayer");
        NXR::tryAddHook(self, hack, "PlayerObject::bumpPlayer");
    }

    void propellPlayer(float yVelocity, bool noEffects, int objectType) {
        if (objectType == 10) tapForPad(this, opts().gravityPads);
        PlayerObject::propellPlayer(yVelocity, noEffects, objectType);
    }

    void bumpPlayer(float bumpMod, int objectType, bool noEffects, GameObject* object) {
        if (objectType == 9 || objectType == 8 || objectType == 34) tapForPad(this, opts().jumpPads);
        PlayerObject::bumpPlayer(bumpMod, objectType, noEffects, object);
    }
};

class $modify(NXRNoxUtilsPlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        auto& hack = hackOf(kWindow, kName);
        NXR::trySetPriority(self, "PlayLayer::resetLevel", -100);
        NXR::tryAddHook(self, hack, "PlayLayer::destroyPlayer");
        NXR::tryAddHook(self, hack, "PlayLayer::resetLevel");
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        const bool wasDead = player && player->m_isDead;
        PlayLayer::destroyPlayer(player, object);
        if (player && !wasDead && player->m_isDead && opts().flipDeath) {
            g_deadPlayer = player->m_isSecondPlayer ? 2 : 1;
        }
    }

    void resetLevel() {
        PlayLayer::resetLevel();

        const int dead = g_deadPlayer;
        g_deadPlayer = 0;

        const auto& current = opts();
        if (!current.flipDeath || dead == 0) return;
        if (NXR::Bot::State::get().mode == NXR::Bot::Mode::Playing) return;

        const bool flipped = controlsFlipped();
        bool queued = false;

        if ((current.flipP1 && dead == 1) || current.flipBoth) {
            if (current.flipSwift) {
                queueTap(this, false);
            } else {
                if (g_logicP1) queueButton(static_cast<int>(PlayerButton::Jump), true, flipped, 0.0);
                g_logicP1 = !g_logicP1;
            }
            queued = true;
        }

        if ((current.flipP2 && dead == 2) || current.flipBoth) {
            if (current.flipSwift) {
                queueTap(this, true);
            } else {
                if (g_logicP2) queueButton(static_cast<int>(PlayerButton::Jump), true, !flipped, 0.0);
                g_logicP2 = !g_logicP2;
            }
            queued = true;
        }

        if (queued) this->processQueuedButtons(0.f, true);
    }
};
