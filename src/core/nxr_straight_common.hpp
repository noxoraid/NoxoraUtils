#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>

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
}
