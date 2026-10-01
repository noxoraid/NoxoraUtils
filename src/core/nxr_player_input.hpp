#pragma once
#include <Geode/Geode.hpp>

namespace NXR::Input {
    inline bool g_injecting = false;

    inline void press(PlayerObject* player) {
        if (!player) return;
        g_injecting = true;
        player->pushButton(PlayerButton::Jump);
        g_injecting = false;
    }

    inline void release(PlayerObject* player) {
        if (!player) return;
        g_injecting = true;
        player->releaseButton(PlayerButton::Jump);
        g_injecting = false;
    }

    inline int slotOf(PlayerObject* player) {
        auto* pl = PlayLayer::get();
        if (!pl) return 0;
        if (player == pl->m_player1) return 1;
        if (player == pl->m_player2) return 2;
        return 0;
    }
}
