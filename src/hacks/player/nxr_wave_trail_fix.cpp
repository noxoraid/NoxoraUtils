#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/HardStreak.hpp>
#include <cmath>
#include "../../core/nxr_config.hpp"

using namespace geode::prelude;

namespace {
    CCPoint g_last[2];
    bool g_hasLast[2] = {false, false};

    void resetLast() {
        g_hasLast[0] = g_hasLast[1] = false;
    }
}

class $modify(NXRWaveTrailFixPlayerObject, PlayerObject) {
    void update(float dt) {
        PlayerObject::update(dt);

        if (!NXRConfig::get().get<bool>("nxr.bot.wave_trail_fix", true)) return;

        auto* pl = PlayLayer::get();
        if (!pl) return;

        int slot = -1;
        if (this == pl->m_player1) slot = 0;
        else if (this == pl->m_player2) slot = 1;
        if (slot < 0) return;

        if (!m_isDart || !m_waveTrail || m_isDead) {
            g_hasLast[slot] = false;
            return;
        }

        const CCPoint pos = this->getPosition();
        if (g_hasLast[slot] && g_last[slot].equals(pos)) return;

        m_waveTrail->addPoint(pos);
        g_last[slot] = pos;
        g_hasLast[slot] = true;
    }
};

class $modify(NXRWaveTrailFixPlayLayer, PlayLayer) {
    void resetLevel() {
        resetLast();
        PlayLayer::resetLevel();
    }
};

// The wave trail is a strip built from a list of points. Two identical points in a row make
// a zero-length segment, whose direction cannot be computed, and the strip then draws a thin
// line stretching back along the trail. Points get duplicated when the game places a corner
// at the player's position and the fix above adds the same position again, which happens at
// every click during playback. This drops those points, and any point that is not a number.
class $modify(NXRWaveTrailGuardHardStreak, HardStreak) {
    struct Fields {
        CCPoint last = {0.f, 0.f};
        bool hasLast = false;
    };

    void addPoint(CCPoint point) {
        if (!NXRConfig::get().get<bool>("nxr.bot.wave_trail_fix", true)) {
            HardStreak::addPoint(point);
            return;
        }

        if (!std::isfinite(point.x) || !std::isfinite(point.y)) return;

        auto fields = m_fields.self();
        constexpr float kSamePoint = 0.001f;
        if (fields->hasLast && std::fabs(fields->last.x - point.x) < kSamePoint && std::fabs(fields->last.y - point.y) < kSamePoint) return;

        fields->last = point;
        fields->hasLast = true;
        HardStreak::addPoint(point);
    }
};
