#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <cmath>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"
#include "../../core/nxr_config.hpp"
#include "../../interface/imgui/nxr_widget_helper.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"
#include "imgui.h"

NXR_HACK_CREATE("Utils", "Auto Clicker", "Automatically clicks the jump button. Normal uses hold/release frames, Super uses clicks per second up to 5,000,000", true);

namespace {
    struct ClickerState {
        int counter = 0;
        bool holding = false;
        double acc = 0.0;

        void reset() {
            counter = 0;
            holding = false;
            acc = 0.0;
        }
    };

    ClickerState g_p1;
    ClickerState g_p2;

    void tickPlayer(PlayerObject* player, ClickerState& state, int hold, int release) {
        hold = std::max(hold, 1);
        release = std::max(release, 1);

        state.counter++;
        if (!state.holding) {
            if (state.counter >= release) {
                state.counter = 0;
                state.holding = true;
                player->pushButton(PlayerButton::Jump);
            }
        } else if (state.counter >= hold) {
            state.counter = 0;
            state.holding = false;
            player->releaseButton(PlayerButton::Jump);
        }
    }

    void tickSuper(PlayerObject* player, ClickerState& state, int cps, float dt) {
        cps = std::clamp(cps, 1, 5000000);
        if (dt <= 0.f || dt > 0.1f) dt = 1.f / 240.f;

        state.acc += 2.0 * static_cast<double>(cps) * dt;
        long long toggles = std::min(static_cast<long long>(state.acc), 60000LL);
        state.acc -= std::floor(state.acc);
        if (toggles <= 0) return;

        if (toggles % 2 == 0) toggles -= 1;

        for (long long i = 0; i < toggles; i++) {
            if (state.holding) {
                player->releaseButton(PlayerButton::Jump);
                state.holding = false;
            } else {
                player->pushButton(PlayerButton::Jump);
                state.holding = true;
            }
        }
    }

    bool physicalHold(GJBaseGameLayer* layer, bool second) {
        auto* ui = layer->m_uiLayer;
        if (!ui) return false;
        const bool first = ui->m_p1Jumping || ui->m_p1TouchId != -1;
        const bool other = ui->m_p2Jumping || ui->m_p2TouchId != -1;
        if (!layer->m_levelSettings || !layer->m_levelSettings->m_twoPlayerMode) return first || other;
        return second ? other : first;
    }

    void releaseIfHolding(PlayerObject* player, ClickerState& state) {
        if (state.holding && player) {
            player->releaseButton(PlayerButton::Jump);
        }
        state.reset();
    }
}

class $modify(NXRAutoClickerBaseGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        auto& gui = NXR::Gui::get();
        auto& hack = gui.getWindow("Utils").findHackByName("Auto Clicker");

        NXR::trySetPriority(self, "GJBaseGameLayer::processCommands", -10);
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::processCommands");

        hack.setCustomWindowImGui([
            mode = hack.formatAdditionalSetting("mode"),
            p1 = hack.formatAdditionalSetting("p1"),
            p1Hold = hack.formatAdditionalSetting("p1_hold"),
            p1Release = hack.formatAdditionalSetting("p1_release"),
            p1Cps = hack.formatAdditionalSetting("p1_cps"),
            p2 = hack.formatAdditionalSetting("p2"),
            p2Hold = hack.formatAdditionalSetting("p2_hold"),
            p2Release = hack.formatAdditionalSetting("p2_release"),
            p2Cps = hack.formatAdditionalSetting("p2_cps"),
            onlyHold = hack.formatAdditionalSetting("only_hold")
        ]{
            NXRWidgetConfig::ModeSwitch(mode, "Normal", "Super");
            NXRWidgetConfig::Checkbox("Only While Holding", onlyHold, false);
            ImGui::Separator();

            auto& config = NXRConfig::get();
            if (config.get<int>(mode, 1) == 2) {
                NXRWidgetConfig::Checkbox("Player 1", p1, true);
                if (NXRWidgetConfig::InputInt("P1 CPS", p1Cps, 240)) {
                    config.set<int>(p1Cps, std::clamp(config.get<int>(p1Cps, 240), 1, 5000000));
                }
                ImGui::Separator();
                NXRWidgetConfig::Checkbox("Player 2", p2, false);
                if (NXRWidgetConfig::InputInt("P2 CPS", p2Cps, 240)) {
                    config.set<int>(p2Cps, std::clamp(config.get<int>(p2Cps, 240), 1, 5000000));
                }
            } else {
                NXRWidgetConfig::Checkbox("Player 1", p1, true);
                NXRWidgetConfig::DragInt("##P1 Hold", p1Hold, 1.f, 1, 999, 1, "P1 Hold: %d");
                NXRWidgetConfig::DragInt("##P1 Release", p1Release, 1.f, 1, 999, 1, "P1 Release: %d");
                ImGui::Separator();
                NXRWidgetConfig::Checkbox("Player 2", p2, false);
                NXRWidgetConfig::DragInt("##P2 Hold", p2Hold, 1.f, 1, 999, 1, "P2 Hold: %d");
                NXRWidgetConfig::DragInt("##P2 Release", p2Release, 1.f, 1, 999, 1, "P2 Release: %d");
            }
        });

        hack.setCustomWindowCocos([
            mode = hack.formatAdditionalSetting("mode"),
            p1 = hack.formatAdditionalSetting("p1"),
            p1Hold = hack.formatAdditionalSetting("p1_hold"),
            p1Release = hack.formatAdditionalSetting("p1_release"),
            p1Cps = hack.formatAdditionalSetting("p1_cps"),
            p2 = hack.formatAdditionalSetting("p2"),
            p2Hold = hack.formatAdditionalSetting("p2_hold"),
            p2Release = hack.formatAdditionalSetting("p2_release"),
            p2Cps = hack.formatAdditionalSetting("p2_cps"),
            onlyHold = hack.formatAdditionalSetting("only_hold")
        ](cocos2d::CCNode* popupNode) {
            auto* popup = static_cast<NXRHackSettingsPopup*>(popupNode);
            popup->addConfigModeToggle(mode, "Normal", "Super", 1, [weak = geode::WeakRef(popup)](int) {
                geode::queueInMainThread([weak] {
                    if (auto popup = weak.lock()) popup->rebuild();
                });
            });
            popup->addConfigToggle("Only While Holding", onlyHold, false);
            popup->addSeparator();

            if (NXRConfig::get().get<int>(mode, 1) == 2) {
                popup->addConfigToggle("Player 1", p1, true);
                popup->addConfigIntInput("P1 CPS", p1Cps, 1, 5000000, 240);
                popup->addSeparator();
                popup->addConfigToggle("Player 2", p2, false);
                popup->addConfigIntInput("P2 CPS", p2Cps, 1, 5000000, 240);
            } else {
                popup->addConfigToggle("Player 1", p1, true);
                popup->addConfigIntInput("P1 Hold", p1Hold, 1, 999, 1);
                popup->addConfigIntInput("P1 Release", p1Release, 1, 999, 1);
                popup->addSeparator();
                popup->addConfigToggle("Player 2", p2, false);
                popup->addConfigIntInput("P2 Hold", p2Hold, 1, 999, 1);
                popup->addConfigIntInput("P2 Release", p2Release, 1, 999, 1);
            }
        });
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto& config = NXRConfig::get();

        auto* pl = PlayLayer::get();
        bool active = pl && static_cast<GJBaseGameLayer*>(pl) == this
            && !pl->m_isPaused
            && !pl->m_levelEndAnimationStarted;

        if (active) {
            PlayerObject* p1 = m_player1;
            PlayerObject* p2 = m_gameState.m_isDualMode ? m_player2 : nullptr;

            const bool super = config.get<int>("nxr.utils.auto_clicker::mode", 1) == 2;
            const bool onlyHold = config.get<bool>("nxr.utils.auto_clicker::only_hold", false);

            if (p1 && !p1->m_isDead && config.get<bool>("nxr.utils.auto_clicker::p1", true) && (!onlyHold || physicalHold(this, false))) {
                if (super) {
                    tickSuper(p1, g_p1, config.get<int>("nxr.utils.auto_clicker::p1_cps", 240), dt);
                } else {
                    tickPlayer(
                        p1, g_p1,
                        config.get<int>("nxr.utils.auto_clicker::p1_hold", 1),
                        config.get<int>("nxr.utils.auto_clicker::p1_release", 1)
                    );
                }
            } else {
                releaseIfHolding(p1, g_p1);
            }

            if (p2 && !p2->m_isDead && config.get<bool>("nxr.utils.auto_clicker::p2", false) && (!onlyHold || physicalHold(this, true))) {
                if (super) {
                    tickSuper(p2, g_p2, config.get<int>("nxr.utils.auto_clicker::p2_cps", 240), dt);
                } else {
                    tickPlayer(
                        p2, g_p2,
                        config.get<int>("nxr.utils.auto_clicker::p2_hold", 1),
                        config.get<int>("nxr.utils.auto_clicker::p2_release", 1)
                    );
                }
            } else {
                releaseIfHolding(m_player2, g_p2);
            }
        } else {
            g_p1.reset();
            g_p2.reset();
        }

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};

class $modify(NXRAutoClickerPlayLayer, PlayLayer) {
    void resetLevel() {
        g_p1.reset();
        g_p2.reset();
        PlayLayer::resetLevel();
    }
};
