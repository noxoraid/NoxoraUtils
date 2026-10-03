#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include <string>
#include "nxr_config.hpp"
#include "nxr_player_input.hpp"

// Shared logic for the "straight" hacks (Wave, Ship). Each hack owns one State per
// player and calls apply() once per physics tick, after the game has moved the player.
namespace NXR::Straight {
    enum Mode : int {
        Manual = 1,
        Auto = 2,
    };

    struct State {
        bool  holding = false;
        int   idleFrames = 0;
        int   autoCounter = 0;
        bool  autoHolding = false;
        float lockY = 0.f;
        bool  hasLockY = false;

        void reset() {
            holding = false;
            idleFrames = 0;
            autoCounter = 0;
            autoHolding = false;
            lockY = 0.f;
            hasLockY = false;
        }
    };

    inline bool shouldStraighten(int mode, const State& st, int graceFrames) {
        if (mode == Auto) return true;

        return st.holding || st.idleFrames <= graceFrames;
    }

    // Keeps `player` on a flat line while the hack decides it should.
    //
    //  Manual: straight while the button is held, plus a short grace window after the
    //          last click so a quick tap still locks the height.
    //  Auto:   the hack clicks by itself every `auto_rate` ticks and always stays straight.
    //
    // `keyPrefix` is the config prefix of the calling hack, e.g. "nxr.utils.wave_straight".
    inline void apply(PlayerObject* player, State& st, const std::string& keyPrefix) {
        auto& config = NXRConfig::get();

        const int mode  = std::clamp(config.get<int>(keyPrefix + "::mode", Manual), Manual, Auto);
        const int grace = std::max(0, config.get<int>(keyPrefix + "::grace", 4));

        if (mode == Auto) {
            const int rate = std::max(1, config.get<int>(keyPrefix + "::auto_rate", 1));
            if (++st.autoCounter >= rate) {
                st.autoCounter = 0;
                st.autoHolding = !st.autoHolding;
                if (st.autoHolding) NXR::Input::press(player);
                else NXR::Input::release(player);
            }
        } else if (st.autoHolding) {
            // Switched from Auto to Manual while the auto click was down: let go of it.
            NXR::Input::release(player);
            st.autoHolding = false;
            st.autoCounter = 0;
        }

        st.idleFrames++;

        if (!shouldStraighten(mode, st, grace)) {
            st.hasLockY = false;
            return;
        }

        // Remember the height of the first straight tick and pin the player to it.
        if (!st.hasLockY) {
            st.lockY = player->getPositionY();
            st.hasLockY = true;
        }

        player->m_yVelocity = 0.0;
        player->setPositionY(st.lockY);
        player->setRotation(0.f);
    }
}
