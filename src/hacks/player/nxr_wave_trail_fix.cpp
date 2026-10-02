#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include "../../core/nxr_config.hpp"

using namespace geode::prelude;

namespace {
    // Last point we pushed for P1 / P2, used to avoid sending the same point twice
    CCPoint g_last[2];
    bool g_hasLast[2] = {false, false};

    void resetLast() {
        g_hasLast[0] = g_hasLast[1] = false;
    }
}

class $modify(NXRWaveTrailFixPlayerObject, PlayerObject) {
    void update(float dt) {
        PlayerObject::update(dt);

        // Toggle lives in Bot > Settings
        if (!NXRConfig::get().get<bool>("nxr.bot.wave_trail_fix", true)) return;

        // Only the real players: the trajectory preview uses fake PlayerObjects that must be left alone
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
