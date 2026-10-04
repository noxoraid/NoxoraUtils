#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <filesystem>
#include <map>
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

using namespace geode::prelude;

NXR_HACK_CREATE(
    "Bot", "Frame Window Counter",
    "Frame perfect counter built from your own NXR macro and your own runs. Every click gets a frame window: how many physics frames "
    "earlier or later the same click would still survive (for example jumping over 3 spikes), measured with the game's own physics. "
    "A marker with the window and the lowest FPS that can hit it appears on the player at the click, a sound plays, and the label list "
    "in the top left counts the clicks by needed FPS (20 FPS white down to 240+ FPS red). While a macro plays, windows come from the macro inputs; "
    "results are kept per level so the next playback shows them instantly. Counter and markers can be hidden separately",
    false
);

namespace {
    constexpr const char* kCounterKey = "nxr.bot.frame_window_counter::counter";
    constexpr const char* kMarksKey = "nxr.bot.frame_window_counter::marks";
    constexpr const char* kSoundKey = "nxr.bot.frame_window_counter::sound";
    constexpr const char* kVolumeKey = "nxr.bot.frame_window_counter::sound_volume";
    constexpr const char* kScaleKey = "nxr.bot.frame_window_counter::scale";
    constexpr const char* kPosYKey = "nxr.bot.frame_window_counter::pos_y";
    constexpr const char* kHorizonKey = "nxr.bot.frame_window_counter::horizon";
    constexpr const char* kMaxKey = "nxr.bot.frame_window_counter::max_window";
    constexpr const char* kMarkSizeKey = "nxr.bot.frame_window_counter::mark_size";

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
        int64_t key = 0;
    };

    struct Job {
        uint64_t tick = 0;
        uint64_t release = 0;
        uint64_t prevRelease = 0;
        int64_t key = 0;
        int window = 0;
        int horizon = 0;
        int phase = 0;
        int k = 0;
        int late = 0;
        int early = 0;
        CCPoint pos;
        std::vector<NXR::Practice::SavedPlayer> states;
        std::vector<char> holds;
    };

    struct OrbTrack {
        int overlap = 0;
        bool clicked = false;
        bool seen = false;
        CCPoint pos;
    };

    struct Result {
        int ticks = 0;
        bool capped = false;
        CCPoint pos;
    };

    struct Marker {
        Ref<CCNode> node;
        CCPoint world;
    };

    struct Row {
        CCLabelBMFont* text = nullptr;
        CCLabelBMFont* count = nullptr;
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
    std::unordered_map<int, std::map<int64_t, Result>> g_store;
    std::unordered_set<int64_t> g_shown;
    std::vector<Marker> g_markers;
    std::array<int, kRows> g_counts{};
    std::array<Row, kRows> g_rows{};
    Ref<CCNode> g_hud;
    Sim g_sim;
    uint64_t g_tick = 0;
    uint64_t g_lastRelease = 0;
    bool g_prevHold = false;
    bool g_hudDirty = true;
    int g_levelId = 0;
    int64_t g_lastKey = -1;
    int g_pulse = -1;
    NXR::Bot::Mode g_lastMode = NXR::Bot::Mode::Off;
    const void* g_layer = nullptr;

    NXR::Hack& hackRef() {
        static NXR::Hack& hack = NXR::Gui::get().getWindow("Bot").findHackByName("Frame Window Counter");
        return hack;
    }

    bool enabled() { return hackRef().getEnabled(); }
    bool showCounter() { return NXRConfig::get().get<bool>(kCounterKey, true); }
    bool showMarks() { return NXRConfig::get().get<bool>(kMarksKey, true); }
    bool soundOn() { return NXRConfig::get().get<bool>(kSoundKey, true); }

    float tps() { return std::max(1.f, NXR::Bot::effectiveTps()); }
    int horizonTicks() { return std::clamp(NXRConfig::get().get<int>(kHorizonKey, 110), 40, 480); }
    int maxWindow() { return std::clamp(NXRConfig::get().get<int>(kMaxKey, 32), 4, 64); }

    int64_t keyNow(PlayLayer* pl) {
        return static_cast<int64_t>(std::llround(pl->m_gameState.m_levelTime * static_cast<double>(tps())));
    }

    std::map<int64_t, Result>& store() { return g_store[g_levelId]; }

    ccColor3B rowColor(int row) {
        static const ccColor3B colors[kRows] = {
            ccc3(255, 255, 255), ccc3(90, 255, 130), ccc3(60, 225, 255), ccc3(90, 150, 255),
            ccc3(255, 225, 60), ccc3(255, 150, 50), ccc3(255, 100, 60), ccc3(255, 60, 60),
        };
        return colors[std::clamp(row, 0, kRows - 1)];
    }

    int rowForTicks(int ticks) {
        const float fps = tps() / static_cast<float>(std::max(1, ticks));
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

    void playSound(int row) {
        if (!soundOn()) return;
        auto* engine = FMODAudioEngine::get();
        if (!engine) return;
        const float volume = static_cast<float>(std::clamp(NXRConfig::get().get<int>(kVolumeKey, 80), 0, 100)) / 100.f;
        const float speed = 0.8f + 0.07f * static_cast<float>(std::clamp(row, 0, kRows - 1));
        engine->playEffect(clickSoundPath(), speed, 0.f, volume);
    }

    void dropHud() {
        if (g_hud) g_hud->removeFromParent();
        g_hud = nullptr;
        g_rows.fill(Row{});
        g_hudDirty = true;
    }

    void refreshHud() {
        for (int i = 0; i < kRows; i++) {
            auto& row = g_rows[static_cast<size_t>(i)];
            if (row.count) row.count->setString(std::to_string(g_counts[static_cast<size_t>(i)]).c_str());
        }
    }

    void ensureHud(PlayLayer* pl) {
        if (!pl || !pl->m_uiLayer) return;
        if (g_hud && g_hud->getParent() != pl->m_uiLayer) dropHud();
        if (g_hud) return;

        const CCSize win = CCDirector::sharedDirector()->getWinSize();
        const float scale = static_cast<float>(std::clamp(NXRConfig::get().get<int>(kScaleKey, 100), 40, 250)) / 100.f;
        const float posY = static_cast<float>(std::clamp(NXRConfig::get().get<int>(kPosYKey, 95), 10, 99)) / 100.f;
        const float step = 18.f * scale;

        auto* root = CCNode::create();
        root->setPosition(CCPoint(5.f, win.height * posY));
        root->setZOrder(9999);

        float maxText = 25.f;
        for (int i = 0; i < kRows; i++) {
            auto* text = CCLabelBMFont::create((rowLabel(i) + ":").c_str(), "bigFont.fnt");
            text->setAnchorPoint(CCPoint(0.f, 1.f));
            text->setScale(0.45f * scale);
            text->setColor(rowColor(i));
            maxText = std::max(maxText, text->getScaledContentSize().width);
            g_rows[static_cast<size_t>(i)].text = text;
            root->addChild(text);
        }
        for (int i = 0; i < kRows; i++) {
            auto& row = g_rows[static_cast<size_t>(i)];
            row.text->setPosition(CCPoint(0.f, -step * static_cast<float>(i)));
            auto* count = CCLabelBMFont::create("0", "bigFont.fnt");
            count->setAnchorPoint(CCPoint(0.f, 1.f));
            count->setScale(0.45f * scale);
            count->setColor(rowColor(i));
            count->setPosition(CCPoint(maxText + 4.f, -step * static_cast<float>(i)));
            row.count = count;
            root->addChild(count);
        }
        pl->m_uiLayer->addChild(root);
        g_hud = root;
        refreshHud();
    }

    void pulseRow(int row) {
        if (row < 0 || row >= kRows) return;
        auto* count = g_rows[static_cast<size_t>(row)].count;
        if (!count) return;
        const float scale = static_cast<float>(std::clamp(NXRConfig::get().get<int>(kScaleKey, 100), 40, 250)) / 100.f;
        const ccColor3B c = rowColor(row);
        count->stopAllActions();
        count->setScale(0.45f * scale);
        count->setColor(c);
        count->runAction(CCSequence::create(
            CCEaseSineOut::create(CCScaleTo::create(0.06f, 0.58f * scale)),
            CCEaseSineOut::create(CCScaleTo::create(0.2f, 0.45f * scale)),
            nullptr
        ));
        count->runAction(CCSequence::create(
            CCEaseSineOut::create(CCTintTo::create(0.06f, 255, 255, 255)),
            CCEaseSineOut::create(CCTintTo::create(0.2f, c.r, c.g, c.b)),
            nullptr
        ));
    }

    void clearMarkers() {
        for (auto& marker : g_markers) {
            if (marker.node) marker.node->removeFromParent();
        }
        g_markers.clear();
    }

    void spawnMarker(PlayLayer* pl, const Result& result, int row) {
        if (!pl || !pl->m_uiLayer || !pl->m_objectLayer || !showMarks()) return;

        auto* node = CCNode::create();
        const ccColor3B c = rowColor(row);
        const float size = static_cast<float>(std::clamp(NXRConfig::get().get<int>(kMarkSizeKey, 100), 40, 250)) / 100.f;

        auto* circle = CCDrawNode::create();
        CCPoint verts[48];
        const float radius = 13.f;
        for (int i = 0; i < 48; i++) {
            const float angle = static_cast<float>(i) * (3.14159265f * 2.f) / 48.f;
            verts[i] = CCPoint(radius * std::cos(angle), radius * std::sin(angle));
        }
        circle->drawPolygon(verts, 48, ccc4f(0.f, 0.f, 0.f, 0.f), 4.f, ccc4f(0.f, 0.f, 0.f, 1.f));
        circle->drawPolygon(verts, 48, ccc4f(0.f, 0.f, 0.f, 0.f), 2.f, ccc4f(c.r / 255.f, c.g / 255.f, c.b / 255.f, 1.f));
        node->addChild(circle, 0);

        const int need = static_cast<int>(std::ceil(tps() / static_cast<float>(std::max(1, result.ticks))));
        auto* label = CCLabelBMFont::create(fmt::format("{}{}f ({}fps)", result.ticks, result.capped ? "+" : "", need).c_str(), "bigFont.fnt");
        label->setAnchorPoint(CCPoint(1.f, 0.5f));
        label->setPosition(CCPoint(-18.f, 0.f));
        label->setScale(0.5f);
        label->setColor(c);
        node->addChild(label, 1);

        node->setScale(std::abs(pl->m_objectLayer->getScaleY()) * size);
        node->setPosition(pl->m_objectLayer->convertToWorldSpace(result.pos));
        pl->m_uiLayer->addChild(node, 50);
        g_markers.push_back({Ref<CCNode>(node), result.pos});
    }

    void updateMarkers(PlayLayer* pl) {
        if (!pl || !pl->m_objectLayer) return;
        const CCSize win = CCDirector::sharedDirector()->getWinSize();
        const float layerScale = std::abs(pl->m_objectLayer->getScaleY());
        const float size = static_cast<float>(std::clamp(NXRConfig::get().get<int>(kMarkSizeKey, 100), 40, 250)) / 100.f;
        constexpr float margin = 300.f;

        for (auto it = g_markers.begin(); it != g_markers.end();) {
            if (!it->node || !it->node->getParent()) {
                it = g_markers.erase(it);
                continue;
            }
            const CCPoint screen = pl->m_objectLayer->convertToWorldSpace(it->world);
            it->node->setPosition(screen);
            it->node->setScale(layerScale * size);
            if (screen.x < -margin || screen.x > win.width + margin || screen.y < -margin || screen.y > win.height + margin) {
                it->node->removeFromParent();
                it = g_markers.erase(it);
            } else {
                ++it;
            }
        }
    }

    void recount(int64_t upTo) {
        g_counts.fill(0);
        g_shown.clear();
        for (auto& [key, result] : store()) {
            if (key > upTo) break;
            g_counts[static_cast<size_t>(rowForTicks(result.ticks))]++;
            g_shown.insert(key);
        }
        g_hudDirty = true;
    }

    void showResult(PlayLayer* pl, int64_t key, const Result& result, bool sound) {
        if (g_shown.contains(key)) return;
        g_shown.insert(key);
        const int row = rowForTicks(result.ticks);
        g_counts[static_cast<size_t>(row)]++;
        g_hudDirty = true;
        g_pulse = row;
        spawnMarker(pl, result, row);
        if (sound) playSound(row);
    }

    void storeResult(PlayLayer* pl, int64_t key, int ticks, bool capped, const CCPoint& pos, bool sound) {
        if (ticks < 1) return;
        Result result;
        result.ticks = ticks;
        result.capped = capped;
        result.pos = pos;
        store()[key] = result;
        showResult(pl, key, result, sound);
    }

    void resetAll() {
        g_pending.clear();
        g_jobs.clear();
        g_orbs.clear();
        for (auto& frame : g_ring) frame.used = false;
        g_prevHold = false;
        g_lastRelease = 0;
        clearMarkers();
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
        if (job.holds.empty()) return false;
        const int64_t index = tau - (t - job.window);
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
            const int64_t holdEnd = job.release > 0 ? static_cast<int64_t>(job.release) : static_cast<int64_t>(job.tick) + job.horizon;
            const int64_t holdLen = holdEnd - static_cast<int64_t>(job.tick);
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
            const int64_t floorK = job.prevRelease > 0
                ? static_cast<int64_t>(job.prevRelease) - static_cast<int64_t>(job.tick) + 1
                : -static_cast<int64_t>(job.window);
            if (-job.k > job.window || job.k < floorK || !survives(pl, job, job.k)) {
                job.early = -job.k - 1;
                const bool capped = job.late >= job.window || job.early >= job.window;
                storeResult(pl, job.key, job.late + job.early + 1, capped, job.pos, false);
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

    bool collectStates(Job& job, uint64_t t, int window) {
        for (int i = -window; i <= 0; i++) {
            const int64_t tau = static_cast<int64_t>(t) + i;
            if (tau < 0) return false;
            auto* f = frameAt(static_cast<uint64_t>(tau));
            if (!f) return false;
            job.states.push_back(f->state);
        }
        auto* origin = frameAt(t);
        if (!origin) return false;
        job.pos = origin->state.position;
        return true;
    }

    void startRecordedJob(const Pending& click) {
        const int horizon = horizonTicks();
        const int window = std::min(maxWindow(), horizon - 1);
        const uint64_t t = click.tick;
        if (t < static_cast<uint64_t>(window) + 1) return;

        Job job;
        job.tick = t;
        job.release = click.release;
        job.prevRelease = click.prevRelease;
        job.key = click.key;
        job.window = window;
        job.horizon = horizon;

        if (!collectStates(job, t, window)) return;

        for (int i = -window; i <= horizon + window; i++) {
            const int64_t tau = static_cast<int64_t>(t) + i;
            auto* f = frameAt(static_cast<uint64_t>(tau));
            if (f) job.holds.push_back(f->hold ? 1 : 0);
            else if (!job.holds.empty()) job.holds.push_back(job.holds.back());
            else return;
        }
        g_jobs.push_back(std::move(job));
    }

    bool startMacroJob(uint64_t t, int64_t key, uint64_t macroFrame) {
        auto& st = NXR::Bot::State::get();
        std::vector<std::pair<int64_t, bool>> toggles;
        for (const auto& ev : st.current.events) {
            if (ev.player() != 1 || ev.button() != 1) continue;
            toggles.emplace_back(static_cast<int64_t>(ev.frame()), ev.down());
        }
        if (toggles.empty()) return false;

        int64_t pressFrame = -1;
        int64_t bestDist = 4;
        for (const auto& [frame, down] : toggles) {
            if (!down) continue;
            const int64_t dist = std::llabs(frame - static_cast<int64_t>(macroFrame));
            if (dist < bestDist) {
                bestDist = dist;
                pressFrame = frame;
            }
        }
        if (pressFrame < 0) return false;

        const int horizon = horizonTicks();
        const int window = std::min(maxWindow(), horizon - 1);
        if (t < static_cast<uint64_t>(window) + 1) return false;

        Job job;
        job.tick = t;
        job.key = key;
        job.window = window;
        job.horizon = horizon;

        int64_t nextUp = -1;
        int64_t prevUp = -1;
        for (const auto& [frame, down] : toggles) {
            if (down) continue;
            if (frame > pressFrame && (nextUp < 0 || frame < nextUp)) nextUp = frame;
            if (frame < pressFrame && frame > prevUp) prevUp = frame;
        }
        if (nextUp > 0) job.release = static_cast<uint64_t>(static_cast<int64_t>(t) + (nextUp - pressFrame));
        if (prevUp > 0 && static_cast<int64_t>(t) + (prevUp - pressFrame) > 0) {
            job.prevRelease = static_cast<uint64_t>(static_cast<int64_t>(t) + (prevUp - pressFrame));
        }

        if (!collectStates(job, t, window)) return false;

        auto holdAt = [&](int64_t frame) {
            bool held = false;
            for (const auto& [f, down] : toggles) {
                if (f > frame) break;
                held = down;
            }
            return held;
        };
        for (int i = -window; i <= horizon + window; i++) {
            job.holds.push_back(holdAt(pressFrame + i) ? 1 : 0);
        }
        g_jobs.push_back(std::move(job));
        return true;
    }

    void tickOrbs(PlayLayer* pl, PlayerObject* player, bool clicked) {
        for (auto& [obj, track] : g_orbs) track.seen = false;

        auto* rings = player ? player->m_touchingRings : nullptr;
        if (rings && rings->count() > 0 && !player->m_isDead) {
            for (auto* obj : CCArrayExt<GameObject*>(rings)) {
                if (!obj) continue;
                auto& track = g_orbs[obj];
                if (track.overlap == 0) track.pos = obj->getPosition();
                track.overlap++;
                track.seen = true;
                if (clicked) track.clicked = true;
            }
        }

        for (auto it = g_orbs.begin(); it != g_orbs.end();) {
            if (it->second.seen) {
                ++it;
                continue;
            }
            if (it->second.clicked && it->second.overlap > 0) {
                const int64_t key = keyNow(pl);
                storeResult(pl, key, it->second.overlap, false, it->second.pos, false);
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

        const bool pressed = holdNow && !g_prevHold;
        const int64_t key = keyNow(pl);

        if (pressed) {
            auto* rings = player->m_touchingRings;
            const bool onOrb = rings && rings->count() > 0;
            auto& known = store();
            auto found = known.find(key);

            if (found != known.end()) {
                const int row = rowForTicks(found->second.ticks);
                if (!g_shown.contains(key)) showResult(pl, key, found->second, true);
                else playSound(row);
            } else {
                playSound(3);
                if (!onOrb) {
                    auto& st = NXR::Bot::State::get();
                    bool queued = false;
                    if (st.mode == NXR::Bot::Mode::Playing) queued = startMacroJob(g_tick, key, st.frame);
                    if (!queued) {
                        Pending click;
                        click.tick = g_tick;
                        click.prevRelease = g_lastRelease;
                        click.key = key;
                        g_pending.push_back(click);
                    }
                }
            }
        }
        if (!holdNow && g_prevHold) g_lastRelease = g_tick;

        for (auto& click : g_pending) {
            if (click.release == 0 && click.tick != g_tick && !holdNow) click.release = g_tick;
        }

        tickOrbs(pl, player, pressed);

        const uint64_t horizon = static_cast<uint64_t>(horizonTicks());
        while (!g_pending.empty() && g_tick >= g_pending.front().tick + horizon) {
            startRecordedJob(g_pending.front());
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
            form.addConfigToggle("Show Markers", kMarksKey, true);
            form.addConfigToggle("Click Sound", kSoundKey, true);
            form.addConfigSlider("Sound Volume", kVolumeKey, 0.f, 100.f, 80.f, 1.f, NXR::SliderScale::Linear, {{"25%", 25.f}, {"50%", 50.f}, {"80%", 80.f}, {"100%", 100.f}}, nullptr, true, "%");
            form.addSeparator();
            form.addConfigSlider("Check Length (ticks)", kHorizonKey, 40.f, 480.f, 110.f, 5.f, NXR::SliderScale::Linear, {{"60", 60.f}, {"110", 110.f}, {"200", 200.f}, {"300", 300.f}}, nullptr, true);
            form.addConfigSlider("Max Window (ticks)", kMaxKey, 4.f, 64.f, 32.f, 1.f, NXR::SliderScale::Linear, {{"16", 16.f}, {"32", 32.f}, {"64", 64.f}}, nullptr, true);
            form.addSeparator();
            form.addConfigSlider("Marker Size", kMarkSizeKey, 40.f, 250.f, 100.f, 5.f, NXR::SliderScale::Linear, {{"70%", 70.f}, {"100%", 100.f}, {"150%", 150.f}}, nullptr, true, "%");
            form.addConfigSlider("Counter Size", kScaleKey, 40.f, 250.f, 100.f, 5.f, NXR::SliderScale::Linear, {{"70%", 70.f}, {"100%", 100.f}, {"150%", 150.f}}, [](float) { dropHud(); }, true, "%");
            form.addConfigSlider("Counter Height", kPosYKey, 10.f, 99.f, 95.f, 1.f, NXR::SliderScale::Linear, {}, [](float) { dropHud(); }, true, "%");
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
        resetAll();
        recount(keyNow(this));
        refreshHud();
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        const int levelId = m_level ? static_cast<int>(m_level->m_levelID.value()) : 0;
        if (g_layer != this) {
            g_layer = this;
            g_levelId = levelId;
            dropHud();
            dropFake();
            resetAll();
            recount(keyNow(this));
        }

        const auto mode = NXR::Bot::State::get().mode;
        if (mode != g_lastMode) {
            if (mode == NXR::Bot::Mode::Recording) {
                g_store[g_levelId].clear();
                recount(-1);
            }
            g_lastMode = mode;
        }

        updateMarkers(this);
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
            if (g_pulse >= 0) {
                pulseRow(g_pulse);
                g_pulse = -1;
            }
        }
    }

    void onQuit() {
        g_layer = nullptr;
        dropHud();
        dropFake();
        resetAll();
        PlayLayer::onQuit();
    }
};
