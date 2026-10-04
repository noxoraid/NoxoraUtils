#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <unordered_map>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_utils.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"

using namespace geode::prelude;

NXR_HACK_CREATE(
    "Bot", "Frame Window Counter",
    "Shows the frame window of every orb you click: how many physics frames the orb could be clicked in, "
    "drawn as a number on the orb. The counter list on the left counts your clicks by how many frames of margin "
    "they had to the edge of the window (0 = frame perfect). A sound plays at every orb click. "
    "Works while playing, recording and in playback. Counter and orb numbers can be hidden separately",
    false
);

namespace {
    constexpr const char* kEnabledKey = "nxr.bot.frame_window_counter";
    constexpr const char* kCounterKey = "nxr.bot.frame_window_counter::counter";
    constexpr const char* kLabelsKey = "nxr.bot.frame_window_counter::labels";
    constexpr const char* kSoundKey = "nxr.bot.frame_window_counter::sound";
    constexpr const char* kVolumeKey = "nxr.bot.frame_window_counter::sound_volume";
    constexpr const char* kResetKey = "nxr.bot.frame_window_counter::reset_attempt";
    constexpr const char* kScaleKey = "nxr.bot.frame_window_counter::scale";
    constexpr const char* kPosYKey = "nxr.bot.frame_window_counter::pos_y";

    constexpr int kBuckets = 8;

    struct Track {
        int overlap = 0;
        int clickAt = 0;
        bool seen = false;
        CCPoint pos;
    };

    struct Pair {
        const void* player = nullptr;
        const void* object = nullptr;
        bool operator==(const Pair& o) const { return player == o.player && object == o.object; }
    };

    struct PairHash {
        size_t operator()(const Pair& p) const {
            return std::hash<const void*>()(p.player) * 31u + std::hash<const void*>()(p.object);
        }
    };

    std::unordered_map<Pair, Track, PairHash> g_tracks;
    std::array<int, kBuckets> g_counts{};
    const void* g_layer = nullptr;
    Ref<CCNode> g_hud;
    std::array<CCLabelBMFont*, kBuckets> g_rows{};
    bool g_hudDirty = true;

    bool enabled() { return NXRConfig::get().get<bool>(kEnabledKey, false); }
    bool showCounter() { return NXRConfig::get().get<bool>(kCounterKey, true); }
    bool showLabels() { return NXRConfig::get().get<bool>(kLabelsKey, true); }
    bool soundOn() { return NXRConfig::get().get<bool>(kSoundKey, true); }

    float soundVolume() {
        return static_cast<float>(std::clamp(NXRConfig::get().get<int>(kVolumeKey, 80), 0, 100)) / 100.f;
    }

    ccColor3B bucketColor(int bucket) {
        static const ccColor3B colors[kBuckets] = {
            ccc3(255, 70, 70), ccc3(255, 150, 50), ccc3(255, 225, 60), ccc3(255, 255, 255),
            ccc3(90, 150, 255), ccc3(60, 225, 255), ccc3(70, 255, 130), ccc3(150, 255, 190),
        };
        return colors[std::clamp(bucket, 0, kBuckets - 1)];
    }

    std::string clickSoundPath() {
        static std::string path;
        if (!path.empty()) return path;
        std::error_code ec;
        const auto absolute = geode::Mod::get()->getResourcesDir() / "click_indicator.mp3";
        if (std::filesystem::exists(absolute, ec)) path = geode::utils::string::pathToString(absolute);
        else path = std::string("click_indicator.mp3"_spr);
        return path;
    }

    void playSound() {
        if (!soundOn()) return;
        auto* engine = FMODAudioEngine::get();
        if (!engine) return;
        engine->playEffect(clickSoundPath(), 1.f, 0.f, soundVolume());
    }

    void dropHud() {
        if (g_hud) g_hud->removeFromParent();
        g_hud = nullptr;
        g_rows.fill(nullptr);
        g_hudDirty = true;
    }

    void refreshHud() {
        for (int i = 0; i < kBuckets; i++) {
            if (!g_rows[i]) continue;
            const std::string text = i == kBuckets - 1
                ? fmt::format("{}+ frames: {}", i, g_counts[i])
                : fmt::format("{} frames: {}", i, g_counts[i]);
            g_rows[i]->setString(text.c_str());
        }
    }

    void ensureHud(PlayLayer* pl) {
        if (!pl || !pl->m_uiLayer) return;
        if (g_hud && g_hud->getParent() != pl->m_uiLayer) dropHud();
        if (g_hud) return;

        const CCSize win = CCDirector::sharedDirector()->getWinSize();
        const float scale = static_cast<float>(std::clamp(NXRConfig::get().get<int>(kScaleKey, 100), 40, 250)) / 100.f;
        const float posY = static_cast<float>(std::clamp(NXRConfig::get().get<int>(kPosYKey, 62), 10, 95)) / 100.f;
        const float step = 14.f * scale;

        auto* root = CCNode::create();
        root->setPosition(CCPoint(6.f, win.height * posY));
        root->setZOrder(900);
        for (int i = 0; i < kBuckets; i++) {
            auto* label = CCLabelBMFont::create("", "bigFont.fnt");
            label->setAnchorPoint(CCPoint(0.f, 0.5f));
            label->setScale(0.3f * scale);
            label->setColor(bucketColor(i));
            label->setPosition(CCPoint(0.f, -step * static_cast<float>(i)));
            label->setOpacity(215);
            root->addChild(label);
            g_rows[i] = label;
        }
        pl->m_uiLayer->addChild(root);
        g_hud = root;
        refreshHud();
    }

    void clearAll() {
        g_tracks.clear();
        g_counts.fill(0);
        g_hudDirty = true;
        refreshHud();
    }

    void addOrbLabel(PlayLayer* pl, const CCPoint& pos, int window, int bucket) {
        if (!pl || !pl->m_objectLayer || !showLabels()) return;
        auto* label = CCLabelBMFont::create(std::to_string(window).c_str(), "bigFont.fnt");
        label->setScale(0.42f);
        label->setColor(bucketColor(bucket));
        label->setPosition(pos + CCPoint(0.f, 26.f));
        label->setZOrder(2000);
        label->setUserObject("nxr-frame-window"_spr, CCString::create("1"));
        label->runAction(CCSequence::create(
            CCDelayTime::create(1.4f),
            CCFadeOut::create(0.4f),
            CCRemoveSelf::create(),
            nullptr
        ));
        pl->m_objectLayer->addChild(label);
    }

    void finish(PlayLayer* pl, const Track& track) {
        if (track.clickAt <= 0 || track.overlap <= 0) return;
        const int window = track.overlap;
        const int margin = std::max(0, std::min(track.clickAt - 1, window - track.clickAt));
        const int bucket = std::min(margin, kBuckets - 1);
        g_counts[bucket]++;
        g_hudDirty = true;
        addOrbLabel(pl, track.pos, window, bucket);
    }

    void tickPlayer(PlayLayer* pl, PlayerObject* player, bool second) {
        if (!player) return;
        auto* rings = player->m_touchingRings;
        if (!rings || rings->count() == 0 || player->m_isDead) return;

        for (auto* obj : CCArrayExt<GameObject*>(rings)) {
            if (!obj) continue;
            auto& track = g_tracks[Pair{player, obj}];
            track.overlap++;
            track.seen = true;
            track.pos = obj->getPosition();
            const bool activated = second ? obj->m_hasBeenActivatedP2 : obj->m_hasBeenActivated;
            if (activated && track.clickAt == 0) {
                track.clickAt = track.overlap;
                playSound();
            }
        }
    }

    void tickAll(PlayLayer* pl, GJBaseGameLayer* layer) {
        for (auto& [key, track] : g_tracks) track.seen = false;

        tickPlayer(pl, layer->m_player1, false);
        if (layer->m_player2 && layer->m_player2 != layer->m_player1) tickPlayer(pl, layer->m_player2, true);

        for (auto it = g_tracks.begin(); it != g_tracks.end();) {
            if (it->second.seen) {
                ++it;
                continue;
            }
            finish(pl, it->second);
            it = g_tracks.erase(it);
        }
    }
}

class $modify(NXRFrameWindowGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Bot").findHackByName("Frame Window Counter");
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::processCommands");

        hack.setForm([](NXR::Form& form) {
            form.addConfigToggle("Show Counter", kCounterKey, true);
            form.addConfigToggle("Show Orb Numbers", kLabelsKey, true);
            form.addSeparator();
            form.addConfigToggle("Click Sound", kSoundKey, true);
            form.addConfigSlider("Sound Volume", kVolumeKey, 0.f, 100.f, 80.f, 1.f, NXR::SliderScale::Linear, {{"25%", 25.f}, {"50%", 50.f}, {"80%", 80.f}, {"100%", 100.f}}, nullptr, true, "%");
            form.addSeparator();
            form.addConfigToggle("Reset On New Attempt", kResetKey, false);
            form.addConfigSlider("Counter Size", kScaleKey, 40.f, 250.f, 100.f, 5.f, NXR::SliderScale::Linear, {{"70%", 70.f}, {"100%", 100.f}, {"150%", 150.f}}, [](float) { dropHud(); }, true, "%");
            form.addConfigSlider("Counter Height", kPosYKey, 10.f, 95.f, 62.f, 1.f, NXR::SliderScale::Linear, {}, [](float) { dropHud(); }, true, "%");
        });
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
        auto* pl = PlayLayer::get();
        if (!isHalfTick && pl && static_cast<GJBaseGameLayer*>(pl) == this && !pl->m_isPaused && enabled()) {
            tickAll(pl, this);
        }
    }
};

class $modify(NXRFrameWindowPlayLayer, PlayLayer) {
    void resetLevel() {
        PlayLayer::resetLevel();
        g_tracks.clear();
        if (NXRConfig::get().get<bool>(kResetKey, false)) clearAll();
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (g_layer != this) {
            g_layer = this;
            dropHud();
            clearAll();
        }
        const bool on = enabled() && showCounter();
        if (!on) {
            if (g_hud) g_hud->setVisible(false);
            return;
        }
        ensureHud(this);
        if (g_hud) {
            g_hud->setVisible(true);
            if (g_hudDirty) {
                refreshHud();
                g_hudDirty = false;
            }
        }
    }

    void onQuit() {
        g_layer = nullptr;
        dropHud();
        g_tracks.clear();
        PlayLayer::onQuit();
    }
};
