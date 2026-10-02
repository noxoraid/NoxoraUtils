#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <cmath>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"
#include "../../core/nxr_config.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"

NXR_HACK_CREATE("Utils", "Auto Clicker", "Automatically clicks the jump button. Normal uses hold/release frames (0.1 minimum), Super uses clicks per second up to 5,000,000, Angle clicks by degree (90 = straight, lower = bigger up/down swings)", true);

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

    void runPhases(PlayerObject* player, ClickerState& state, double holdTicks, double releaseTicks) {
        holdTicks = std::max(holdTicks, 0.1);
        releaseTicks = std::max(releaseTicks, 0.1);

        state.acc += 1.0;

        bool phaseHolding = state.holding;
        long long toggles = 0;
        while (toggles < 60000LL) {
            const double need = phaseHolding ? holdTicks : releaseTicks;
            if (state.acc < need) break;
            state.acc -= need;
            phaseHolding = !phaseHolding;
            toggles++;
        }
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

    void tickPlayer(PlayerObject* player, ClickerState& state, double hold, double release) {
        runPhases(player, state, hold, release);
    }

    void tickAngle(PlayerObject* player, ClickerState& state, double angle) {
        angle = std::clamp(angle, 0.0, 90.0);
        const double half = std::max(0.5, (90.0 - angle) / 3.0);
        runPhases(player, state, half, half);
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
            p1Angle = hack.formatAdditionalSetting("p1_angle"),
            p2Angle = hack.formatAdditionalSetting("p2_angle"),
            onlyHold = hack.formatAdditionalSetting("only_hold")
        ](cocos2d::CCNode* popupNode) {
            auto* popup = static_cast<NXRHackSettingsPopup*>(popupNode);
            popup->addConfigSelect("Mode", mode, {{"Normal", 1}, {"Super", 2}, {"Angle", 3}}, 1, [weak = geode::WeakRef(popup)](int) {
                geode::queueInMainThread([weak] {
                    if (auto popup = weak.lock()) popup->rebuild();
                });
            });
            popup->addConfigToggle("Only While Holding", onlyHold, false);
            popup->addSeparator();

            const int curMode = NXRConfig::get().get<int>(mode, 1);
            if (curMode == 3) {
                popup->addConfigToggle("Player 1", p1, true);
                popup->addConfigFloatInput("P1 Angle (0 - 90)", p1Angle, 0.f, 90.f, 90.f);
                popup->addSeparator();
                popup->addConfigToggle("Player 2", p2, false);
                popup->addConfigFloatInput("P2 Angle (0 - 90)", p2Angle, 0.f, 90.f, 90.f);
            } else if (curMode == 2) {
                popup->addConfigToggle("Player 1", p1, true);
                popup->addConfigIntInput("P1 CPS", p1Cps, 1, 5000000, 240);
                popup->addSeparator();
                popup->addConfigToggle("Player 2", p2, false);
                popup->addConfigIntInput("P2 CPS", p2Cps, 1, 5000000, 240);
            } else {
                popup->addConfigToggle("Player 1", p1, true);
                popup->addConfigFloatInput("P1 Hold", p1Hold, 0.1f, 999.f, 1.f);
                popup->addConfigFloatInput("P1 Release", p1Release, 0.1f, 999.f, 1.f);
                popup->addSeparator();
                popup->addConfigToggle("Player 2", p2, false);
                popup->addConfigFloatInput("P2 Hold", p2Hold, 0.1f, 999.f, 1.f);
                popup->addConfigFloatInput("P2 Release", p2Release, 0.1f, 999.f, 1.f);
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

            const int clickMode = config.get<int>("nxr.utils.auto_clicker::mode", 1);
            const bool super = clickMode == 2;
            const bool angleMode = clickMode == 3;
            const bool onlyHold = config.get<bool>("nxr.utils.auto_clicker::only_hold", false);

            if (p1 && !p1->m_isDead && config.get<bool>("nxr.utils.auto_clicker::p1", true) && (!onlyHold || physicalHold(this, false))) {
                if (angleMode) {
                    tickAngle(p1, g_p1, config.get<float>("nxr.utils.auto_clicker::p1_angle", 90.f));
                } else if (super) {
                    tickSuper(p1, g_p1, config.get<int>("nxr.utils.auto_clicker::p1_cps", 240), dt);
                } else {
                    tickPlayer(
                        p1, g_p1,
                        config.get<float>("nxr.utils.auto_clicker::p1_hold", 1.f),
                        config.get<float>("nxr.utils.auto_clicker::p1_release", 1.f)
                    );
                }
            } else {
                releaseIfHolding(p1, g_p1);
            }

            if (p2 && !p2->m_isDead && config.get<bool>("nxr.utils.auto_clicker::p2", false) && (!onlyHold || physicalHold(this, true))) {
                if (angleMode) {
                    tickAngle(p2, g_p2, config.get<float>("nxr.utils.auto_clicker::p2_angle", 90.f));
                } else if (super) {
                    tickSuper(p2, g_p2, config.get<int>("nxr.utils.auto_clicker::p2_cps", 240), dt);
                } else {
                    tickPlayer(
                        p2, g_p2,
                        config.get<float>("nxr.utils.auto_clicker::p2_hold", 1.f),
                        config.get<float>("nxr.utils.auto_clicker::p2_release", 1.f)
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
