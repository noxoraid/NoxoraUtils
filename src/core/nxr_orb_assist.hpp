#pragma once
#include <Geode/Geode.hpp>
#include <unordered_set>
#include "nxr_bot.hpp"
#include "nxr_player_input.hpp"

namespace NXR::Orb {
    constexpr int kBlackOrb = 1330;
    constexpr int kPinkDash = 1751;
    constexpr int kGreenDash = 1704;

    struct Tracker {
        std::unordered_set<const void*> used;
        bool holding = false;
        int age = 0;

        void reset() {
            used.clear();
            holding = false;
            age = 0;
        }
    };

    inline bool gameActive() {
        auto* pl = PlayLayer::get();
        if (!pl || pl->m_isPaused || pl->m_levelEndAnimationStarted) return false;
        return NXR::Bot::State::get().mode != NXR::Bot::Mode::Playing;
    }

    inline bool isHolding(PlayerObject* player) {
        auto it = player->m_holdingButtons.find(1);
        return it != player->m_holdingButtons.end() && it->second;
    }

    template <class Match>
    inline void tick(PlayerObject* player, Tracker& t, bool dash, Match&& match) {
        if (!player || player->m_isDead) {
            t.reset();
            return;
        }

        if (t.holding) {
            t.age++;
            const bool finished = dash ? (!player->m_isDashing && t.age >= 2) : t.age >= 2;
            if (!finished && t.age <= 480) return;
            NXR::Input::release(player);
            t.holding = false;
            t.age = 0;
        }

        auto* rings = player->m_touchingRings;
        if (!rings || rings->count() == 0) return;

        for (auto* obj : CCArrayExt<GameObject*>(rings)) {
            if (!obj || t.used.contains(obj) || !match(obj)) continue;
            t.used.insert(obj);
            if (isHolding(player)) NXR::Input::release(player);
            NXR::Input::press(player);
            t.holding = true;
            t.age = 0;
            return;
        }
    }
}
