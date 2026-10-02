#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <cmath>
#include <vector>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_utils.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"

using namespace geode::prelude;

NXR_HACK_CREATE(
    "Player", "Show Hitbox",
    "Draws the hitboxes of the player and of every object in the level. Normal mode draws outlines only, Filled mode also fills each box with a translucent color. Solid blocks, spikes, triggers and the player each have their own color",
    false
);

namespace {
    constexpr const char* kEnabledKey = "nxr.player.show_hitbox";
    constexpr const char* kModeKey = "nxr.player.show_hitbox::mode";
    constexpr const char* kOpacityKey = "nxr.player.show_hitbox::opacity";
    constexpr const char* kSizeKey = "nxr.player.show_hitbox::size";
    constexpr const char* kPlayerKey = "nxr.player.show_hitbox::show_player";
    constexpr const char* kObjectsKey = "nxr.player.show_hitbox::show_objects";
    constexpr const char* kInnerKey = "nxr.player.show_hitbox::show_inner";
    constexpr const char* kSolidKey = "nxr.player.show_hitbox::col_solid";
    constexpr const char* kPassableKey = "nxr.player.show_hitbox::col_passable";
    constexpr const char* kDangerKey = "nxr.player.show_hitbox::col_danger";
    constexpr const char* kOtherKey = "nxr.player.show_hitbox::col_other";
    constexpr const char* kTriggerKey = "nxr.player.show_hitbox::col_trigger";
    constexpr const char* kPlayerColorKey = "nxr.player.show_hitbox::col_player";
    constexpr const char* kInnerColorKey = "nxr.player.show_hitbox::col_inner";
    constexpr const char* kRotatedKey = "nxr.player.show_hitbox::col_rotated";

    enum Mode : int { ModeOutline = 1, ModeFilled = 2 };

    struct Palette {
        ccColor4F solid;
        ccColor4F passable;
        ccColor4F danger;
        ccColor4F other;
        ccColor4F trigger;
        ccColor4F player;
        ccColor4F inner;
        ccColor4F rotated;
    };

    struct Style {
        Palette colors;
        float border = 0.5f;
        float fillAlpha = 0.f;
    };

    Ref<CCDrawNode> g_node;
    std::vector<GameObject*> g_cache;
    PlayLayer* g_cacheOwner = nullptr;

    ccColor4F readColor(const char* key, const char* fallback) {
        return NXR::Utils::hexToColor4F(NXRConfig::get().get<std::string>(key, fallback));
    }

    ccColor4F withAlpha(const ccColor4F& c, float alpha) {
        return {c.r, c.g, c.b, alpha};
    }

    Style readStyle() {
        auto& config = NXRConfig::get();
        Style style;
        style.colors.solid = readColor(kSolidKey, "#003FFFFF");
        style.colors.passable = readColor(kPassableKey, "#00FFFFFF");
        style.colors.danger = readColor(kDangerKey, "#FF0000FF");
        style.colors.other = readColor(kOtherKey, "#00FF00FF");
        style.colors.trigger = readColor(kTriggerKey, "#FF00E6FF");
        style.colors.player = readColor(kPlayerColorKey, "#FF0000FF");
        style.colors.inner = readColor(kInnerColorKey, "#003FFFFF");
        style.colors.rotated = readColor(kRotatedKey, "#800000FF");
        style.border = std::clamp(config.get<float>(kSizeKey, 0.5f), 0.1f, 10.f);

        const int mode = std::clamp(config.get<int>(kModeKey, ModeOutline), 1, 2);
        style.fillAlpha = mode == ModeFilled
            ? static_cast<float>(std::clamp(config.get<int>(kOpacityKey, 60), 0, 255)) / 255.f
            : 0.f;
        return style;
    }

    template <class T>
    bool objectActive(T* obj) {
        if constexpr (requires { obj->m_isActivated; obj->m_isGroupDisabled; }) {
            return static_cast<bool>(obj->m_isActivated) && !static_cast<bool>(obj->m_isGroupDisabled);
        } else {
            return true;
        }
    }

    template <class T>
    bool objectPassable(T* obj) {
        if constexpr (requires { obj->m_isPassable; }) return static_cast<bool>(obj->m_isPassable);
        else return false;
    }

    template <class T>
    float objectRadius(T* obj) {
        if constexpr (requires { obj->m_objectRadius; }) return std::max(obj->m_scaleX, obj->m_scaleY) * static_cast<float>(obj->m_objectRadius);
        else return 0.f;
    }

    template <class T>
    int slopeDirection(T* obj) {
        if constexpr (requires { obj->m_slopeDirection; }) return static_cast<int>(obj->m_slopeDirection);
        else return 0;
    }

    template <class T>
    bool touchTriggered(T* obj) {
        if constexpr (requires { static_cast<EffectGameObject*>(obj)->m_isTouchTriggered; }) return static_cast<bool>(static_cast<EffectGameObject*>(obj)->m_isTouchTriggered);
        else return false;
    }

    void drawRect(CCDrawNode* node, const CCRect& rect, const ccColor4F& fill, float border, const ccColor4F& edge) {
        CCPoint vertices[4] = {
            {rect.getMinX(), rect.getMinY()},
            {rect.getMinX(), rect.getMaxY()},
            {rect.getMaxX(), rect.getMaxY()},
            {rect.getMaxX(), rect.getMinY()}
        };
        node->drawPolygon(vertices, 4, fill, border, edge);
    }

    void drawCircle(CCDrawNode* node, const CCPoint& center, float radius, const ccColor4F& fill, float border, const ccColor4F& edge) {
        constexpr int segments = 20;
        CCPoint vertices[segments];
        for (int i = 0; i < segments; i++) {
            const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(segments);
            vertices[i] = CCPoint(center.x + radius * std::cos(a), center.y + radius * std::sin(a));
        }
        node->drawPolygon(vertices, segments, fill, border, edge);
    }

    template <class T>
    bool drawOriented(CCDrawNode* node, T* obj, const ccColor4F& fill, float border, const ccColor4F& edge) {
        if constexpr (requires {
            obj->calculateOrientedBox();
            obj->getOrientedBox()->m_corners.data();
            obj->m_shouldUseOuterOb;
            obj->m_isObjectRectDirty;
            obj->m_boxOffsetCalculated;
        }) {
            const int rotation = static_cast<int>(obj->getRotation());
            if ((rotation % 90) == 0 || !obj->m_shouldUseOuterOb) return false;

            const auto dirty = obj->m_isObjectRectDirty;
            const auto offset = obj->m_boxOffsetCalculated;

            obj->calculateOrientedBox();
            bool drawn = false;
            if (auto* box = obj->getOrientedBox()) {
                node->drawPolygon(box->m_corners.data(), 4, fill, border, edge);
                drawn = true;
            }

            obj->m_isObjectRectDirty = dirty;
            obj->m_boxOffsetCalculated = offset;
            return drawn;
        } else {
            return false;
        }
    }

    void drawOrientedOrRect(CCDrawNode* node, GameObject* obj, const ccColor4F& fill, float border, const ccColor4F& edge) {
        if (!drawOriented(node, obj, fill, border, edge)) drawRect(node, obj->getObjectRect(), fill, border, edge);
    }

    void drawObject(PlayLayer* pl, CCDrawNode* node, GameObject* obj, const Style& style) {
        if (!obj) return;
        if (obj == pl->m_player1 || obj == pl->m_player2) return;
        if (obj->m_objectType == GameObjectType::Decoration || !objectActive(obj)) return;

        const auto& c = style.colors;
        const float b = style.border;
        const float fa = style.fillAlpha;

        switch (obj->m_objectType) {
            case GameObjectType::Solid: {
                const auto& color = objectPassable(obj) ? c.passable : c.solid;
                drawRect(node, obj->getObjectRect(), withAlpha(color, fa), b, color);
                break;
            }
            case GameObjectType::Slope: {
                const CCRect rect = obj->getObjectRect();
                CCPoint verts[3] = {
                    {rect.getMinX(), rect.getMinY()},
                    {rect.getMinX(), rect.getMaxY()},
                    {rect.getMaxX(), rect.getMinY()}
                };
                const CCPoint top{rect.getMaxX(), rect.getMaxY()};
                switch (slopeDirection(obj)) {
                    case 0: case 7: verts[1] = top; break;
                    case 1: case 5: verts[0] = top; break;
                    case 3: case 6: verts[2] = top; break;
                    default: break;
                }
                const auto& color = objectPassable(obj) ? c.passable : c.solid;
                node->drawPolygon(verts, 3, withAlpha(color, fa), b, color);
                break;
            }
            case GameObjectType::AnimatedHazard:
            case GameObjectType::Hazard: {
                const float radius = objectRadius(obj);
                if (radius > 0.f) drawCircle(node, obj->getPosition(), radius, withAlpha(c.danger, fa), b, c.danger);
                else drawOrientedOrRect(node, obj, withAlpha(c.danger, fa), b, c.danger);
                break;
            }
            case GameObjectType::CollisionObject:
                break;
            default: {
                const int id = obj->m_objectID;
                const bool speedPortal = id == 200 || id == 201 || id == 202 || id == 203 || id == 1334;
                if (obj->m_objectType == GameObjectType::Modifier && !speedPortal) {
                    if (touchTriggered(obj)) drawOrientedOrRect(node, obj, withAlpha(c.trigger, fa), b, c.trigger);
                    break;
                }
                drawOrientedOrRect(node, obj, withAlpha(c.other, fa), b, c.other);
                break;
            }
        }
    }

    template <class L, class F>
    void forEachVisible(L* layer, F&& fn) {
        if constexpr (requires {
            layer->m_sections;
            layer->m_sectionSizes;
            layer->m_leftSectionIndex;
            layer->m_rightSectionIndex;
            layer->m_topSectionIndex;
            layer->m_bottomSectionIndex;
            layer->m_sections[0]->size();
            layer->m_sections[0]->at(0)->data();
            layer->m_sectionSizes[0]->at(0);
        }) {
            if (layer->m_sections.empty()) return;

            const int rightBound = std::min(static_cast<int>(layer->m_rightSectionIndex), static_cast<int>(layer->m_sections.size()) - 1);
            for (int i = std::max(0, static_cast<int>(layer->m_leftSectionIndex)); i <= rightBound; ++i) {
                auto* column = layer->m_sections[i];
                if (!column) continue;

                auto* sizes = layer->m_sectionSizes[i];
                const int topBound = std::min(static_cast<int>(layer->m_topSectionIndex), static_cast<int>(column->size()) - 1);
                for (int j = std::max(0, static_cast<int>(layer->m_bottomSectionIndex)); j <= topBound; ++j) {
                    auto* section = column->at(j);
                    if (!section || !sizes) continue;

                    const int count = sizes->at(j);
                    auto* data = section->data();
                    for (int k = 0; k < count; ++k) fn(data[k]);
                }
            }
        } else {
            auto* player = layer->m_player1;
            if (!player || !layer->m_objects) return;

            if (g_cacheOwner != layer || g_cache.empty()) {
                g_cache.clear();
                g_cacheOwner = layer;
                for (auto* obj : CCArrayExt<GameObject*>(layer->m_objects)) {
                    if (obj) g_cache.push_back(obj);
                }
                std::stable_sort(g_cache.begin(), g_cache.end(), [](GameObject* a, GameObject* b) {
                    return a->getPositionX() < b->getPositionX();
                });
            }

            const float width = CCDirector::sharedDirector()->getWinSize().width;
            const float x = player->getPositionX();
            const float lo = x - width * 0.9f;
            const float hi = x + width * 1.6f;

            auto first = std::lower_bound(g_cache.begin(), g_cache.end(), lo, [](GameObject* obj, float value) {
                return obj->getPositionX() < value;
            });
            for (auto it = first; it != g_cache.end() && (*it)->getPositionX() <= hi; ++it) fn(*it);
        }
    }

    template <class P>
    void drawPlayerT(CCDrawNode* node, P* player, const Style& style, bool showInner) {
        if (!player || player->m_isDead) return;

        const auto& c = style.colors;
        const float b = style.border;
        const float fa = style.fillAlpha;

        if constexpr (requires { player->updateOrientedBox(); player->getOrientedBox()->m_corners.data(); }) {
            if (!player->m_isBall) {
                player->updateOrientedBox();
                if (auto* box = player->getOrientedBox()) {
                    node->drawPolygon(box->m_corners.data(), 4, withAlpha(c.rotated, fa), b, c.rotated);
                }
            }
        }

        drawRect(node, player->getObjectRect(), withAlpha(c.player, fa), b, c.player);
        if (showInner) drawRect(node, player->getObjectRect(0.3f, 0.3f), withAlpha(c.inner, fa), b, c.inner);
    }

    void drawPlayer(CCDrawNode* node, PlayerObject* player, const Style& style, bool showInner) {
        drawPlayerT(node, player, style, showInner);
    }

    template <class N>
    void disableArea(N* node) {
        if constexpr (requires { node->m_bUseArea; }) node->m_bUseArea = false;
    }

    void ensureNode(PlayLayer* pl) {
        CCNode* parent = pl->m_debugDrawNode ? pl->m_debugDrawNode->getParent() : nullptr;
        if (!parent) parent = pl->m_objectLayer;
        if (!parent) return;

        if (g_node && g_node->getParent() == parent) return;
        if (g_node && g_node->getParent()) g_node->removeFromParent();

        g_node = CCDrawNode::create();
        g_node->setBlendFunc({GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});
        g_node->setZOrder(9999);
        disableArea(g_node.data());
        parent->addChild(g_node);
    }

    void clearNode() {
        if (g_node && g_node->getParent()) g_node->removeFromParent();
        g_node = nullptr;
    }

    void refresh(PlayLayer* pl) {
        ensureNode(pl);
        if (!g_node) return;
        g_node->clear();

        auto& config = NXRConfig::get();
        const Style style = readStyle();

        if (config.get<bool>(kObjectsKey, true)) {
            forEachVisible(static_cast<GJBaseGameLayer*>(pl), [&](GameObject* obj) {
                drawObject(pl, g_node, obj, style);
            });
        }

        if (config.get<bool>(kPlayerKey, true)) {
            const bool inner = config.get<bool>(kInnerKey, true);
            drawPlayer(g_node, pl->m_player1, style, inner);
            if (pl->m_gameState.m_isDualMode) drawPlayer(g_node, pl->m_player2, style, inner);
        }
    }
}

class $modify(NXRShowHitboxPlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Player").findHackByName("Show Hitbox");

        hack.setCustomWindowCocos([](cocos2d::CCNode* node) {
            auto* popup = static_cast<NXRHackSettingsPopup*>(node);
            popup->addConfigModeToggle(kModeKey, "Normal", "Filled");
            popup->addConfigIntInput("Fill Opacity", kOpacityKey, 0, 255, 60);
            popup->addConfigFloatInput("Line Size", kSizeKey, 0.1f, 10.f, 0.5f);
            popup->addSeparator();
            popup->addConfigToggle("Show Player", kPlayerKey, true);
            popup->addConfigToggle("Show Inner Box", kInnerKey, true);
            popup->addConfigToggle("Show Objects", kObjectsKey, true);
            popup->addSeparator();
            popup->addConfigColor4Hex("Solid Blocks", kSolidKey, "#003FFFFF");
            popup->addConfigColor4Hex("Passable Blocks", kPassableKey, "#00FFFFFF");
            popup->addConfigColor4Hex("Spikes And Saws", kDangerKey, "#FF0000FF");
            popup->addConfigColor4Hex("Triggers", kTriggerKey, "#FF00E6FF");
            popup->addConfigColor4Hex("Other Objects", kOtherKey, "#00FF00FF");
            popup->addSeparator();
            popup->addConfigColor4Hex("Player Box", kPlayerColorKey, "#FF0000FF");
            popup->addConfigColor4Hex("Player Inner Box", kInnerColorKey, "#003FFFFF");
            popup->addConfigColor4Hex("Player Rotated Box", kRotatedKey, "#800000FF");
        });

        hack.setHandler([](bool enabled) {
            if (!enabled) clearNode();
        });
    }

    void updateVisibility(float dt) {
        PlayLayer::updateVisibility(dt);

        if (NXRConfig::get().get<bool>(kEnabledKey, false)) refresh(this);
        else if (g_node) clearNode();
    }

    void resetLevel() {
        clearNode();
        g_cache.clear();
        g_cacheOwner = nullptr;
        PlayLayer::resetLevel();
    }

    void onQuit() {
        clearNode();
        g_cache.clear();
        g_cacheOwner = nullptr;
        PlayLayer::onQuit();
    }
};
