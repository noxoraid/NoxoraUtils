#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/HardStreak.hpp>
#include <cmath>
#include "../../core/nxr_config.hpp"

using namespace geode::prelude;

namespace {
    struct WaveTrack {
        CCPoint last = {0.f, 0.f};
        int sx = 0;
        int sy = 0;
        bool has = false;
        void* trail = nullptr;

        void reset() {
            has = false;
            sx = 0;
            sy = 0;
            trail = nullptr;
        }
    };

    WaveTrack g_track[2];
    unsigned g_generation = 0;

    constexpr float kMoveEps = 0.01f;
    constexpr float kJumpDist = 100.f;
    constexpr float kSamePoint = 0.01f;

    bool fixEnabled() {
        return NXRConfig::get().get<bool>("nxr.bot.wave_trail_fix", true);
    }

    int sign(float v) {
        if (v > kMoveEps) return 1;
        if (v < -kMoveEps) return -1;
        return 0;
    }

    void resetAll() {
        g_track[0].reset();
        g_track[1].reset();
        g_generation++;
    }
}

class $modify(NXRWaveTrailFixPlayerObject, PlayerObject) {
    void update(float dt) {
        PlayerObject::update(dt);

        if (!fixEnabled()) return;

        auto* pl = PlayLayer::get();
        if (!pl) return;

        int slot = -1;
        if (this == pl->m_player1) slot = 0;
        else if (this == pl->m_player2) slot = 1;
        if (slot < 0) return;

        auto& track = g_track[slot];

        if (!m_isDart || !m_waveTrail || m_isDead) {
            track.reset();
            return;
        }

        const CCPoint pos = this->getPosition();

        if (!std::isfinite(pos.x) || !std::isfinite(pos.y)) {
            track.reset();
            return;
        }

        if (!track.has || track.trail != static_cast<void*>(m_waveTrail)) {
            track.reset();
            track.last = pos;
            track.trail = m_waveTrail;
            track.has = true;
            return;
        }

        const float dx = pos.x - track.last.x;
        const float dy = pos.y - track.last.y;

        if (std::sqrt(dx * dx + dy * dy) > kJumpDist) {
            track.last = pos;
            track.sx = 0;
            track.sy = 0;
            return;
        }

        const int sx = sign(dx);
        const int sy = sign(dy);
        if (sx == 0 && sy == 0) return;

        const bool turned = (track.sx != 0 || track.sy != 0) && (sx != track.sx || sy != track.sy);
        if (turned) m_waveTrail->addPoint(track.last);

        track.sx = sx;
        track.sy = sy;
        track.last = pos;
    }
};

class $modify(NXRWaveTrailFixPlayLayer, PlayLayer) {
    void resetLevel() {
        resetAll();
        PlayLayer::resetLevel();
        resetAll();
    }

    void onQuit() {
        resetAll();
        PlayLayer::onQuit();
    }
};

class $modify(NXRWaveTrailGuardHardStreak, HardStreak) {
    struct Fields {
        CCPoint last = {0.f, 0.f};
        unsigned generation = 0;
        bool hasLast = false;
    };

    void addPoint(CCPoint point) {
        if (!fixEnabled()) {
            HardStreak::addPoint(point);
            return;
        }

        if (!std::isfinite(point.x) || !std::isfinite(point.y)) return;

        auto fields = m_fields.self();

        if (fields->generation != g_generation) {
            fields->generation = g_generation;
            fields->hasLast = false;
        }

        if (fields->hasLast
            && std::fabs(fields->last.x - point.x) < kSamePoint
            && std::fabs(fields->last.y - point.y) < kSamePoint) return;

        fields->last = point;
        fields->hasLast = true;
        HardStreak::addPoint(point);
    }
};
