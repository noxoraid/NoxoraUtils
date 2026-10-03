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

    constexpr float kMoveEps = 0.01f;
    constexpr float kJumpDist = 100.f;
    constexpr float kMerge = 3.f;
    constexpr float kSpikeCos = -0.9f;
    constexpr float kCornerGap = 8.f;
    constexpr float kHeadNudge = 0.75f;

    bool fixEnabled() {
        return NXRConfig::get().get<bool>("nxr.bot.wave_trail_fix", true);
    }

    int sign(float v) {
        if (v > kMoveEps) return 1;
        if (v < -kMoveEps) return -1;
        return 0;
    }

    float dist(CCPoint a, CCPoint b) {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    bool finite(CCPoint p) {
        return std::isfinite(p.x) && std::isfinite(p.y);
    }

    PointNode* nodeAt(CCArray* arr, int index) {
        return static_cast<PointNode*>(arr->objectAtIndex(index));
    }

    bool lastPoint(HardStreak* streak, CCPoint& out) {
        auto* arr = streak->m_pointArray;
        if (!arr || arr->count() == 0) return false;
        out = nodeAt(arr, static_cast<int>(arr->count()) - 1)->m_point;
        return true;
    }

    void resetAll() {
        g_track[0].reset();
        g_track[1].reset();
    }

    bool platformerLevel() {
        auto* pl = PlayLayer::get();
        return pl && pl->m_level && pl->m_level->isPlatformer();
    }

    void sanitize(HardStreak* streak) {
        auto* arr = streak->m_pointArray;
        if (!arr) return;

        for (int i = static_cast<int>(arr->count()) - 1; i >= 0; i--) {
            if (!finite(nodeAt(arr, i)->m_point)) {
                arr->removeObjectAtIndex(i);
                continue;
            }
            if (i >= 1 && dist(nodeAt(arr, i)->m_point, nodeAt(arr, i - 1)->m_point) < kMerge) {
                arr->removeObjectAtIndex(i);
            }
        }

        for (int i = static_cast<int>(arr->count()) - 2; i >= 1; i--) {
            const CCPoint a = nodeAt(arr, i - 1)->m_point;
            const CCPoint b = nodeAt(arr, i)->m_point;
            const CCPoint c = nodeAt(arr, i + 1)->m_point;
            const float l1 = dist(a, b);
            const float l2 = dist(b, c);
            if (l1 < 0.0001f || l2 < 0.0001f) continue;
            const float cosine = ((b.x - a.x) * (c.x - b.x) + (b.y - a.y) * (c.y - b.y)) / (l1 * l2);
            if (cosine < kSpikeCos) arr->removeObjectAtIndex(i);
        }

        if (arr->count() >= 2 && !platformerLevel()) {
            const float span = nodeAt(arr, static_cast<int>(arr->count()) - 1)->m_point.x - nodeAt(arr, 0)->m_point.x;
            if (std::fabs(span) > 1.f) {
                const float dir = span > 0.f ? 1.f : -1.f;
                for (int i = static_cast<int>(arr->count()) - 1; i >= 1; i--) {
                    const float dx = nodeAt(arr, i)->m_point.x - nodeAt(arr, i - 1)->m_point.x;
                    if (dx * dir < -0.01f) arr->removeObjectAtIndex(i);
                }
            }
        }
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

        if (!finite(pos)) {
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
        if (turned) {
            CCPoint tail;
            const bool covered = lastPoint(m_waveTrail, tail) && dist(tail, track.last) < kCornerGap;
            if (!covered) m_waveTrail->addPoint(track.last);
        }

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
    void addPoint(CCPoint point) {
        if (!fixEnabled()) {
            HardStreak::addPoint(point);
            return;
        }

        if (!finite(point)) return;

        CCPoint tail;
        if (lastPoint(this, tail) && dist(tail, point) < kMerge) return;

        HardStreak::addPoint(point);
    }

    void updateStroke(float dt) {
        if (!fixEnabled()) {
            HardStreak::updateStroke(dt);
            return;
        }

        sanitize(this);

        if (!finite(m_currentPoint)) {
            HardStreak::updateStroke(dt);
            return;
        }

        CCPoint tail;
        if (!lastPoint(this, tail) || dist(tail, m_currentPoint) >= kMerge) {
            HardStreak::updateStroke(dt);
            return;
        }

        CCPoint dir = CCPoint(1.f, 0.f);
        auto* arr = m_pointArray;
        if (arr && arr->count() >= 2) {
            const CCPoint prev = nodeAt(arr, static_cast<int>(arr->count()) - 2)->m_point;
            const float len = dist(tail, prev);
            if (len > 0.0001f) dir = CCPoint((tail.x - prev.x) / len, (tail.y - prev.y) / len);
        }

        const CCPoint saved = m_currentPoint;
        m_currentPoint = CCPoint(tail.x + dir.x * kHeadNudge, tail.y + dir.y * kHeadNudge);
        HardStreak::updateStroke(dt);
        m_currentPoint = saved;
    }
};
