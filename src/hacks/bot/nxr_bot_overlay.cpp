#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCParticleSystem.hpp>
#include <cmath>
#include <algorithm>
#include <string>
#include <memory>
#include "../../core/nxr_bot.hpp"
#include "../../core/nxr_config.hpp"

using namespace geode::prelude;

namespace {
    using namespace NXR::Bot;

    Ref<CCMenu> g_menu;
    CCMenuItemSpriteExtra* g_clock = nullptr;
    CCMenuItemSpriteExtra* g_pause = nullptr;
    CCMenuItemSpriteExtra* g_step = nullptr;

    constexpr float kButtonScale = 2.0f;

    constexpr float kSafeValues[] = {
        1.f / 240.f, 1.f / 120.f, 1.f / 80.f, 1.f / 60.f, 1.f / 48.f,
        1.f / 40.f, 1.f / 30.f, 1.f / 24.f, 1.f / 20.f, 1.f / 16.f,
        1.f / 15.f, 1.f / 12.f, 1.f / 10.f, 1.f / 8.f, 1.f / 6.f,
        1.f / 5.f, 1.f / 4.f, 1.f / 3.f, 1.f / 2.f
    };

    float snapSpeed(float speed) {
        const int whole = static_cast<int>(speed);
        const float decimals = speed - static_cast<float>(whole);
        if (decimals <= 0.f) return speed;

        float closest = kSafeValues[0];
        float minDiff = std::abs(decimals - closest);
        for (float value : kSafeValues) {
            const float diff = std::abs(decimals - value);
            if (diff < minDiff) {
                minDiff = diff;
                closest = value;
            }
        }
        return static_cast<float>(whole) + closest;
    }

    float desiredScale() {
        auto& st = State::get();
        if (!st.slowActive) return 1.f;

        float speed = std::clamp(NXRConfig::get().get<float>("nxr.bot.speed", 0.5f), 0.1f, 10000.f);
        if (st.mode != Mode::Off) speed = snapSpeed(speed);
        return std::clamp(speed, 0.1f, 10000.f);
    }


    int botType() {
        return std::clamp(NXRConfig::get().get<int>("nxr.bot.type", 1), 1, 3);
    }

    bool touchHeld(PlayLayer* pl) {
        auto* ui = pl->m_uiLayer;
        if (!ui) return false;
        return ui->m_p1Jumping || ui->m_p1TouchId != -1 || ui->m_p2Jumping || ui->m_p2TouchId != -1;
    }

    float playbackAllowance(PlayLayer* pl, float dt) {
        auto& st = State::get();
        st.waiting = false;

        const int type = botType();
        const bool touch = touchHeld(pl);
        const bool tapped = touch && !st.prevTouch;
        st.prevTouch = touch;

        if (st.mode != Mode::Playing || type == 1) {
            st.clickCredits = 0;
            return dt;
        }
        if (pl->m_isPaused || pl->m_levelEndAnimationStarted || (pl->m_player1 && pl->m_player1->m_isDead)) return dt;

        if (type == 2) {
            if (!touch) { st.waiting = true; return 0.f; }
            return dt;
        }

        auto& events = st.current.events;
        if (st.playIndex < st.lastPlayIndex) st.lastPlayIndex = st.playIndex;
        while (st.lastPlayIndex < st.playIndex && st.lastPlayIndex < events.size()) {
            if (events[st.lastPlayIndex].down() && st.clickCredits > 0) st.clickCredits--;
            st.lastPlayIndex++;
        }
        if (tapped) st.clickCredits = 1;

        size_t next = st.playIndex;
        while (next < events.size() && !events[next].down()) next++;
        if (next >= events.size()) return dt;
        if (st.clickCredits > 0) return dt;

        const int64_t ticksToPress = static_cast<int64_t>(events[next].frame()) - static_cast<int64_t>(st.frame) - 1;
        if (ticksToPress <= 0) { st.waiting = true; return 0.f; }

        const float tps = std::max(1.f, NXR::Bot::effectiveTps());
        const float limit = (static_cast<float>(ticksToPress) - 0.01f) / tps;
        return std::min(dt, limit);
    }

    float pitchFor(float scale) {
        return std::clamp(scale, 0.1f, 10.f);
    }

    FMOD::ChannelGroup* masterGroup() {
        auto* engine = FMODAudioEngine::sharedEngine();
        if (!engine || !engine->m_system) return nullptr;

        FMOD::ChannelGroup* group = nullptr;
        if (engine->m_system->getMasterChannelGroup(&group) != FMOD_OK) return nullptr;
        return group;
    }

    cocos2d::CCScheduler* scheduler() {
        auto* director = CCDirector::sharedDirector();
        return director ? director->getScheduler() : nullptr;
    }

    template <class E, class F>
    bool withMusic(E* engine, F&& fn) {
        if constexpr (requires { engine->m_backgroundMusicChannel; }) {
            auto* channel = engine->m_backgroundMusicChannel;
            if (channel) {
                fn(channel);
                return true;
            }
        }
        return false;
    }

    bool controlsInSync() {
        auto& st = State::get();
        const float scale = desiredScale();
        const float pitch = pitchFor(scale);

        auto* sch = scheduler();
        if (sch && sch->getTimeScale() != 1.f) return false;

        auto* engine = FMODAudioEngine::sharedEngine();
        if (!engine) return true;

        bool synced = true;
        const bool handled = withMusic(engine, [&](auto* channel) {
            float current = 1.f;
            bool paused = false;
            if (channel->getPitch(&current) == FMOD_OK && current != pitch) synced = false;
            if (channel->getPaused(&paused) == FMOD_OK && paused != (st.frozen || st.waiting)) synced = false;
        });

        if (!handled) {
            if (auto* group = masterGroup()) {
                float current = 1.f;
                bool paused = false;
                if (group->getPitch(&current) == FMOD_OK && current != pitch) synced = false;
                if (group->getPaused(&paused) == FMOD_OK && paused != (st.frozen || st.waiting)) synced = false;
            }
        }

        return synced;
    }

    void applyControls() {
        auto& st = State::get();
        const float pitch = pitchFor(desiredScale());

        if (auto* sch = scheduler()) sch->setTimeScale(1.f);

        auto* engine = FMODAudioEngine::sharedEngine();
        if (!engine) return;

        const bool handled = withMusic(engine, [&](auto* channel) {
            channel->setPitch(pitch);
            channel->setPaused(st.frozen || st.waiting);
        });

        if (!handled) {
            if (auto* group = masterGroup()) {
                group->setPitch(pitch);
                group->setPaused(st.frozen || st.waiting);
            }
        }
    }

    void setOpacity(CCMenuItemSpriteExtra* item, bool active) {
        if (!item) return;
        if (auto* sprite = typeinfo_cast<CCSprite*>(item->getNormalImage())) {
            sprite->setOpacity(active ? 255 : 150);
        }
    }

    void refreshButtons() {
        auto& st = State::get();
        setOpacity(g_clock, st.slowActive);

        if (g_pause) {
            const char* file = st.frozen ? "NXR_stepPlay.png"_spr : "NXR_stepPause.png"_spr;
            if (auto* sprite = CCSprite::create(file)) {
                sprite->setScale(kButtonScale);
                g_pause->setSprite(sprite);
            }
        }

        if (g_step) g_step->setVisible(st.frozen);
    }

    void clearOverlay() {
        if (g_menu && g_menu->getParent()) g_menu->removeFromParent();
        g_menu = nullptr;
        g_clock = nullptr;
        g_pause = nullptr;
        g_step = nullptr;
    }

    void buildOverlay(PlayLayer* pl) {
        if (!pl->m_uiLayer) return;

        auto win = CCDirector::sharedDirector()->getWinSize();
        auto* menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        menu->setZOrder(100);

        const float topY = win.height - 42.f;
        const float spacing = 78.f;

        auto make = [&](const char* file, float x, geode::Function<void()> callback) -> CCMenuItemSpriteExtra* {
            auto* sprite = CCSprite::create(file);
            sprite->setScale(kButtonScale);
            auto shared = std::make_shared<geode::Function<void()>>(std::move(callback));
            auto* item = CCMenuItemExt::createSpriteExtra(sprite, [shared](CCMenuItemSpriteExtra*) {
                (*shared)();
            });
            item->setPosition({x, topY});
            menu->addChild(item);
            return item;
        };

        g_clock = make("NXR_clockBtn.png"_spr, win.width - 90.f, [] {
            auto& st = State::get();
            st.slowActive = !st.slowActive;
            applyControls();
            refreshButtons();
        });

        g_pause = make("NXR_stepPause.png"_spr, 150.f, [] {
            auto& st = State::get();
            st.frozen = !st.frozen;
            st.stepRequests = 0;
            applyControls();
            refreshButtons();
        });

        g_step = make("NXR_stepNext.png"_spr, 150.f + spacing, [] {
            auto& st = State::get();
            if (st.frozen) st.stepRequests++;
        });

        pl->m_uiLayer->addChild(menu);
        g_menu = menu;
        refreshButtons();
    }

    void syncOverlay(PlayLayer* pl) {
        auto& st = State::get();
        bool wanted = st.mode == Mode::Recording;

        if (wanted && (!g_menu || !g_menu->getParent())) buildOverlay(pl);

        if (!wanted && g_menu) {
            st.frozen = false;
            st.stepRequests = 0;
            st.slowActive = false;
            clearOverlay();
        }

        if (!controlsInSync()) applyControls();
    }
}

void NXR::Bot::resetControls() {
    auto& st = State::get();
    st.waiting = false;
    st.clickCredits = 0;
    st.frozen = false;
    st.stepRequests = 0;
    st.slowActive = false;
    clearOverlay();
    applyControls();
}

void NXR::Bot::resetAttemptControls() {
    auto& st = State::get();
    st.waiting = false;
    st.clickCredits = 0;
    st.lastPlayIndex = 0;
    st.frozen = false;
    st.stepRequests = 0;
    applyControls();
    refreshButtons();
}

void NXR::Bot::syncControls() {
    if (!controlsInSync()) applyControls();
}

class $modify(NXRBotUpdateLayer, GJBaseGameLayer) {
    void update(float dt) {
        auto* pl = PlayLayer::get();
        if (!pl || static_cast<GJBaseGameLayer*>(pl) != this) {
            GJBaseGameLayer::update(dt);
            return;
        }

        syncOverlay(pl);

        auto& st = State::get();
        if (st.frozen) {
            if (st.stepParticles > 0) st.stepParticles--;

            if (st.stepRequests > 0) {
                st.stepRequests--;
                st.stepParticles = 4;
                const float stepSize = std::clamp(NXRConfig::get().get<float>("nxr.bot.frame_step", 1.f), 0.1f, 10.f);
                GJBaseGameLayer::update(stepSize / NXR::Bot::effectiveTps());
            }
            return;
        }

        const float scaled = dt * desiredScale();
        const float allowed = playbackAllowance(pl, scaled);
        if (allowed <= 0.f) {
            if (st.stepParticles > 0) st.stepParticles--;
            return;
        }
        GJBaseGameLayer::update(allowed);
    }
};

class $modify(NXRBotParticleSystem, CCParticleSystem) {
    virtual void update(float dt) {
        auto& st = State::get();
        if ((st.frozen || st.waiting) && st.stepParticles <= 0 && PlayLayer::get()) return;

        CCParticleSystem::update(dt);
    }
};
