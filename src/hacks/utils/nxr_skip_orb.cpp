#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <string>
#include <unordered_set>
#include <vector>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_orb_assist.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"

NXR_HACK_CREATE(
    "Utils", "Skip Orb",
    "Pick which orbs, dashes and portals you skip. No Touch: selected orbs and dashes are never touched while you play or record, "
    "and in playback they pulse as if clicked. Auto Click: they are clicked for you and the click is stored in the macro. "
    "Selected portals are ignored and never change your gamemode",
    true
);

namespace {
    using GOT = GameObjectType;

    NXR::Orb::Tracker g_orbP1;
    NXR::Orb::Tracker g_orbP2;
    NXR::Orb::Tracker g_dashP1;
    NXR::Orb::Tracker g_dashP2;
    std::unordered_set<const void*> g_seenP1;
    std::unordered_set<const void*> g_seenP2;

    constexpr const char* kPrefix = "nxr.utils.skip_orb::";

    bool opt(const char* name, bool def) {
        return NXRConfig::get().get<bool>(std::string(kPrefix) + name, def);
    }

    bool isDashType(GOT t) {
        return t == GOT::DashRing || t == GOT::GravityDashRing;
    }

    bool isOrbType(GOT t) {
        return t == GOT::YellowJumpRing || t == GOT::PinkJumpRing || t == GOT::GravityRing
            || t == GOT::GreenRing || t == GOT::RedJumpRing || t == GOT::DropRing || t == GOT::CustomRing;
    }

    bool orbSelected(GameObject* obj) {
        const auto t = obj->m_objectType;
        if (!isOrbType(t)) return false;
        if (opt("all_orbs", false)) return true;
        switch (t) {
            case GOT::YellowJumpRing: return opt("yellow", true);
            case GOT::PinkJumpRing: return opt("pink", true);
            case GOT::GravityRing: return opt("blue", true);
            case GOT::GreenRing: return opt("green", true);
            case GOT::RedJumpRing: return opt("red", true);
            case GOT::DropRing: return opt("black", true);
            case GOT::CustomRing: return opt("custom", false);
            default: return false;
        }
    }

    bool dashSelected(GameObject* obj) {
        const auto t = obj->m_objectType;
        if (!isDashType(t)) return false;
        if (opt("all_dash", false)) return true;
        if (t == GOT::DashRing) return opt("green_dash", true);
        return opt("pink_dash", true);
    }

    bool isSpeedPortal(GameObject* obj) {
        const int id = obj->m_objectID;
        return id == 200 || id == 201 || id == 202 || id == 203 || id == 1334;
    }

    bool isPortalType(GOT t) {
        return t == GOT::CubePortal || t == GOT::ShipPortal || t == GOT::BallPortal || t == GOT::UfoPortal
            || t == GOT::WavePortal || t == GOT::RobotPortal || t == GOT::SpiderPortal || t == GOT::SwingPortal
            || t == GOT::TeleportPortal || t == GOT::DualPortal || t == GOT::SoloPortal
            || t == GOT::GravityTogglePortal || t == GOT::NormalGravityPortal || t == GOT::InverseGravityPortal
            || t == GOT::NormalMirrorPortal || t == GOT::InverseMirrorPortal
            || t == GOT::MiniSizePortal || t == GOT::RegularSizePortal;
    }

    bool portalSelected(GameObject* obj) {
        const auto t = obj->m_objectType;
        if (isSpeedPortal(obj)) return opt("all_portals", false) || opt("speed", false);
        if (!isPortalType(t)) return false;
        if (opt("all_portals", false)) return true;
        switch (t) {
            case GOT::CubePortal: return opt("cube", false);
            case GOT::ShipPortal: return opt("ship", false);
            case GOT::BallPortal: return opt("ball", false);
            case GOT::UfoPortal: return opt("ufo", false);
            case GOT::WavePortal: return opt("wave", false);
            case GOT::RobotPortal: return opt("robot", false);
            case GOT::SpiderPortal: return opt("spider", false);
            case GOT::SwingPortal: return opt("swing", false);
            case GOT::TeleportPortal: return opt("teleport", false);
            case GOT::DualPortal:
            case GOT::SoloPortal: return opt("dual", false);
            case GOT::GravityTogglePortal:
            case GOT::NormalGravityPortal:
            case GOT::InverseGravityPortal: return opt("gravity", false);
            case GOT::NormalMirrorPortal:
            case GOT::InverseMirrorPortal: return opt("mirror", false);
            case GOT::MiniSizePortal:
            case GOT::RegularSizePortal: return opt("size", false);
            default: return false;
        }
    }

    int skipMode() {
        return NXRConfig::get().get<int>("nxr.utils.skip_orb::mode", 0);
    }

    bool botPlaying() {
        return NXR::Bot::State::get().mode == NXR::Bot::Mode::Playing;
    }

    bool playerEnabled(bool p2) {
        return opt(p2 ? "p2" : "p1", true);
    }
}

class $modify(NXRSkipOrbGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Utils").findHackByName("Skip Orb");
        NXR::trySetPriority(self, "GJBaseGameLayer::processCommands", -10);
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::processCommands");
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::canBeActivatedByPlayer");
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::playerTouchedRing");

        hack.setForm([
            mode = hack.formatAdditionalSetting("mode"),
            p1 = hack.formatAdditionalSetting("p1"),
            p2 = hack.formatAdditionalSetting("p2"),
            allOrbs = hack.formatAdditionalSetting("all_orbs"),
            yellow = hack.formatAdditionalSetting("yellow"),
            pink = hack.formatAdditionalSetting("pink"),
            blue = hack.formatAdditionalSetting("blue"),
            green = hack.formatAdditionalSetting("green"),
            red = hack.formatAdditionalSetting("red"),
            black = hack.formatAdditionalSetting("black"),
            custom = hack.formatAdditionalSetting("custom"),
            allDash = hack.formatAdditionalSetting("all_dash"),
            greenDash = hack.formatAdditionalSetting("green_dash"),
            pinkDash = hack.formatAdditionalSetting("pink_dash"),
            allPortals = hack.formatAdditionalSetting("all_portals"),
            cube = hack.formatAdditionalSetting("cube"),
            ship = hack.formatAdditionalSetting("ship"),
            ball = hack.formatAdditionalSetting("ball"),
            ufo = hack.formatAdditionalSetting("ufo"),
            wave = hack.formatAdditionalSetting("wave"),
            robot = hack.formatAdditionalSetting("robot"),
            spider = hack.formatAdditionalSetting("spider"),
            swing = hack.formatAdditionalSetting("swing"),
            gravity = hack.formatAdditionalSetting("gravity"),
            size = hack.formatAdditionalSetting("size"),
            dual = hack.formatAdditionalSetting("dual"),
            mirror = hack.formatAdditionalSetting("mirror"),
            speed = hack.formatAdditionalSetting("speed"),
            teleport = hack.formatAdditionalSetting("teleport")
        ](NXR::Form& form) {
            auto* f = &form;
            f->addConfigRadio("Orb / Dash Mode", mode, {{"No Touch", 0}, {"Auto Click", 1}}, 0);
            f->addSeparator();
            f->addConfigToggle("Player 1", p1, true);
            f->addConfigToggle("Player 2", p2, true);
            f->addSeparator();
            f->addConfigToggle("All Orbs", allOrbs, false);
            f->addConfigToggle("Yellow Orb", yellow, true);
            f->addConfigToggle("Pink / Purple Orb", pink, true);
            f->addConfigToggle("Blue Orb", blue, true);
            f->addConfigToggle("Green Orb", green, true);
            f->addConfigToggle("Red Orb", red, true);
            f->addConfigToggle("Black Orb", black, true);
            f->addConfigToggle("Spider / Custom Orb", custom, false);
            f->addSeparator();
            f->addConfigToggle("All Dash Orbs", allDash, false);
            f->addConfigToggle("Green Dash Orb", greenDash, true);
            f->addConfigToggle("Pink Dash Orb", pinkDash, true);
            f->addSeparator();
            f->addConfigToggle("All Portals", allPortals, false);
            f->addConfigToggle("Cube Portal", cube, false);
            f->addConfigToggle("Ship Portal", ship, false);
            f->addConfigToggle("Ball Portal", ball, false);
            f->addConfigToggle("UFO Portal", ufo, false);
            f->addConfigToggle("Wave Portal", wave, false);
            f->addConfigToggle("Robot Portal", robot, false);
            f->addConfigToggle("Spider Portal", spider, false);
            f->addConfigToggle("Swing Portal", swing, false);
            f->addConfigToggle("Gravity Portals", gravity, false);
            f->addConfigToggle("Size Portals (Mini / Big)", size, false);
            f->addConfigToggle("Dual / Solo Portals", dual, false);
            f->addConfigToggle("Mirror Portals", mirror, false);
            f->addConfigToggle("Speed Portals", speed, false);
            f->addConfigToggle("Teleport Portal", teleport, false);
        });
    }

    void playerTouchedRing(PlayerObject* player, RingObject* object) {
        auto* pl = PlayLayer::get();
        if (object && pl && static_cast<GJBaseGameLayer*>(pl) == this && skipMode() == 0) {
            const bool isP2 = player == m_player2 && player != m_player1;
            if (playerEnabled(isP2) && (orbSelected(object) || dashSelected(object))) {
                if (botPlaying()) {
                    auto& seen = isP2 ? g_seenP2 : g_seenP1;
                    if (seen.insert(object).second) object->spawnCircle();
                }
                return;
            }
        }
        GJBaseGameLayer::playerTouchedRing(player, object);
    }

    bool canBeActivatedByPlayer(PlayerObject* player, EffectGameObject* object) {
        auto* pl = PlayLayer::get();
        if (object && pl && static_cast<GJBaseGameLayer*>(pl) == this && portalSelected(object)) {
            const bool isP2 = player == m_player2 && player != m_player1;
            if (playerEnabled(isP2)) return false;
        }
        return GJBaseGameLayer::canBeActivatedByPlayer(player, object);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto* pl = PlayLayer::get();
        if (pl && static_cast<GJBaseGameLayer*>(pl) == this && !isHalfTick && skipMode() == 1) {
            if (NXR::Orb::gameActive()) {
                auto orbMatch = [](GameObject* obj) { return orbSelected(obj); };
                auto dashMatch = [](GameObject* obj) { return dashSelected(obj); };

                if (playerEnabled(false)) {
                    NXR::Orb::tick(m_player1, g_dashP1, true, dashMatch);
                    if (!g_dashP1.holding) NXR::Orb::tick(m_player1, g_orbP1, false, orbMatch);
                } else {
                    g_orbP1.reset();
                    g_dashP1.reset();
                }

                if (m_gameState.m_isDualMode && playerEnabled(true)) {
                    NXR::Orb::tick(m_player2, g_dashP2, true, dashMatch);
                    if (!g_dashP2.holding) NXR::Orb::tick(m_player2, g_orbP2, false, orbMatch);
                } else {
                    g_orbP2.reset();
                    g_dashP2.reset();
                }
            } else {
                g_orbP1.holding = false;
                g_orbP2.holding = false;
                g_dashP1.holding = false;
                g_dashP2.holding = false;
            }
        }

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};

class $modify(NXRSkipOrbPlayLayer, PlayLayer) {
    void resetLevel() {
        g_orbP1.reset();
        g_orbP2.reset();
        g_dashP1.reset();
        g_dashP2.reset();
        g_seenP1.clear();
        g_seenP2.clear();
        PlayLayer::resetLevel();
    }
};
