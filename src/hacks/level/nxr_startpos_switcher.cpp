#include <Geode/Geode.hpp>
#include <algorithm>
#include <vector>
#include <Geode/modify/PlayLayer.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_keybinds.hpp"
#include "../../core/nxr_utils.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"

NXR_HACK_CREATE(
    "Level", "Startpos Switcher",
    "Switch between the start positions of a level with two on-screen arrows and a counter at the bottom. The arrows fade out after a moment and light up again when you use them. Q and E switch on PC",
    true
);

namespace {
    struct StartposState {
        PlayLayer* owner = nullptr;
        std::vector<geode::Ref<StartPosObject>> points;
        int index = -1;
        geode::Ref<cocos2d::CCLabelBMFont> label;
        geode::Ref<cocos2d::CCMenu> menu;
        std::vector<geode::Ref<cocos2d::CCSprite>> arrows;

        void clearUi() {
            if (label && label->getParent()) label->removeFromParent();
            if (menu && menu->getParent()) menu->removeFromParent();
            label = nullptr;
            menu = nullptr;
            arrows.clear();
        }

        void clear() {
            clearUi();
            owner = nullptr;
            points.clear();
            index = -1;
        }
    };

    StartposState g_state;

    PlayLayer* g_collectOwner = nullptr;
    std::vector<geode::Ref<StartPosObject>> g_collected;

    void scan(PlayLayer* layer) {
        g_state.points.clear();
        g_state.index = -1;
        if (!layer) return;

        if (g_collectOwner == layer && !g_collected.empty()) {
            g_state.points = g_collected;
        } else if (layer->m_objects) {
            for (auto* obj : geode::cocos::CCArrayExt<GameObject*>(layer->m_objects)) {
                if (auto* sp = typeinfo_cast<StartPosObject*>(obj)) {
                    g_state.points.emplace_back(sp);
                }
            }
        }

        std::stable_sort(g_state.points.begin(), g_state.points.end(),
            [](const geode::Ref<StartPosObject>& a, const geode::Ref<StartPosObject>& b) {
                return a->getPositionX() < b->getPositionX();
            });

        if (layer->m_startPosObject) {
            for (int i = 0; i < static_cast<int>(g_state.points.size()); i++) {
                if (g_state.points[i].data() == layer->m_startPosObject) {
                    g_state.index = i;
                    break;
                }
            }
        }
    }

    void refreshLabel() {
        if (!g_state.label) return;
        const int total = static_cast<int>(g_state.points.size());
        const int shown = g_state.index < 0 ? 0 : g_state.index + 1;
        g_state.label->setString(fmt::format("{} / {}", shown, total).c_str());
    }

    void applyIndex(int next) {
        auto* layer = PlayLayer::get();
        if (!layer || g_state.owner != layer) return;
        if (g_state.points.empty() || layer->m_levelEndAnimationStarted) return;

        g_state.index = next;

        StartPosObject* target = next >= 0 ? g_state.points[next].data() : nullptr;

        refreshLabel();

        const bool practice = layer->m_isPracticeMode;
        if (practice) layer->togglePracticeMode(false);

        layer->m_isPaused = false;
        layer->m_isTestMode = target != nullptr;
        layer->m_currentCheckpoint = nullptr;
        layer->setStartPosObject(target);
        layer->resetLevel();

        if (NXRConfig::get().get<bool>("nxr.level.startpos_switcher::reset_camera", false)) layer->resetCamera();

        layer->startMusic();

        if (practice) layer->togglePracticeMode(true);
    }

    void switchBy(int delta) {
        if (g_state.points.empty()) return;

        const int total = static_cast<int>(g_state.points.size());

        int next = g_state.index + delta;
        if (next < -1) next = total - 1;
        if (next >= total) next = -1;

        applyIndex(next);
    }

    void restartFade() {
        auto& config = NXRConfig::get();
        const int maxOpacity = std::clamp(config.get<int>("nxr.level.startpos_switcher::max_opacity", 200), 0, 255);
        const int minOpacity = std::clamp(config.get<int>("nxr.level.startpos_switcher::min_opacity", 100), 0, 255);

        auto fade = [&](auto* node) {
            if (!node) return;
            node->stopAllActions();
            node->setOpacity(static_cast<GLubyte>(maxOpacity));
            node->runAction(cocos2d::CCSequence::create(
                cocos2d::CCDelayTime::create(2.f),
                cocos2d::CCFadeTo::create(0.3f, static_cast<GLubyte>(minOpacity)),
                nullptr
            ));
        };

        fade(g_state.label.data());
        for (auto& arrow : g_state.arrows) fade(arrow.data());
    }

    enum Action : int { ActionPrev = 0, ActionNext = 1 };

    void runAction(int action) {
        switch (action) {
            case ActionPrev: switchBy(-1); break;
            case ActionNext: switchBy(1); break;
            default: break;
        }
        restartFade();
    }

    void buildUi(PlayLayer* layer) {
        g_state.clearUi();
        if (!layer || !layer->m_uiLayer) return;

        const cocos2d::ccColor3B color = {255, 255, 255};
        constexpr float scale = 1.f;

        auto* director = cocos2d::CCDirector::sharedDirector();
        const auto win = director->getWinSize();
        const float bottom = director->getScreenBottom();

        const float arrow = 32.f * scale;
        const float sideOffset = 50.f;
        const float cx = win.width / 2.f;
        const float cy = bottom + arrow * 0.5f + 6.f;

        g_state.label = cocos2d::CCLabelBMFont::create("0 / 0", "bigFont.fnt");
        g_state.label->setScale(0.45f);
        g_state.label->setColor(color);
        g_state.label->setPosition({cx, cy});
        g_state.label->setZOrder(100);
        layer->m_uiLayer->addChild(g_state.label);

        g_state.menu = cocos2d::CCMenu::create();
        g_state.menu->setPosition({0.f, 0.f});
        g_state.menu->setZOrder(100);

        auto makeArrow = [&](const char* file, float x, float y, int action) {
            auto* sprite = cocos2d::CCSprite::create(file);
            if (!sprite) return;
            sprite->setScale(scale);
            sprite->setColor(color);
            auto* item = CCMenuItemExt::createSpriteExtra(sprite, [action](CCMenuItemSpriteExtra*) {
                runAction(action);
            });
            item->setPosition({x, y});
            g_state.menu->addChild(item);
            g_state.arrows.emplace_back(sprite);
        };

        makeArrow("NXR_arrowLeft.png"_spr, cx - sideOffset, cy, ActionPrev);
        makeArrow("NXR_arrowRight.png"_spr, cx + sideOffset, cy, ActionNext);

        layer->m_uiLayer->addChild(g_state.menu);

        refreshLabel();
        restartFade();
    }

    void bindKeys();

    void install(PlayLayer* layer) {
        if (!layer) return;
        g_state.clear();
        g_state.owner = layer;
        scan(layer);
        bindKeys();
        buildUi(layer);
    }

    void ensureUi(PlayLayer* layer) {
        if (!layer || g_state.owner != layer) return;
        if (g_state.menu && g_state.menu->getParent() && g_state.label && g_state.label->getParent()) return;
        buildUi(layer);
    }

    void bindKeys() {
        auto& kb = NXR::Keybinds::get();
        kb.registerAction("nxr.startpos::prev", "Startpos: Previous", geode::Keybind(cocos2d::KEY_Q, geode::KeyboardModifier::None), [](bool repeat) {
            if (!repeat) switchBy(-1);
        });
        kb.registerAction("nxr.startpos::next", "Startpos: Next", geode::Keybind(cocos2d::KEY_E, geode::KeyboardModifier::None), [](bool repeat) {
            if (!repeat) switchBy(1);
        });
    }

    void unbindKeys() {
        auto& kb = NXR::Keybinds::get();
        kb.clearCallback("nxr.startpos::prev");
        kb.clearCallback("nxr.startpos::next");
    }
}

class $modify(NXRStartposSwitcherPlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Level").findHackByName("Startpos Switcher");

        NXR::tryAddHook(self, hack, "PlayLayer::setupHasCompleted");
        NXR::tryAddHook(self, hack, "PlayLayer::addObject");
        NXR::tryAddHook(self, hack, "PlayLayer::resetLevel");

        hack.setHandler([](bool state) {
            if (!state) {
                unbindKeys();
                g_state.clear();
                return;
            }

            if (auto* layer = PlayLayer::get()) install(layer);
        });

        hack.setCustomWindowCocos([
            minOpacityKey = hack.formatAdditionalSetting("min_opacity"),
            maxOpacityKey = hack.formatAdditionalSetting("max_opacity"),
            cameraKey = hack.formatAdditionalSetting("reset_camera")
        ](cocos2d::CCNode* popupNode) {
            auto* popup = static_cast<NXRHackSettingsPopup*>(popupNode);
            popup->addConfigToggle("Reset Camera", cameraKey, false);
            popup->addConfigIntInput("Min Opacity (0 - 255)", minOpacityKey, 0, 255, 100);
            popup->addConfigIntInput("Max Opacity (0 - 255)", maxOpacityKey, 0, 255, 200);
        });
    }

    void addObject(GameObject* obj) {
        PlayLayer::addObject(obj);

        if (g_collectOwner != this) {
            g_collectOwner = this;
            g_collected.clear();
        }

        if (obj && obj->m_objectID == 31) {
            if (auto* sp = typeinfo_cast<StartPosObject*>(obj)) g_collected.emplace_back(sp);
        }
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        install(this);
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        ensureUi(this);
    }

    void onQuit() {
        unbindKeys();
        g_state.clear();
        g_collected.clear();
        g_collectOwner = nullptr;
        PlayLayer::onQuit();
    }
};
