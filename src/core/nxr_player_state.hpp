#pragma once
#include <Geode/Geode.hpp>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>
#include <vector>
#include "nxr_practice_fix.hpp"

namespace NXR::Capture {

    struct PlayerState {
        float x = 0.f;
        float y = 0.f;
        float rot = 0.f;
        float yVel = 0.f;
        float xVel = 0.f;
        uint32_t flags = 0;
    };

    inline float wrapRotation(float rotation) {
        if (!std::isfinite(rotation)) return 0.f;
        rotation = std::fmod(rotation, 360.f);
        if (rotation < 0.f) rotation += 360.f;
        return rotation;
    }

    template <class P>
    inline float readXVel(P& player) {
        if constexpr (requires { player.m_platformerXVelocity; }) return static_cast<float>(player.m_platformerXVelocity);
        else return 0.f;
    }

    template <class P>
    inline void writeXVel(P& player, float value) {
        if constexpr (requires { player.m_platformerXVelocity = value; }) player.m_platformerXVelocity = value;
    }

    constexpr uint32_t kModeShift = 8;
    constexpr uint32_t kModeMask = 7u << kModeShift;
    constexpr uint32_t kMiniBit = 1u << 11;
    constexpr uint32_t kSpeedShift = 12;
    constexpr uint32_t kSpeedMask = 7u << kSpeedShift;
    constexpr uint32_t kModeValid = 1u << 15;

    inline int modeOf(PlayerObject& p) {
        if (p.m_isShip) return 1;
        if (p.m_isBall) return 2;
        if (p.m_isBird) return 3;
        if (p.m_isDart) return 4;
        if (p.m_isRobot) return 5;
        if (p.m_isSpider) return 6;
        if (p.m_isSwing) return 7;
        return 0;
    }

    inline int speedIndexOf(float speed) {
        constexpr float values[5] = {0.7f, 0.9f, 1.1f, 1.3f, 1.6f};
        for (int i = 0; i < 5; i++) {
            if (std::fabs(speed - values[i]) < 0.001f) return i;
        }
        return 7;
    }

    inline float speedFromIndex(int index) {
        constexpr float values[5] = {0.7f, 0.9f, 1.1f, 1.3f, 1.6f};
        return (index >= 0 && index < 5) ? values[index] : 0.f;
    }

    inline uint32_t readModeBits(PlayerObject& p) {
        uint32_t f = kModeValid;
        f |= (static_cast<uint32_t>(modeOf(p)) << kModeShift) & kModeMask;
        if (p.m_vehicleSize < 0.9f) f |= kMiniBit;
        f |= (static_cast<uint32_t>(speedIndexOf(p.m_playerSpeed)) << kSpeedShift) & kSpeedMask;
        return f;
    }

    template <class P>
    inline uint32_t readFlags(P& p) {
        uint32_t f = 0;
        if constexpr (requires { p.m_isOnGround; }) { if (p.m_isOnGround) f |= 1u; }
        if constexpr (requires { p.m_isUpsideDown; }) { if (p.m_isUpsideDown) f |= 2u; }
        if constexpr (requires { p.m_isDashing; }) { if (p.m_isDashing) f |= 4u; }
        if constexpr (requires { p.m_isSliding; }) { if (p.m_isSliding) f |= 8u; }
        if constexpr (requires { p.m_isOnSlope; }) { if (p.m_isOnSlope) f |= 16u; }
        if constexpr (requires { p.m_touchedPad; }) { if (p.m_touchedPad) f |= 32u; }
        if constexpr (requires { p.m_touchedRing; }) { if (p.m_touchedRing) f |= 64u; }
        if constexpr (requires { p.m_jumpBuffered; }) { if (p.m_jumpBuffered) f |= 128u; }
        if constexpr (std::is_same_v<std::remove_cvref_t<P>, PlayerObject>) f |= readModeBits(p);
        return f;
    }

    template <class P>
    inline void writeFlags(P& p, uint32_t f) {
        if constexpr (requires { p.m_isOnGround = true; }) p.m_isOnGround = (f & 1u) != 0;
        if constexpr (requires { p.m_isUpsideDown = true; }) p.m_isUpsideDown = (f & 2u) != 0;
        if constexpr (requires { p.m_isSliding = true; }) p.m_isSliding = (f & 8u) != 0;
        if constexpr (requires { p.m_isOnSlope = true; }) p.m_isOnSlope = (f & 16u) != 0;
        if constexpr (requires { p.m_jumpBuffered = true; }) p.m_jumpBuffered = (f & 128u) != 0;
    }

    template <class P>
    inline PlayerState readState(P* p) {
        PlayerState s;
        if (!p) return s;
        const auto pos = p->getPosition();
        s.x = pos.x;
        s.y = pos.y;
        s.rot = wrapRotation(p->getRotation());
        s.yVel = static_cast<float>(p->m_yVelocity);
        s.xVel = readXVel(*p);
        s.flags = readFlags(*p);
        return s;
    }

    template <class P>
    inline void writeState(P* p, const PlayerState& s, bool full, bool platformer) {
        if (!p || s.x == 0.f || s.y == 0.f) return;
        p->setPosition({s.x, s.y});
        if (s.rot != 0.f) p->setRotation(s.rot);
        if (!full) return;
        p->m_yVelocity = s.yVel;
        if (platformer && std::isfinite(s.xVel)) writeXVel(*p, s.xVel);
        writeFlags(*p, s.flags);
    }

    template <class T>
    inline constexpr bool kPortable = (std::is_arithmetic_v<T> || std::is_enum_v<T>) && std::is_trivially_copyable_v<T>;

    struct BlobWriter {
        std::vector<uint8_t>* out;

        template <class T>
        void put(const T& value) {
            const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
            out->insert(out->end(), bytes, bytes + sizeof(T));
        }
    };

    struct BlobReader {
        const uint8_t* cursor;
        const uint8_t* end;
        bool ok = true;

        template <class T>
        void get(T& value) {
            if (!ok || static_cast<size_t>(end - cursor) < sizeof(T)) {
                ok = false;
                return;
            }
            std::memcpy(&value, cursor, sizeof(T));
            cursor += sizeof(T);
        }
    };

    template <class P>
    inline void takeBlob(P& p, std::vector<uint8_t>& out) {
        out.clear();
        BlobWriter w{&out};
#define X(f) \
        if constexpr (requires { p.f; requires !std::is_const_v<decltype(p.f)>; }) { \
            using T = std::remove_cvref_t<decltype(p.f)>; \
            if constexpr (kPortable<T>) { T v = p.f; w.put(v); } \
            else if constexpr (std::is_same_v<T, cocos2d::CCPoint>) { T v = p.f; w.put(v.x); w.put(v.y); } \
        }
        NXR_PLAYER_FIELDS(X)
#undef X
    }

    template <class P>
    inline uint32_t blobLayout(size_t* bytes = nullptr) {
        uint32_t h = 2166136261u;
        size_t total = 0;
        auto mix = [&](const char* name, size_t size) {
            for (const char* c = name; *c; ++c) {
                h ^= static_cast<uint8_t>(*c);
                h *= 16777619u;
            }
            h ^= static_cast<uint32_t>(size);
            h *= 16777619u;
            total += size;
        };
#define X(f) \
        if constexpr (requires(P& q) { q.f; requires !std::is_const_v<decltype(q.f)>; }) { \
            using T = std::remove_cvref_t<decltype(std::declval<P&>().f)>; \
            if constexpr (kPortable<T>) mix(#f, sizeof(T)); \
            else if constexpr (std::is_same_v<T, cocos2d::CCPoint>) mix(#f, 2 * sizeof(float)); \
        }
        NXR_PLAYER_FIELDS(X)
#undef X
        if (bytes) *bytes = total;
        return h;
    }

    template <class P>
    inline bool putBlob(P& p, const std::vector<uint8_t>& data) {
        size_t expected = 0;
        blobLayout<P>(&expected);
        if (data.size() != expected) return false;

        BlobReader r{data.data(), data.data() + data.size()};
#define X(f) \
        if constexpr (requires { p.f; requires !std::is_const_v<decltype(p.f)>; }) { \
            using T = std::remove_cvref_t<decltype(p.f)>; \
            if constexpr (kPortable<T>) { T v{}; r.get(v); if (r.ok) p.f = v; } \
            else if constexpr (std::is_same_v<T, cocos2d::CCPoint>) { float a = 0.f; float b = 0.f; r.get(a); r.get(b); if (r.ok) p.f = T(a, b); } \
        }
        NXR_PLAYER_FIELDS(X)
#undef X
        return r.ok;
    }
}
