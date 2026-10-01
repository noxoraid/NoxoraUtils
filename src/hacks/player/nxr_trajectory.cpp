#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <vector>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_utils.hpp"
#include "../../core/nxr_practice_fix.hpp"
#include "../../interface/imgui/nxr_widget_helper.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"
#include "imgui.h"

using namespace geode::prelude;

NXR_HACK_CREATE(
    "Player", "Show Trajectory",
    "Draws the real path the player will take, simulated with the game's own physics: one line if you hold jump and one if you release. The path stops with a hitbox where the player would die",
    false
);

namespace {
    constexpr float kStepDelta = 0.25f;
    constexpr int kFadeSteps = 40;

    const std::unordered_set<int> kPortalIDs = { 101, 99, 11, 10, 200, 201, 202, 203, 1334 };
    const std::unordered_set<int> kObjectTypes = { 0, 2, 47, 25 };

    bool isCollectible(int id) {
        switch (id) {
            case 1275: case 1329: case 1587: case 1589: case 1598: case 1614: case 3601:
                return true;
            default:
                return id >= 4401 && id <= 4539;
        }
    }

    struct Palette {
        ccColor4F hold;
        ccColor4F release;
        ccColor4F merged;
    };

    struct SimState {
        Ref<PlayerObject> fake1;
        Ref<PlayerObject> fake2;
        Ref<CCDrawNode> node;
        bool creating = false;
        bool cancelled = false;
        float deathRotation = 0.f;
        int tick = 0;
        std::vector<CCPoint> holdPath;
    };

    SimState g_sim;

    const std::string& enabledKey() {
        static const std::string key = NXR::Gui::get().getWindow("Player").findHackByName("Show Trajectory").getID();
        return key;
    }

    std::string settingKey(const char* name) {
        return enabledKey() + "::" + name;
    }

    bool enabled() {
        return NXRConfig::get().get<bool>(enabledKey(), false);
    }

    ccColor4F toColor4F(const ccColor3B& color, float alpha) {
        return ccc4f(
            static_cast<float>(color.r) / 255.f,
            static_cast<float>(color.g) / 255.f,
            static_cast<float>(color.b) / 255.f,
            alpha
        );
    }

    Palette palette() {
        auto& config = NXRConfig::get();
        const ccColor4F hold = toColor4F(NXR::Utils::hexToColor(config.get<std::string>(settingKey("hold_color"), "39FF6E")), 1.f);
        const ccColor4F release = toColor4F(NXR::Utils::hexToColor(config.get<std::string>(settingKey("release_color"), "FF3B3B")), 1.f);

        ccColor4F merged = { 0.f, 0.f, 0.f, 1.f };
        merged.r = std::min(1.f, (hold.r + release.r) / 2.f + 0.45f);
        merged.g = std::min(1.f, (hold.g + release.g) / 2.f + 0.45f);
        merged.b = std::min(1.f, (hold.b + release.b) / 2.f + 0.45f);

        return { hold, release, merged };
    }

    void hideSim() {
        if (!g_sim.node) return;
        g_sim.node->clear();
        g_sim.node->setVisible(false);
    }

    void resetSim() {
        if (g_sim.node && g_sim.node->getParent()) g_sim.node->removeFromParent();
        if (g_sim.fake1 && g_sim.fake1->getParent()) g_sim.fake1->removeFromParent();
        if (g_sim.fake2 && g_sim.fake2->getParent()) g_sim.fake2->removeFromParent();
        g_sim = SimState{};
    }

    bool ensureSim(PlayLayer* pl) {
        auto* layer = pl->m_objectLayer;
        if (!layer) return false;

        if (g_sim.node && g_sim.fake1 && g_sim.fake2
            && g_sim.node->getParent() == layer
            && g_sim.fake1->getParent() == layer
            && g_sim.fake2->getParent() == layer) {
            return true;
        }

        resetSim();

        auto makeFake = [pl, layer]() -> PlayerObject* {
            auto* fake = PlayerObject::create(1, 1, pl, pl, true);
            if (!fake) return nullptr;
            fake->setPosition({ 0.f, 105.f });
            fake->setVisible(false);
            layer->addChild(fake);
            return fake;
        };

        auto* first = makeFake();
        auto* second = makeFake();
        if (!first || !second) return false;

        g_sim.fake1 = first;
        g_sim.fake2 = second;

        auto* node = CCDrawNode::create();
        ccBlendFunc blend;
        blend.src = GL_SRC_ALPHA;
        blend.dst = GL_ONE_MINUS_SRC_ALPHA;
        node->setBlendFunc(blend);
        layer->addChild(node, 500);
        g_sim.node = node;

        return true;
    }

    void handlePortal(PlayerObject* player, int id) {
        if (!kPortalIDs.contains(id)) return;

        switch (id) {
            case 101:
                player->togglePlayerScale(true, true);
                player->updatePlayerScale();
                break;
            case 99:
                player->togglePlayerScale(false, true);
                player->updatePlayerScale();
                break;
            case 200: player->m_playerSpeed = 0.7f; break;
            case 201: player->m_playerSpeed = 0.9f; break;
            case 202: player->m_playerSpeed = 1.1f; break;
            case 203: player->m_playerSpeed = 1.3f; break;
            case 1334: player->m_playerSpeed = 1.6f; break;
            default: break;
        }
    }

    std::vector<CCPoint> hitboxVertices(PlayerObject* player, CCRect rect, float rotation) {
        std::vector<CCPoint> vertices = {
            ccp(rect.getMinX(), rect.getMaxY()),
            ccp(rect.getMaxX(), rect.getMaxY()),
            ccp(rect.getMaxX(), rect.getMinY()),
            ccp(rect.getMinX(), rect.getMinY())
        };

        const CCPoint center = ccp(
            (rect.getMinX() + rect.getMaxX()) / 2.f,
            (rect.getMinY() + rect.getMaxY()) / 2.f
        );

        const int size = static_cast<int>(rect.getMaxX() - rect.getMinX());

        if ((size == 18 || size == 5) && player->getScale() == 1) {
            for (auto& vertex : vertices) {
                vertex.x = center.x + (vertex.x - center.x) / 0.6f;
                vertex.y = center.y + (vertex.y - center.y) / 0.6f;
            }
        }

        if ((size == 7 || size == 30 || size == 29 || size == 9) && player->getScale() != 1) {
            for (auto& vertex : vertices) {
                vertex.x = center.x + (vertex.x - center.x) * 0.6f;
                vertex.y = center.y + (vertex.y - center.y) * 0.6f;
            }
        }

        if (player->m_isDart) {
            for (auto& vertex : vertices) {
                vertex.x = center.x + (vertex.x - center.x) * 0.3f;
                vertex.y = center.y + (vertex.y - center.y) * 0.3f;
            }
        }

        const float angle = CC_DEGREES_TO_RADIANS(rotation * -1.f);
        for (auto& vertex : vertices) {
            const float x = vertex.x - center.x;
            const float y = vertex.y - center.y;

            vertex.x = center.x + (x * std::cos(angle)) - (y * std::sin(angle));
            vertex.y = center.y + (x * std::sin(angle)) + (y * std::cos(angle));
        }

        return vertices;
    }

    void drawHitbox(PlayerObject* player, const Palette& pal) {
        if (!g_sim.node) return;

        const CCRect bigRect = player->GameObject::getObjectRect();
        const CCRect smallRect = player->GameObject::getObjectRect(0.3f, 0.3f);

        auto vertices = hitboxVertices(player, bigRect, g_sim.deathRotation);
        g_sim.node->drawPolygon(vertices.data(), 4, ccc4f(pal.release.r, pal.release.g, pal.release.b, 0.2f), 0.5f, pal.release);

        vertices = hitboxVertices(player, smallRect, g_sim.deathRotation);
        g_sim.node->drawPolygon(vertices.data(), 4, ccc4f(pal.merged.r, pal.merged.g, pal.merged.b, 0.2f), 0.35f, ccc4f(pal.merged.r, pal.merged.g, pal.merged.b, 0.55f));
    }

    void clearCollisionLogs(PlayerObject* player) {
        if (player->m_collisionLogTop) player->m_collisionLogTop->removeAllObjects();
        if (player->m_collisionLogBottom) player->m_collisionLogBottom->removeAllObjects();
        if (player->m_collisionLogLeft) player->m_collisionLogLeft->removeAllObjects();
        if (player->m_collisionLogRight) player->m_collisionLogRight->removeAllObjects();
    }

    void simulate(PlayLayer* pl, PlayerObject* fake, PlayerObject* real, bool hold, int length, float radius, const Palette& pal) {
        auto& sim = g_sim;
        if (!fake || !real || !sim.node) return;

        NXR::Practice::restore(fake, NXR::Practice::capture(real));
        sim.cancelled = false;

        const bool platformer = pl->m_levelSettings && pl->m_levelSettings->m_platformerMode;
        const int fadeStart = std::max(0, length - kFadeSteps);

        if (hold) sim.holdPath.assign(static_cast<size_t>(length), CCPoint{ -1e9f, -1e9f });

        for (int i = 0; i < length; i++) {
            const CCPoint previous = fake->getPosition();

            if (hold && static_cast<size_t>(i) < sim.holdPath.size()) sim.holdPath[static_cast<size_t>(i)] = previous;

            clearCollisionLogs(fake);
            pl->checkCollisions(fake, kStepDelta, false);

            if (sim.cancelled) {
                fake->updatePlayerScale();
                drawHitbox(fake, pal);
                break;
            }

            if (i == 0) {
                if (hold) fake->pushButton(PlayerButton::Jump);
                else fake->releaseButton(PlayerButton::Jump);

                if (platformer) {
                    fake->pushButton(real->m_isGoingLeft ? PlayerButton::Left : PlayerButton::Right);
                }
            }

            fake->update(kStepDelta);
            fake->updateRotation(kStepDelta);
            fake->updatePlayerScale();

            ccColor4F color = hold ? pal.hold : pal.release;

            if (!hold && static_cast<size_t>(i) < sim.holdPath.size() && sim.holdPath[static_cast<size_t>(i)] == previous) {
                color = pal.merged;
            }

            if (i >= fadeStart) {
                color.a = static_cast<float>(length - i) / static_cast<float>(kFadeSteps);
            }

            sim.node->drawSegment(previous, fake->getPosition(), radius, color);
        }
    }

    void updateTrajectory(PlayLayer* pl) {
        auto* player1 = pl->m_player1;
        if (!player1 || player1->m_isDead || pl->m_levelEndAnimationStarted) {
            hideSim();
            return;
        }

        if (!ensureSim(pl)) return;

        auto& sim = g_sim;
        auto& config = NXRConfig::get();

        sim.node->setVisible(true);

        const int interval = std::clamp(config.get<int>(settingKey("interval"), 2), 1, 10);
        if (++sim.tick < interval) return;
        sim.tick = 0;

        const int length = std::clamp(config.get<int>(settingKey("length"), 180), 30, 480);
        const float radius = std::clamp(config.get<float>(settingKey("thickness"), 3.f), 1.f, 10.f) * 0.2f;
        const Palette pal = palette();

        sim.node->clear();
        sim.creating = true;

        simulate(pl, sim.fake1.data(), player1, true, length, radius, pal);
        simulate(pl, sim.fake2.data(), player1, false, length, radius, pal);

        auto* player2 = pl->m_player2;
        if (pl->m_gameState.m_isDualMode && player2 && !player2->m_isDead) {
            simulate(pl, sim.fake2.data(), player2, true, length, radius, pal);
            simulate(pl, sim.fake1.data(), player2, false, length, radius, pal);
        }

        sim.creating = false;
    }
}

class $modify(NXRTrajectoryPlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Player").findHackByName("Show Trajectory");

        NXR::trySetPriority(self, "PlayLayer::destroyPlayer", -1000);
        NXR::tryAddHook(self, hack, "PlayLayer::destroyPlayer");
        NXR::tryAddHook(self, hack, "PlayLayer::playEndAnimationToPos");

        hack.setCustomWindowImGui([
            holdColor = hack.formatAdditionalSetting("hold_color"),
            releaseColor = hack.formatAdditionalSetting("release_color"),
            length = hack.formatAdditionalSetting("length"),
            thickness = hack.formatAdditionalSetting("thickness"),
            interval = hack.formatAdditionalSetting("interval")
        ]{
            NXRWidgetConfig::ColorEdit3Hex("Hold Color", holdColor, "39FF6E");
            NXRWidgetConfig::ColorEdit3Hex("Release Color", releaseColor, "FF3B3B");
            NXRWidgetConfig::DragInt("##nxr_traj_length", length, 1.f, 30, 480, 180, "Length: %d steps");
            NXRWidgetConfig::DragFloat("##nxr_traj_thick", thickness, 0.2f, 1.f, 10.f, 3.f, "Line Thickness: %.1f");
            NXRWidgetConfig::DragInt("##nxr_traj_interval", interval, 1.f, 1, 10, 2, "Update Every: %d frames");
        });

        hack.setCustomWindowCocos([
            holdColor = hack.formatAdditionalSetting("hold_color"),
            releaseColor = hack.formatAdditionalSetting("release_color"),
            length = hack.formatAdditionalSetting("length"),
            thickness = hack.formatAdditionalSetting("thickness"),
            interval = hack.formatAdditionalSetting("interval")
        ](cocos2d::CCNode* node) {
            auto* popup = static_cast<NXRHackSettingsPopup*>(node);
            popup->addConfigColor3Hex("Hold Color", holdColor, "39FF6E");
            popup->addConfigColor3Hex("Release Color", releaseColor, "FF3B3B");
            popup->addConfigIntInput("Length (steps)", length, 30, 480, 180);
            popup->addConfigFloatInput("Line Thickness", thickness, 1.f, 10.f, 3.f);
            popup->addConfigIntInput("Update Every (frames)", interval, 1, 10, 2);
        });

        hack.setHandler([](bool state) {
            if (!state) hideSim();
        });
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        if (g_sim.creating) return;

        if (enabled()) updateTrajectory(this);
        else if (g_sim.node) hideSim();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (g_sim.creating || (player && (player == g_sim.fake1.data() || player == g_sim.fake2.data()))) {
            if (player) g_sim.deathRotation = player->getRotation();
            g_sim.cancelled = true;
            return;
        }

        PlayLayer::destroyPlayer(player, object);
    }

    void playEndAnimationToPos(cocos2d::CCPoint position) {
        if (g_sim.creating) return;
        PlayLayer::playEndAnimationToPos(position);
    }

    void resetLevel() {
        hideSim();
        PlayLayer::resetLevel();
    }

    void onQuit() {
        resetSim();
        PlayLayer::onQuit();
    }
};

class $modify(NXRTrajectoryBaseGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Player").findHackByName("Show Trajectory");

        NXR::tryAddHook(self, hack, "GJBaseGameLayer::collisionCheckObjects");
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::canBeActivatedByPlayer");
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::playerTouchedRing");
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::playerTouchedTrigger");
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::activateSFXTrigger");
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::activateSongEditTrigger");
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::gameEventTriggered");
    }

    void collisionCheckObjects(PlayerObject* player, gd::vector<GameObject*>* objects, int count, float dt) {
        if (!g_sim.creating || !objects) {
            GJBaseGameLayer::collisionCheckObjects(player, objects, count, dt);
            return;
        }

        std::vector<GameObject*> disabled;

        for (auto* obj : *objects) {
            if (!obj) continue;

            const bool allowed = kObjectTypes.contains(static_cast<int>(obj->m_objectType)) || kPortalIDs.contains(obj->m_objectID);
            if (allowed && !isCollectible(obj->m_objectID)) continue;
            if (obj->m_isDisabled || obj->m_isDisabled2) continue;

            disabled.push_back(obj);
            obj->m_isDisabled = true;
            obj->m_isDisabled2 = true;
        }

        GJBaseGameLayer::collisionCheckObjects(player, objects, count, dt);

        for (auto* obj : disabled) {
            if (!obj) continue;
            obj->m_isDisabled = false;
            obj->m_isDisabled2 = false;
        }
    }

    bool canBeActivatedByPlayer(PlayerObject* player, EffectGameObject* object) {
        if (g_sim.creating) {
            if (object) handlePortal(player, object->m_objectID);
            return false;
        }

        return GJBaseGameLayer::canBeActivatedByPlayer(player, object);
    }

    void playerTouchedRing(PlayerObject* player, RingObject* object) {
        if (g_sim.creating) return;
        GJBaseGameLayer::playerTouchedRing(player, object);
    }

    void playerTouchedTrigger(PlayerObject* player, EffectGameObject* object) {
        if (g_sim.creating) {
            if (object) handlePortal(player, object->m_objectID);
            return;
        }

        GJBaseGameLayer::playerTouchedTrigger(player, object);
    }

    void activateSFXTrigger(SFXTriggerGameObject* object) {
        if (g_sim.creating) return;
        GJBaseGameLayer::activateSFXTrigger(object);
    }

    void activateSongEditTrigger(SongTriggerGameObject* object) {
        if (g_sim.creating) return;
        GJBaseGameLayer::activateSongEditTrigger(object);
    }

    void gameEventTriggered(GJGameEvent event, int material, int playerID) {
        if (g_sim.creating) return;
        GJBaseGameLayer::gameEventTriggered(event, material, playerID);
    }
};
