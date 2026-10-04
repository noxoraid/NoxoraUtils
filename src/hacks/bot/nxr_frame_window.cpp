#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_bot.hpp"
#include "../../core/nxr_safe_hook.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_practice_fix.hpp"
#include "../../interface/cocos/nxr_hack_settings_popup.hpp"
#include <filesystem>

using namespace geode::prelude;

NXR_HACK_CREATE(
    "Bot", "Frame Window Counter",
    "Frame perfect counter. For every click it checks how many physics frames earlier or later the same click would still survive "
    "(for example jumping over 3 spikes), shows the window as a marked area in the level with the size in frames and the lowest FPS "
    "that can still hit it, and counts the clicks in a list on the left by needed FPS (20 FPS in white down to 240+ FPS in red). "
    "Orb clicks use the time the orb is touched. A sound plays at every click. Counter and marks can be hidden separately",
    false
);

namespace {
    constexpr const char* kCounterKey = "nxr.bot.frame_window_counter::counter";
    constexpr const char* kMarksKey = "nxr.bot.frame_window_counter::marks";
    constexpr const char* kSoundKey = "nxr.bot.frame_window_counter::sound";
    constexpr const char* kVolumeKey = "nxr.bot.frame_window_counter::sound_volume";
    constexpr const char* kResetKey = "nxr.bot.frame_window_counter::reset_attempt";
    constexpr const char* kScaleKey = "nxr.bot.frame_window_counter::scale";
    constexpr const char* kPosYKey = "nxr.bot.frame_window_counter::pos_y";
    constexpr const char* kHorizonKey = "nxr.bot.frame_window_counter::horizon";
    constexpr const char* kMaxKey = "nxr.bot.frame_window_counter::max_window";
    constexpr const char* kMarkTimeKey = "nxr.bot.frame_window_counter::mark_time";

    constexpr int kRows = 8;
    constexpr size_t kRing = 512;
    constexpr int kSimsPerFrame = 3;
    constexpr std::array<int, kRows - 1> kFpsSteps = {20, 30, 45, 60, 90, 120, 240};

    struct Frame {
        uint64_t tick = 0;
        bool used = false;
        bool hold = false;
        NXR::Practice::SavedPlayer state;
    };

    struct Pending {
        uint64_t tick = 0;
        uint64_t release = 0;
        uint64_t prevRelease = 0;
    };

    struct Job {
        uint64_t tick = 0;
        uint64_t release = 0;
        uint64_t prevRelease = 0;
        int window = 0;
        int horizon = 0;
        int phase = 0;
        int k = 0;
        int late = 0;
        int early = 0;
        float y = 0.f;
        std::vector<NXR::Practice::SavedPlayer> states;
        std::vector<char> holds;
        std::vector<float> xs;
    };

    struct OrbTrack {
        int overlap = 0;
        int clickAt = 0;
        bool seen = false;
        float x0 = 0.f;
        float x1 = 0.f;
        float y = 0.f;
    };

    struct Mark {
        Ref<CCNode> node;
        double expire = 0.0;
    };

    struct Sim {
        Ref<PlayerObject> fake;
        bool creating = false;
        bool cancelled = false;
    };

    const std::unordered_set<int> kPortalIDs = {101, 99, 11, 10, 200, 201, 202, 203, 1334};
    const std::unordered_set<int> kObjectTypes = {0, 2, 47, 25};

    std::vector<Frame> g_ring(kRing);
    std::deque<Pending> g_pending;
    std::deque<Job> g_jobs;
    std::unordered_map<const void*, OrbTrack> g_orbs;
    std::vector<Mark> g_marks;
    std::array<int, kRows> g_counts{};
    std::array<CCLabelBMFont*, kRows> g_rows{};
    Ref<CCNode> g_hud;
    Sim g_sim;
    uint64_t g_tick = 0;
    uint64_t g_lastRelease = 0;
    bool g_prevHold = false;
    bool g_hudDirty = true;
    const void* g_layer = nullptr;

    NXR::Hack& hackRef() {
        static NXR::Hack& hack = NXR::Gui::get().getWindow("Bot").findHackByName("Frame Window Counter");
        return hack;
    }

    bool enabled() { return hackRef().getEnabled(); }
    bool showCounter() { return NXRConfig::get().get<bool>(kCounterKey, true); }
    bool showMarks() { return NXRConfig::get().get<bool>(kMarksKey, true); }
    bool soundOn() { return NXRConfig::get().get<bool>(kSoundKey, true); }

    double nowSeconds() {
        using namespace std::chrono;
        return duration<double>(steady_clock::now().time_since_epoch()).count();
    }

    float tps() { return std::max(1.f, NXR::Bot::effectiveTps()); }

    int horizonTicks() { return std::clamp(NXRConfig::get().get<int>(kHorizonKey, 110), 40, 480); }

    int maxWindow() { return std::clamp(NXRConfig::get().get<int>(kMaxKey, 32), 4, 64); }

    ccColor3B rowColor(int row) {
        static const ccColor3B colors[kRows] = {
            ccc3(255, 255, 255), ccc3(90, 255, 130), ccc3(60, 225, 255), ccc3(90, 150, 255),
            ccc3(255, 225, 60), ccc3(255, 150, 50), ccc3(255, 100, 60), ccc3(255, 60, 60),
        };
        return colors[std::clamp(row, 0, kRows - 1)];
    }

    int rowForFps(float fps) {
        for (int i = 0; i < kRows - 1; i++) {
            if (fps <= static_cast<float>(kFpsSteps[static_cast<size_t>(i)]) + 0.001f) return i;
        }
        return kRows - 1;
    }

    std::string rowLabel(int row) {
        if (row >= kRows - 1) return fmt::format("{}+ fps", kFpsSteps[kRows - 2]);
        return fmt::format("{} fps", kFpsSteps[static_cast<size_t>(row)]);
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
        const float volume = static_cast<float>(std::clamp(NXRConfig::get().get<int>(kVolumeKey, 80), 0, 100)) / 100.f;
        engine->playEffect(clickSoundPath(), 1.f, 0.f, volume);
    }

    void dropHud() {
        if (g_hud) g_hud->removeFromParent();
        g_hud = nullptr;
        g_rows.fill(nullptr);
        g_hudDirty = true;
    }

    void refreshHud() {
        for (int i = 0; i < kRows; i++) {
            if (!g_rows[static_cast<size_t>(i)]) continue;
            const std::string text = fmt::format("{}: {}", rowLabel(i), g_counts[static_cast<size_t>(i)]);
            g_rows[static_cast<size_t>(i)]->setString(text.c_str());
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
        for (int i = 0; i < kRows; i++) {
            auto* label = CCLabelBMFont::create("", "bigFont.fnt");
            label->setAnchorPoint(CCPoint(0.f, 0.5f));
            label->setScale(0.3f * scale);
            label->setColor(rowColor(i));
            label->setPosition(CCPoint(0.f, -step * static_cast<float>(i)));
            label->setOpacity(225);
            root->addChild(label);
            g_rows[static_cast<size_t>(i)] = label;
        }
        pl->m_uiLayer->addChild(root);
        g_hud = root;
        refreshHud();
    }

    void clearMarks() {
        for (auto& mark : g_marks) {
            if (mark.node) mark.node->removeFromParent();
        }
        g_marks.clear();
    }

    void clearRun() {
        g_pending.clear();
        g_jobs.clear();
        g_orbs.clear();
        for (auto& frame : g_ring) frame.used = false;
        g_prevHold = false;
        g_lastRelease = 0;
    }

    void clearCounts() {
        g_counts.fill(0);
        g_hudDirty = true;
        refreshHud();
    }

    void emit(PlayLayer* pl, int ticks, float x0, float x1, float y, bool capped) {
        if (ticks < 1) return;
        const float fps = tps() / static_cast<float>(ticks);
        const int row = rowForFps(fps);
        g_counts[static_cast<size_t>(row)]++;
        g_hudDirty = true;

        if (!pl || !pl->m_objectLayer || !showMarks()) return;
        const float a = std::min(x0, x1);
        const float b = std::max(x0, x1);

        auto* node = CCNode::create();
        auto* draw = CCDrawNode::create();
        const ccColor3B c = rowColor(row);
        const ccColor4F fill = ccc4f(c.r / 255.f, c.g / 255.f, c.b / 255.f, 0.35f);
        const ccColor4F edge = ccc4f(c.r / 255.f, c.g / 255.f, c.b / 255.f, 0.95f);
        const float w = std::max(b - a, 2.f);
        CCPoint rect[4] = {ccp(a, y - 18.f), ccp(a + w, y - 18.f), ccp(a + w, y + 18.f), ccp(a, y + 18.f)};
        draw->drawPolygon(rect, 4, fill, 0.6f, edge);
        node->addChild(draw);

        const int need = static_cast<int>(std::ceil(fps));
        auto* label = CCLabelBMFont::create(fmt::format("{}{}f | {}fps", ticks, capped ? "+" : "", need).c_str(), "bigFont.fnt");
        label->setScale(0.3f);
        label->setColor(c);
        label->setPosition(CCPoint(a + w * 0.5f, y + 30.f));
        node->addChild(label);

        node->setZOrder(1800);
        pl->m_objectLayer->addChild(node);
        const float life = static_cast<float>(std::clamp(NXRConfig::get().get<int>(kMarkTimeKey, 3), 1, 20));
        g_marks.push_back({Ref<CCNode>(node), nowSeconds() + static_cast<double>(life)});
        if (g_marks.size() > 24) {
            if (g_marks.front().node) g_marks.front().node->removeFromParent();
            g_marks.erase(g_marks.begin());
        }
    }

    void trimMarks() {
        const double now = nowSeconds();
        for (auto it = g_marks.begin(); it != g_marks.end();) {
            if (now >= it->expire || !it->node) {
                if (it->node) it->node->removeFromParent();
                it = g_marks.erase(it);
            } else {
                ++it;
            }
        }
    }

    void clearCollisionLogs(PlayerObject* player) {
        if (player->m_collisionLogTop) player->m_collisionLogTop->removeAllObjects();
        if (player->m_collisionLogBottom) player->m_collisionLogBottom->removeAllObjects();
        if (player->m_collisionLogLeft) player->m_collisionLogLeft->removeAllObjects();
        if (player->m_collisionLogRight) player->m_collisionLogRight->removeAllObjects();
    }

    bool ensureFake(PlayLayer* pl) {
        auto* layer = pl->m_objectLayer;
        if (!layer) return false;
        if (g_sim.fake && g_sim.fake->getParent() == layer) return true;
        if (g_sim.fake && g_sim.fake->getParent()) g_sim.fake->removeFromParent();
        g_sim.fake = nullptr;
        auto* fake = PlayerObject::create(1, 1, pl, pl, true);
        if (!fake) return false;
        fake->setPosition({0.f, 105.f});
        fake->setVisible(false);
        layer->addChild(fake);
        g_sim.fake = fake;
        return true;
    }

    void dropFake() {
        if (g_sim.fake && g_sim.fake->getParent()) g_sim.fake->removeFromParent();
        g_sim = Sim{};
    }

    bool heldAt(const Job& job, int64_t tau, int64_t pressAt) {
        const int64_t t = static_cast<int64_t>(job.tick);
        const int64_t end = job.release > 0 ? static_cast<int64_t>(job.release) : t + job.horizon + job.window;
        if (tau >= pressAt && tau < end) return true;
        if (tau >= t && tau < end) return false;
        const int64_t index = tau - (t - job.window);
        if (job.holds.empty()) return false;
        const int64_t clamped = std::clamp<int64_t>(index, 0, static_cast<int64_t>(job.holds.size()) - 1);
        return job.holds[static_cast<size_t>(clamped)] != 0;
    }

    bool survives(PlayLayer* pl, Job& job, int k) {
        auto* fake = g_sim.fake.data();
        if (!fake) return false;

        const int64_t t = static_cast<int64_t>(job.tick);
        const int64_t start = t + std::min(k, 0);
        const size_t idx = static_cast<size_t>(start - (t - job.window));
        if (idx >= job.states.size() || !job.states[idx].valid) return false;

        NXR::Practice::restore(fake, job.states[idx]);
        g_sim.cancelled = false;
        g_sim.creating = true;

        const float dt = 60.f / tps();
        const int64_t pressAt = t + k;
        const int64_t last = pressAt + job.horizon;
        bool ok = true;

        for (int64_t tau = start; tau < last; tau++) {
            clearCollisionLogs(fake);
            pl->checkCollisions(fake, dt, false);
            if (g_sim.cancelled) {
                ok = false;
                break;
            }

            const bool want = heldAt(job, tau, pressAt);
            auto it = fake->m_holdingButtons.find(static_cast<int>(PlayerButton::Jump));
            const bool have = it != fake->m_holdingButtons.end() && it->second;
            if (want && !have) fake->pushButton(PlayerButton::Jump);
            else if (!want && have) fake->releaseButton(PlayerButton::Jump);

            fake->update(dt);
            fake->updateRotation(dt);
            fake->updatePlayerScale();
        }

        g_sim.creating = false;
        return ok;
    }

    void finishJob(PlayLayer* pl, Job& job) {
        const int total = job.late + job.early + 1;
        const bool capped = job.late >= job.window || job.early >= job.window;
        const int64_t t = static_cast<int64_t>(job.tick);
        auto xAt = [&](int k) {
            const size_t idx = static_cast<size_t>(t + k - (t - job.window));
            return idx < job.xs.size() ? job.xs[idx] : job.xs[static_cast<size_t>(job.window)];
        };
        emit(pl, total, xAt(-job.early), xAt(job.late), job.y, capped);
    }

    void stepJob(PlayLayer* pl, Job& job) {
        if (job.phase == 0) {
            if (!survives(pl, job, 0)) {
                job.phase = 9;
                return;
            }
            job.phase = 1;
            job.k = 1;
            return;
        }

        if (job.phase == 1) {
            const int64_t holdLen = (job.release > 0 ? static_cast<int64_t>(job.release) : static_cast<int64_t>(job.tick) + job.horizon) - static_cast<int64_t>(job.tick);
            if (job.k > job.window || job.k >= holdLen || !survives(pl, job, job.k)) {
                job.late = job.k - 1;
                job.phase = 2;
                job.k = -1;
                return;
            }
            job.k++;
            return;
        }

        if (job.phase == 2) {
            const int64_t floorK = job.prevRelease > 0 ? static_cast<int64_t>(job.prevRelease) - static_cast<int64_t>(job.tick) + 1 : -static_cast<int64_t>(job.window);
            if (-job.k > job.window || job.k < floorK || !survives(pl, job, job.k)) {
                job.early = -job.k - 1;
                finishJob(pl, job);
                job.phase = 9;
                return;
            }
            job.k--;
            return;
        }
    }

    void runJobs(PlayLayer* pl) {
        if (g_jobs.empty()) return;
        if (!ensureFake(pl)) {
            g_jobs.clear();
            return;
        }
        int budget = kSimsPerFrame;
        while (budget > 0 && !g_jobs.empty()) {
            auto& job = g_jobs.front();
            stepJob(pl, job);
            budget--;
            if (job.phase == 9) g_jobs.pop_front();
        }
    }

    Frame* frameAt(uint64_t tick) {
        auto& f = g_ring[static_cast<size_t>(tick % kRing)];
        return f.used && f.tick == tick ? &f : nullptr;
    }

    void startJob(PlayLayer* pl, const Pending& click) {
        const int horizon = horizonTicks();
        const int window = std::min(maxWindow(), horizon - 1);
        const uint64_t t = click.tick;
        if (t < static_cast<uint64_t>(window) + 1) return;

        Job job;
        job.tick = t;
        job.release = click.release;
        job.prevRelease = click.prevRelease;
        job.window = window;
        job.horizon = horizon;

        for (int i = -window; i <= horizon + window; i++) {
            const int64_t tau = static_cast<int64_t>(t) + i;
            if (tau < 0) return;
            auto* f = frameAt(static_cast<uint64_t>(tau));
            if (i <= window) {
                if (!f) return;
                job.states.push_back(f->state);
                job.xs.push_back(f->state.position.x);
            }
            if (f) job.holds.push_back(f->hold ? 1 : 0);
            else if (!job.holds.empty()) job.holds.push_back(job.holds.back());
            else return;
        }

        auto* origin = frameAt(t);
        if (!origin) return;
        job.y = origin->state.position.y;
        g_jobs.push_back(std::move(job));
        (void)pl;
    }

    void tickOrbs(PlayLayer* pl, PlayerObject* player) {
        for (auto& [key, track] : g_orbs) track.seen = false;

        auto* rings = player ? player->m_touchingRings : nullptr;
        if (rings && rings->count() > 0 && !player->m_isDead) {
            for (auto* obj : CCArrayExt<GameObject*>(rings)) {
                if (!obj) continue;
                auto& track = g_orbs[obj];
                if (track.overlap == 0) {
                    track.x0 = player->getPositionX();
                    track.y = obj->getPositionY();
                }
                track.overlap++;
                track.seen = true;
                track.x1 = player->getPositionX();
                if (obj->m_hasBeenActivated && track.clickAt == 0) track.clickAt = track.overlap;
            }
        }

        for (auto it = g_orbs.begin(); it != g_orbs.end();) {
            if (it->second.seen) {
                ++it;
                continue;
            }
            if (it->second.clickAt > 0 && it->second.overlap > 0) {
                emit(pl, it->second.overlap, it->second.x0, it->second.x1, it->second.y, false);
            }
            it = g_orbs.erase(it);
        }
    }

    void beforeTick(PlayLayer* pl) {
        auto* player = pl->m_player1;
        if (!player) return;
        g_tick++;
        auto& f = g_ring[static_cast<size_t>(g_tick % kRing)];
        f.tick = g_tick;
        f.used = true;
        f.state = NXR::Practice::capture(player);
        f.hold = f.state.holding[1];
        g_prevHold = f.hold;
    }

    void afterTick(PlayLayer* pl) {
        auto* player = pl->m_player1;
        if (!player) return;

        if (player->m_isDead) {
            g_pending.clear();
            g_jobs.clear();
            g_orbs.clear();
            return;
        }

        auto it = player->m_holdingButtons.find(static_cast<int>(PlayerButton::Jump));
        const bool holdNow = it != player->m_holdingButtons.end() && it->second;

        auto& f = g_ring[static_cast<size_t>(g_tick % kRing)];
        if (f.used && f.tick == g_tick) f.hold = holdNow;

        if (holdNow && !g_prevHold) {
            playSound();
            auto* rings = player->m_touchingRings;
            const bool onOrb = rings && rings->count() > 0;
            if (!onOrb) {
                Pending click;
                click.tick = g_tick;
                click.prevRelease = g_lastRelease;
                g_pending.push_back(click);
            }
        }
        if (!holdNow && g_prevHold) g_lastRelease = g_tick;

        for (auto& click : g_pending) {
            if (click.release == 0 && click.tick != g_tick && !holdNow) click.release = g_tick;
        }

        tickOrbs(pl, player);

        const uint64_t horizon = static_cast<uint64_t>(horizonTicks());
        while (!g_pending.empty() && g_tick >= g_pending.front().tick + horizon) {
            startJob(pl, g_pending.front());
            g_pending.pop_front();
        }
    }
}

class $modify(NXRFrameWindowGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Bot").findHackByName("Frame Window Counter");
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::processCommands");
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::collisionCheckObjects");
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::canBeActivatedByPlayer");
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::playerTouchedRing");
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::playerTouchedTrigger");
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::activateSFXTrigger");
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::activateSongEditTrigger");
        NXR::tryAddHook(self, hack, "GJBaseGameLayer::gameEventTriggered");

        hack.setForm([](NXR::Form& form) {
            form.addConfigToggle("Show Counter", kCounterKey, true);
            form.addConfigToggle("Show Marks", kMarksKey, true);
            form.addConfigToggle("Click Sound", kSoundKey, true);
            form.addConfigSlider("Sound Volume", kVolumeKey, 0.f, 100.f, 80.f, 1.f, NXR::SliderScale::Linear, {{"25%", 25.f}, {"50%", 50.f}, {"80%", 80.f}, {"100%", 100.f}}, nullptr, true, "%");
            form.addSeparator();
            form.addConfigSlider("Check Length (ticks)", kHorizonKey, 40.f, 480.f, 110.f, 5.f, NXR::SliderScale::Linear, {{"60", 60.f}, {"110", 110.f}, {"200", 200.f}, {"300", 300.f}}, nullptr, true);
            form.addConfigSlider("Max Window (ticks)", kMaxKey, 4.f, 64.f, 32.f, 1.f, NXR::SliderScale::Linear, {{"16", 16.f}, {"32", 32.f}, {"64", 64.f}}, nullptr, true);
            form.addConfigSlider("Mark Time (s)", kMarkTimeKey, 1.f, 20.f, 3.f, 1.f, NXR::SliderScale::Linear, {{"2", 2.f}, {"3", 3.f}, {"6", 6.f}}, nullptr, true);
            form.addSeparator();
            form.addConfigToggle("Reset On New Attempt", kResetKey, false);
            form.addConfigSlider("Counter Size", kScaleKey, 40.f, 250.f, 100.f, 5.f, NXR::SliderScale::Linear, {{"70%", 70.f}, {"100%", 100.f}, {"150%", 150.f}}, [](float) { dropHud(); }, true, "%");
            form.addConfigSlider("Counter Height", kPosYKey, 10.f, 95.f, 62.f, 1.f, NXR::SliderScale::Linear, {}, [](float) { dropHud(); }, true, "%");
        });
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto* pl = PlayLayer::get();
        const bool track = !isHalfTick && !g_sim.creating && pl && static_cast<GJBaseGameLayer*>(pl) == this && !pl->m_isPaused && !pl->m_levelEndAnimationStarted && enabled();
        if (track) beforeTick(pl);
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
        if (track) afterTick(pl);
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
            const bool collectible = [&] {
                switch (obj->m_objectID) {
                    case 1275: case 1329: case 1587: case 1589: case 1598: case 1614: case 3601: return true;
                    default: return obj->m_objectID >= 4401 && obj->m_objectID <= 4539;
                }
            }();
            if (allowed && !collectible) continue;
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
            if (object && player) {
                switch (object->m_objectID) {
                    case 101: player->togglePlayerScale(true, true); player->updatePlayerScale(); break;
                    case 99: player->togglePlayerScale(false, true); player->updatePlayerScale(); break;
                    case 200: player->m_playerSpeed = 0.7f; break;
                    case 201: player->m_playerSpeed = 0.9f; break;
                    case 202: player->m_playerSpeed = 1.1f; break;
                    case 203: player->m_playerSpeed = 1.3f; break;
                    case 1334: player->m_playerSpeed = 1.6f; break;
                    default: break;
                }
            }
            return false;
        }
        return GJBaseGameLayer::canBeActivatedByPlayer(player, object);
    }

    void playerTouchedRing(PlayerObject* player, RingObject* object) {
        if (g_sim.creating) return;
        GJBaseGameLayer::playerTouchedRing(player, object);
    }

    void playerTouchedTrigger(PlayerObject* player, EffectGameObject* object) {
        if (g_sim.creating) return;
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

class $modify(NXRFrameWindowPlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Bot").findHackByName("Frame Window Counter");
        NXR::trySetPriority(self, "PlayLayer::destroyPlayer", -1000);
        NXR::tryAddHook(self, hack, "PlayLayer::destroyPlayer");
        NXR::tryAddHook(self, hack, "PlayLayer::postUpdate");
        NXR::tryAddHook(self, hack, "PlayLayer::resetLevel");
        NXR::tryAddHook(self, hack, "PlayLayer::onQuit");
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (g_sim.creating || (player && player == g_sim.fake.data())) {
            g_sim.cancelled = true;
            return;
        }
        PlayLayer::destroyPlayer(player, object);
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        clearRun();
        clearMarks();
        if (NXRConfig::get().get<bool>(kResetKey, false)) clearCounts();
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (g_layer != this) {
            g_layer = this;
            dropHud();
            dropFake();
            clearRun();
            clearMarks();
            clearCounts();
        }

        trimMarks();
        runJobs(this);

        if (!showCounter()) {
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
        dropFake();
        clearRun();
        clearMarks();
        PlayLayer::onQuit();
    }
};
