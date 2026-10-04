#pragma once
#include <Geode/Geode.hpp>
#include <array>
#include <cstdint>

namespace NXR::Stats {

    enum class Cat : uint8_t {
        Other,
        Solid,
        Hazard,
        Orb,
        Pad,
        Portal,
        Coin,
        Deco,
    };

    struct LevelStats {
        int levelId = 0;
        uint32_t objects = 0;
        uint32_t solids = 0;
        uint32_t hazards = 0;
        uint32_t decorations = 0;
        uint32_t orbs = 0;
        uint32_t pads = 0;
        uint32_t portals = 0;
        uint32_t gravityPortals = 0;
        uint32_t speedPortals = 0;
        uint32_t modePortals = 0;
        uint32_t sizePortals = 0;
        uint32_t mirrorPortals = 0;
        uint32_t dualPortals = 0;
        uint32_t coins = 0;
        uint32_t triggers = 0;
        uint32_t startPositions = 0;

        uint32_t interactables() const { return orbs + pads + portals; }

        static constexpr size_t kFieldCount = 17;

        std::array<uint32_t, kFieldCount> toArray() const {
            return {static_cast<uint32_t>(levelId), objects, solids, hazards, decorations, orbs, pads, portals, gravityPortals,
                    speedPortals, modePortals, sizePortals, mirrorPortals, dualPortals, coins, triggers, startPositions};
        }

        static LevelStats fromArray(const std::array<uint32_t, kFieldCount>& v) {
            LevelStats s;
            s.levelId = static_cast<int>(v[0]);
            s.objects = v[1]; s.solids = v[2]; s.hazards = v[3]; s.decorations = v[4];
            s.orbs = v[5]; s.pads = v[6]; s.portals = v[7]; s.gravityPortals = v[8];
            s.speedPortals = v[9]; s.modePortals = v[10]; s.sizePortals = v[11];
            s.mirrorPortals = v[12]; s.dualPortals = v[13]; s.coins = v[14];
            s.triggers = v[15]; s.startPositions = v[16];
            return s;
        }
    };

    bool isSpeedPortalId(int id);
    Cat categorize(GameObject* object);

    LevelStats compute(PlayLayer* layer);
    uint64_t fingerprint(PlayLayer* layer);
    void remember(const LevelStats& stats);
    bool cachedFor(int levelId, LevelStats& out);
    bool lastCached(LevelStats& out);
}
