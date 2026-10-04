#include <cmath>
#include "nxr_level_stats.hpp"
#include <Geode/modify/PlayLayer.hpp>
#include <map>
#include <mutex>

namespace {
    std::mutex g_mutex;
    std::map<int, NXR::Stats::LevelStats> g_levels;
    int g_lastId = 0;
    bool g_hasLast = false;
}

bool NXR::Stats::isSpeedPortalId(int id) {
    return id == 200 || id == 201 || id == 202 || id == 203 || id == 1334;
}

NXR::Stats::Cat NXR::Stats::categorize(GameObject* object) {
    if (!object) return Cat::Other;
    switch (object->m_objectType) {
        case GameObjectType::Solid:
        case GameObjectType::Slope:
            return Cat::Solid;
        case GameObjectType::Hazard:
        case GameObjectType::AnimatedHazard:
            return Cat::Hazard;
        case GameObjectType::YellowJumpRing:
        case GameObjectType::PinkJumpRing:
        case GameObjectType::RedJumpRing:
        case GameObjectType::GravityRing:
        case GameObjectType::GreenRing:
        case GameObjectType::DropRing:
        case GameObjectType::CustomRing:
        case GameObjectType::DashRing:
        case GameObjectType::GravityDashRing:
            return Cat::Orb;
        case GameObjectType::YellowJumpPad:
        case GameObjectType::PinkJumpPad:
        case GameObjectType::RedJumpPad:
        case GameObjectType::GravityPad:
            return Cat::Pad;
        case GameObjectType::InverseGravityPortal:
        case GameObjectType::NormalGravityPortal:
        case GameObjectType::ShipPortal:
        case GameObjectType::CubePortal:
        case GameObjectType::BallPortal:
        case GameObjectType::UfoPortal:
        case GameObjectType::WavePortal:
        case GameObjectType::RobotPortal:
        case GameObjectType::SpiderPortal:
        case GameObjectType::SwingPortal:
        case GameObjectType::RegularSizePortal:
        case GameObjectType::MiniSizePortal:
        case GameObjectType::InverseMirrorPortal:
        case GameObjectType::NormalMirrorPortal:
        case GameObjectType::DualPortal:
        case GameObjectType::SoloPortal:
        case GameObjectType::TeleportPortal:
            return Cat::Portal;
        case GameObjectType::SecretCoin:
        case GameObjectType::UserCoin:
            return Cat::Coin;
        case GameObjectType::Decoration:
            return Cat::Deco;
        case GameObjectType::Modifier:
            return isSpeedPortalId(object->m_objectID) ? Cat::Portal : Cat::Other;
        default:
            return Cat::Other;
    }
}

NXR::Stats::LevelStats NXR::Stats::compute(PlayLayer* layer) {
    LevelStats stats;
    if (!layer || !layer->m_level) return stats;

    stats.levelId = layer->m_level->m_levelID.value();
    if (!layer->m_objects) return stats;

    stats.objects = static_cast<uint32_t>(layer->m_objects->count());

    for (auto* object : geode::cocos::CCArrayExt<GameObject*>(layer->m_objects)) {
        if (!object) continue;

        if (object->m_objectID == 31) stats.startPositions++;
        if (object->m_isTrigger) stats.triggers++;

        switch (categorize(object)) {
            case Cat::Solid: stats.solids++; break;
            case Cat::Hazard: stats.hazards++; break;
            case Cat::Deco: stats.decorations++; break;
            case Cat::Coin: stats.coins++; break;
            case Cat::Orb: stats.orbs++; break;
            case Cat::Pad: stats.pads++; break;
            case Cat::Portal: {
                stats.portals++;
                switch (object->m_objectType) {
                    case GameObjectType::InverseGravityPortal:
                    case GameObjectType::NormalGravityPortal:
                        stats.gravityPortals++;
                        break;
                    case GameObjectType::RegularSizePortal:
                    case GameObjectType::MiniSizePortal:
                        stats.sizePortals++;
                        break;
                    case GameObjectType::InverseMirrorPortal:
                    case GameObjectType::NormalMirrorPortal:
                        stats.mirrorPortals++;
                        break;
                    case GameObjectType::DualPortal:
                    case GameObjectType::SoloPortal:
                        stats.dualPortals++;
                        break;
                    case GameObjectType::Modifier:
                        stats.speedPortals++;
                        break;
                    case GameObjectType::TeleportPortal:
                        break;
                    default:
                        stats.modePortals++;
                        break;
                }
                break;
            }
            default:
                break;
        }
    }

    return stats;
}

namespace {
    uint64_t mix64(uint64_t x) {
        x += 0x9e3779b97f4a7c15ull;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
        return x ^ (x >> 31);
    }

    cocos2d::CCPoint startPointOf(GameObject* object) {
        if constexpr (requires { object->m_startPosition; }) return object->m_startPosition;
        return object->getPosition();
    }
}

uint64_t NXR::Stats::fingerprint(PlayLayer* layer) {
    if (!layer || !layer->m_objects) return 0;

    uint64_t sum = 0;
    uint64_t count = 0;
    for (auto* object : geode::cocos::CCArrayExt<GameObject*>(layer->m_objects)) {
        if (!object) continue;
        if (categorize(object) == Cat::Deco) continue;

        const auto p = startPointOf(object);
        const int64_t ix = static_cast<int64_t>(std::llround(p.x * 100.f));
        const int64_t iy = static_cast<int64_t>(std::llround(p.y * 100.f));
        uint64_t h = mix64(static_cast<uint64_t>(static_cast<int64_t>(object->m_objectID)));
        h = mix64(h ^ static_cast<uint64_t>(ix));
        h = mix64(h ^ static_cast<uint64_t>(iy) * 0x100000001b3ull);
        sum += h;
        count++;
    }

    const uint64_t result = mix64(sum ^ (count * 0x9e3779b97f4a7c15ull));
    return result == 0 ? 1 : result;
}

void NXR::Stats::remember(const LevelStats& stats) {
    std::lock_guard lock(g_mutex);
    g_levels[stats.levelId] = stats;
    g_lastId = stats.levelId;
    g_hasLast = true;
}

bool NXR::Stats::cachedFor(int levelId, LevelStats& out) {
    std::lock_guard lock(g_mutex);
    auto it = g_levels.find(levelId);
    if (it == g_levels.end()) return false;
    out = it->second;
    return true;
}

bool NXR::Stats::lastCached(LevelStats& out) {
    std::lock_guard lock(g_mutex);
    if (!g_hasLast) return false;
    auto it = g_levels.find(g_lastId);
    if (it == g_levels.end()) return false;
    out = it->second;
    return true;
}

class $modify(NXRLevelStatsPlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        NXR::Stats::remember(NXR::Stats::compute(this));
    }
};
