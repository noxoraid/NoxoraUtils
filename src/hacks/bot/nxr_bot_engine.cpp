#include <Geode/Geode.hpp>
#include <algorithm>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_bot.hpp"
#include "../../core/nxr_practice_fix.hpp"
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <memory>
#include <cmath>

// Bot engine: records and replays a run frame by frame.
//
// Everything hangs off three hooks:
//   GJBaseGameLayer::processCommands   one call per physics tick; this is where the frame
//                                      number advances and rows are recorded or applied
//   GJBaseGameLayer::processQueuedButtons / handleButton
//                                      inputs go in (playback) or come out (recording)
//   PlayLayer::resetLevel / checkpoints
//                                      rewinds the frame counter and cuts the macro
//
// The macro data model lives in core/nxr_bot.hpp, the file format in core/nxr_macro_io.cpp.
namespace {
    using namespace NXR::Bot;
    namespace Cap = NXR::Capture;

    // Hook priorities. Lower runs first. Other mods hook the same functions, so these are
    // explicit rather than left to load order.
    constexpr int kProcessCommandsPriority = -30;
    constexpr int kButtonPriority = -1000;  // must see the button state after every other mod

    // A full player snapshot is stored this often (in ticks). Playback restores the nearest
    // earlier snapshot when it detects drift, so a smaller value means finer repair but a
    // bigger file.
    constexpr uint32_t kSuperInterval = 30;

    // True only while we are inside processQueuedButtons; lets handleButton tell our own
    // replayed inputs apart from real touches.
    bool g_inQueue = false;
    // True only while injectReplayInputs queues a button, so the input blocker lets it through.
    bool g_replayInject = false;

    // The frame number is derived from the game's tick counter plus a bias. The bias is
    // recalculated after every reset so a checkpoint load lands on the macro frame that
    // checkpoint was stored at.
    int64_t g_frameBias = 0;
    bool g_needAnchor = true;
    Mode g_anchorMode = Mode::Off;

    // Whether m_currentProgress has already been incremented when processCommands is entered
    // is not the same on every platform/build. We watch a few ticks and decide at runtime
    // (see the probe at the end of processCommands) instead of hard-coding it.
    bool g_progressInside = true;
    bool g_progressProbed = false;
    int g_outsideVotes = 0;
    int g_lastProgressAfter = -1;

    int64_t completedTicks(GJBaseGameLayer* layer) {
        return static_cast<int64_t>(layer->m_gameState.m_currentProgress);
    }

    // Frame numbers start at 1. The +1 compensates when the tick counter is incremented
    // inside processCommands (g_progressInside) rather than before it.
    uint64_t anchoredFrame(GJBaseGameLayer* layer) {
        const int64_t frame = completedTicks(layer) + (g_progressInside ? 1 : 0) + g_frameBias;
        return frame < 1 ? uint64_t{1} : static_cast<uint64_t>(frame);
    }

    bool ignoringInputs() {
        return NXRConfig::get().get<bool>("nxr.bot.bot_engine::ignore_input", true);
    }

    bool practiceFixEnabled() {
        return NXRConfig::get().get<bool>("nxr.bot.practice_fix", true);
    }

    bool fixRandomEnabled() {
        return NXRConfig::get().get<bool>("nxr.bot.fix_random", true);
    }

    bool playbackDeathEnabled() {
        return NXRConfig::get().get<bool>("nxr.bot.playback_death", true);
    }

    bool rescueEnabled() {
        return NXRConfig::get().get<bool>("nxr.bot.desync_rescue", true);
    }

    bool cbfBypassEnabled() {
        return NXRConfig::get().get<bool>("nxr.bot.cbf_bypass", true);
    }

    bool g_cbfSuspended = false;
    bool g_cbfPrevious = false;

    uint64_t seedForFrame(uint64_t frame) {
        uint64_t z = frame + 0x9E3779B97F4A7C15ULL;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    std::unordered_map<const void*, std::shared_ptr<NXR::Practice::SavedPair>> g_saved;

    bool isPlatformer() {
        auto* pl = PlayLayer::get();
        return pl && pl->m_levelSettings && pl->m_levelSettings->m_platformerMode;
    }

    bool controlsFlipped() {
        if (isPlatformer()) return false;
        return GameManager::get()->getGameVariable("0010");
    }

    bool playerFlipped(bool player2) {
        return player2 ^ controlsFlipped();
    }

    bool isActiveLayer(GJBaseGameLayer* layer) {
        auto* pl = PlayLayer::get();
        return pl && static_cast<GJBaseGameLayer*>(pl) == layer;
    }

    // While a replay plays, real touches are dropped (option "Ignore Inputs") so the player
    // cannot disturb it. Inputs we inject ourselves carry g_replayInject / g_inQueue.
    bool blockRealInput(GJBaseGameLayer* layer) {
        return State::get().mode == Mode::Playing && ignoringInputs() && !g_replayInject && isActiveLayer(layer);
    }

    uint32_t superLayout() {
        static const uint32_t layout = Cap::blobLayout<PlayerObject>();
        return layout;
    }

    uint8_t heldMask() {
        auto& st = State::get();
        uint8_t mask = 0;
        for (int slot = 0; slot < 2; slot++) {
            for (int button = 1; button <= 3; button++) {
                if (st.held[slot][button]) mask |= holdBit(slot, button);
            }
        }
        return mask;
    }

    void stampHeader(PlayLayer* pl) {
        auto& m = State::get().current;
        auto& cfg = NXRConfig::get();
        const bool limiter = cfg.get<bool>("nxr.player.fps_limiter", false);

        m.tps = effectiveTps();
        m.fps = limiter ? static_cast<float>(std::clamp(cfg.get<int>("nxr.player.fps_limiter::fps", 240), 0, 5000000)) : 0.f;
        m.version = geode::Mod::get()->getVersion().toVString();

        if (pl) m.levelHash = NXR::Stats::fingerprint(pl);

        if (pl && pl->m_level) {
            m.levelName = pl->m_level->m_levelName;
            m.levelId = static_cast<int32_t>(pl->m_level->m_levelID.value());
        }
    }

    void recordSuper(GJBaseGameLayer* layer, uint64_t frame) {
        auto& m = State::get().current;
        if (!layer->m_player1 || !layer->m_player2) return;
        if (!m.supers.empty() && m.supers.back().frame >= frame) return;

        SuperFrame sf;
        sf.frame = static_cast<uint32_t>(frame);
        Cap::takeBlob(*layer->m_player1, sf.p1);
        Cap::takeBlob(*layer->m_player2, sf.p2);
        m.layout = superLayout();
        m.supers.push_back(std::move(sf));
    }

    void recordRow(GJBaseGameLayer* layer, uint64_t frame, bool forceSuper, int holdOverride = -1) {
        auto& m = State::get().current;

        if (!layer->m_player1 || !layer->m_player2) return;
        if (frame == 0 || frame > InputEvent::kMaxFrame) return;

        const bool dual = layer->m_gameState.m_isDualMode;
        if (layer->m_player1->m_isDead || (dual && layer->m_player2->m_isDead)) return;
        if (!m.frames.empty() && m.frames.back().frame >= frame) return;

        MacroFrame row;
        row.frame = static_cast<uint32_t>(frame);
        row.p1 = Cap::readState(layer->m_player1);
        row.p2 = Cap::readState(layer->m_player2);
        if (dual) row.p2.flags |= Cap::kDualBit;
        row.hold = holdOverride >= 0 ? static_cast<uint8_t>(holdOverride) : heldMask();
        row.full = true;
        m.frames.push_back(row);
        m.totalFrames = std::max<uint64_t>(m.totalFrames, frame);

        if (forceSuper || frame == 1 || frame % kSuperInterval == 0) recordSuper(layer, frame);
    }

    void applyHold(GJBaseGameLayer* layer, uint8_t mask, bool dual) {
        const int count = dual ? 2 : 1;
        for (int i = 0; i < count; i++) {
            auto* player = i == 0 ? layer->m_player1 : layer->m_player2;
            if (!player) continue;

            const int slot = playerFlipped(i == 1) ? 1 : 0;
            for (int button = 1; button <= 3; button++) {
                player->m_holdingButtons[button] = (mask & holdBit(slot, button)) != 0;
            }
        }
    }

    const SuperFrame* superFast(const Macro& m, uint64_t frame);

    void restoreSuper(GJBaseGameLayer* layer, const Macro& m, uint64_t frame, bool dual) {
        if (m.supers.empty() || m.layout != superLayout()) return;

        const SuperFrame* sf = superFast(m, frame);
        if (!sf) return;

        if (layer->m_player1) Cap::putBlob(*layer->m_player1, sf->p1);
        if (dual && layer->m_player2) Cap::putBlob(*layer->m_player2, sf->p2);
    }

    size_t g_rowHint = 0;
    size_t g_superHint = 0;

    const MacroFrame* rowFast(const Macro& m, uint64_t frame) {
        const auto& rows = m.frames;
        if (rows.empty()) return nullptr;
        if (g_rowHint >= rows.size()) g_rowHint = 0;
        if (rows[g_rowHint].frame == frame) return &rows[g_rowHint];
        if (g_rowHint + 1 < rows.size() && rows[g_rowHint + 1].frame == frame) return &rows[++g_rowHint];

        auto it = std::partition_point(rows.begin(), rows.end(), [frame](const MacroFrame& row) {
            return static_cast<uint64_t>(row.frame) < frame;
        });
        if (it == rows.end() || static_cast<uint64_t>(it->frame) != frame) return nullptr;
        g_rowHint = static_cast<size_t>(it - rows.begin());
        return &*it;
    }

    const SuperFrame* superFast(const Macro& m, uint64_t frame) {
        const auto& list = m.supers;
        if (list.empty()) return nullptr;
        if (g_superHint >= list.size()) g_superHint = 0;
        if (list[g_superHint].frame == frame) return &list[g_superHint];
        if (g_superHint + 1 < list.size() && list[g_superHint + 1].frame == frame) return &list[++g_superHint];

        auto it = std::partition_point(list.begin(), list.end(), [frame](const SuperFrame& sf) {
            return static_cast<uint64_t>(sf.frame) < frame;
        });
        if (it == list.end() || static_cast<uint64_t>(it->frame) != frame) return nullptr;
        g_superHint = static_cast<size_t>(it - list.begin());
        return &*it;
    }

    void enforceMode(PlayLayer* pl, PlayerObject* p, uint32_t flags) {
        if (!pl || !p || !(flags & Cap::kModeValid)) return;

        const int want = static_cast<int>((flags & Cap::kModeMask) >> Cap::kModeShift);
        const int have = Cap::modeOf(*p);

        if (want != have) {
            switch (want) {
                case 0:
                    p->toggleFlyMode(false, false);
                    p->toggleRollMode(false, false);
                    p->toggleBirdMode(false, false);
                    p->toggleDartMode(false, false);
                    p->toggleRobotMode(false, false);
                    p->toggleSpiderMode(false, false);
                    p->toggleSwingMode(false, false);
                    break;
                case 1: p->toggleFlyMode(true, true); break;
                case 2: p->toggleRollMode(true, true); break;
                case 3: p->toggleBirdMode(true, true); break;
                case 4: p->toggleDartMode(true, true); break;
                case 5: p->toggleRobotMode(true, true); break;
                case 6: p->toggleSpiderMode(true, true); break;
                case 7: p->toggleSwingMode(true, true); break;
                default: break;
            }

            auto* obj = TeleportPortalObject::create("edit_eGameRotBtn_001.png", true);
            obj->m_cameraIsFreeMode = true;
            pl->playerWillSwitchMode(p, obj);
        }

        const bool wantMini = (flags & Cap::kMiniBit) != 0;
        const bool haveMini = p->m_vehicleSize < 0.9f;
        if (wantMini != haveMini) p->togglePlayerScale(wantMini, true);

        const int speedIdx = static_cast<int>((flags & Cap::kSpeedMask) >> Cap::kSpeedShift);
        const float wantSpeed = Cap::speedFromIndex(speedIdx);
        if (wantSpeed > 0.f && std::fabs(p->m_playerSpeed - wantSpeed) > 0.001f) p->m_playerSpeed = wantSpeed;
    }

    void syncGravity(PlayerObject* p, const NXR::Capture::PlayerState& state) {
        if (!p || !(state.flags & Cap::kModeValid)) return;
        const bool want = (state.flags & 2u) != 0;
        if (p->m_isUpsideDown == want) return;
        p->flipGravity(want, true);
    }

    // How far the live player may stray from the recorded row before playback rewrites it.
    // Positions are in game units, rotation in degrees. Small values keep the replay exact;
    // large values would let tiny float differences accumulate into a death.
    bool levelMatchesMacro(PlayLayer* pl) {
        static const void* cachedLayer = nullptr;
        static uint64_t cachedKey = 0;
        static bool cachedResult = true;

        auto& m = State::get().current;
        if (!pl) return true;

        const uint64_t key = m.levelHash != 0
            ? m.levelHash
            : (m.hasStats ? (static_cast<uint64_t>(m.stats.objects) << 40) ^ (static_cast<uint64_t>(m.stats.solids) << 20) ^ m.stats.hazards ^ 0x5bd1e995ull : 0);
        if (key == 0) return true;
        if (cachedLayer == pl && cachedKey == key) return cachedResult;

        if (m.levelHash != 0) {
            cachedResult = NXR::Stats::fingerprint(pl) == m.levelHash;
        } else {
            const auto now = NXR::Stats::compute(pl);
            const auto& was = m.stats;
            cachedResult = now.objects == was.objects && now.solids == was.solids && now.hazards == was.hazards;
        }
        cachedLayer = pl;
        cachedKey = key;
        return cachedResult;
    }

    bool repairAllowed(PlayLayer* pl) {
        if (!rescueEnabled()) return false;
        auto& st = State::get();
        if (st.current.noclip && !NXRConfig::get().get<bool>("nxr.player.noclip", false)) return false;
        return levelMatchesMacro(pl);
    }

    constexpr float kDriftEps = 0.002f;
    constexpr float kVelEps = 0.002f;
    constexpr float kRotEps = 1.f;

    bool drifted(PlayerObject* p, const NXR::Capture::PlayerState& s, bool full) {
        if (!p || s.x == 0.f || s.y == 0.f) return false;
        const auto pos = p->getPosition();
        if (std::fabs(pos.x - s.x) > kDriftEps || std::fabs(pos.y - s.y) > kDriftEps) return true;
        if (full && std::fabs(static_cast<float>(p->m_yVelocity) - s.yVel) > kVelEps) return true;
        return false;
    }

    bool rotationOff(PlayerObject* p, const NXR::Capture::PlayerState& s) {
        if (!p || s.rot == 0.f) return false;
        const float cur = Cap::wrapRotation(p->getRotation());
        const float diff = std::fabs(std::fmod(cur - s.rot + 540.f, 360.f) - 180.f);
        return diff > kRotEps;
    }

    // Brings the players in line with the recorded row for `frame`.
    // Order matters: mode/gravity first (they change what the other fields mean), then a
    // position check. Only when the player has drifted do we restore the nearest super frame
    // and overwrite the state; otherwise we leave the game's own physics alone.
    void applyPlayback(GJBaseGameLayer* layer, uint64_t frame, bool force = false) {
        auto& m = State::get().current;
        if (frame == 0 || m.frames.empty()) return;
        if (!layer->m_player1 || !layer->m_player2 || layer->m_player1->m_isDead) return;

        const MacroFrame* row = rowFast(m, frame);
        if (!row) return;

        const bool dual = layer->m_gameState.m_isDualMode;
        const bool platformer = isPlatformer();

        if (auto* pl = PlayLayer::get(); pl && static_cast<GJBaseGameLayer*>(pl) == layer) {
            enforceMode(pl, layer->m_player1, row->p1.flags);
            if (dual) enforceMode(pl, layer->m_player2, row->p2.flags);
        }

        if (row->full) {
            syncGravity(layer->m_player1, row->p1);
            if (dual) syncGravity(layer->m_player2, row->p2);
        }

        const bool repair = repairAllowed(PlayLayer::get());
        const bool off = force
            || (repair && (drifted(layer->m_player1, row->p1, row->full)
            || (dual && drifted(layer->m_player2, row->p2, row->full))));

        if (off) {
            restoreSuper(layer, m, frame, dual);
            Cap::writeState(layer->m_player1, row->p1, row->full, platformer);
            if (dual) Cap::writeState(layer->m_player2, row->p2, row->full, platformer);
        } else if (repair) {
            if (rotationOff(layer->m_player1, row->p1)) layer->m_player1->setRotation(row->p1.rot);
            if (dual && rotationOff(layer->m_player2, row->p2)) layer->m_player2->setRotation(row->p2.rot);
        }

        applyHold(layer, row->hold, dual);
    }

    // A death during playback while a recorded row exists for this frame means the replay
    // drifted (the original run survived here). Returns true so the caller can swallow the
    // death and let applyPlayback repair the state on the next tick.
    bool globalNoclipEnabled() {
        return NXRConfig::get().get<bool>("nxr.player.noclip", false);
    }

    bool isDesyncDeath(PlayLayer* pl, GameObject* object) {
        auto& st = State::get();
        if (st.mode != Mode::Playing || !repairAllowed(pl)) return false;
        if (!pl || pl->m_levelEndAnimationStarted) return false;
        if (object && object == pl->m_anticheatSpike) return false;
        if (st.current.frames.empty() || st.frame == 0) return false;
        if (pl->m_player1 && pl->m_player1->m_isDead) return false;
        return rowFast(st.current, st.frame) != nullptr;
    }

    void noteRescue() {
        auto& st = State::get();
        if (st.rescues == 0) st.firstRescueFrame = st.frame;
        if (st.lastRescueFrame != st.frame) st.rescues++;
        st.lastRescueFrame = st.frame;
    }

    void addInput(uint64_t frame, bool player2, int button, bool down) {
        auto& st = State::get();
        if (button < 1 || button > 3) return;
        if (frame > InputEvent::kMaxFrame) return;
        if (!st.current.events.empty() && st.current.events.back().frame() > frame) return;

        const int slot = player2 ? 1 : 0;
        bool& held = st.held[slot][button];
        if (held == down) return;
        held = down;

        auto& events = st.current.events;
        size_t sameFrame = 0;
        for (auto it = events.rbegin(); it != events.rend() && it->frame() == frame; ++it) {
            if (it->player() == (player2 ? 2 : 1) && it->button() == button) sameFrame++;
        }

        if (sameFrame >= 2) {
            for (auto it = events.rbegin(); it != events.rend() && it->frame() == frame; ++it) {
                if (it->player() == (player2 ? 2 : 1) && it->button() == button) {
                    *it = InputEvent::make(frame, player2 ? 2 : 1, static_cast<uint8_t>(button), down);
                    break;
                }
            }
            return;
        }

        events.push_back(InputEvent::make(frame, player2 ? 2 : 1, static_cast<uint8_t>(button), down));
        st.current.totalFrames = std::max(st.current.totalFrames, frame);
    }

    void recordDirect(PlayerObject* player, PlayerButton button, bool down) {
        auto& st = State::get();
        if (g_inQueue || st.mode != Mode::Recording) return;

        auto* pl = PlayLayer::get();
        if (!pl || !pl->m_player1 || pl->m_player1->m_isDead) return;

        const bool isP2 = player == pl->m_player2 && player != pl->m_player1;
        if (!isP2 && player != pl->m_player1) return;
        if (isP2 && !pl->m_gameState.m_isDualMode) return;

        addInput(st.frame, playerFlipped(isP2), static_cast<int>(button), down);
    }

    void recordQueued(GJBaseGameLayer* layer) {
        auto& st = State::get();
        if (st.mode != Mode::Recording) return;
        if (!layer->m_player1 || layer->m_player1->m_isDead) return;
        if (layer->m_queuedButtons.empty()) return;

        for (auto& cmd : layer->m_queuedButtons) {
            addInput(st.frame, playerFlipped(cmd.m_isPlayer2), static_cast<int>(cmd.m_button), cmd.m_isPush);
        }
    }

    void rebuildHeld(uint64_t frame) {
        auto& st = State::get();
        st.held = {};

        for (const auto& ev : st.current.events) {
            if (ev.frame() > frame) break;
            const int button = ev.button();
            if (button < 1 || button > 3) continue;
            st.held[ev.player() == 2 ? 1 : 0][button] = ev.down();
        }
    }

    void onReset(uint64_t newFrame) {
        auto& st = State::get();
        auto& m = st.current;

        if (st.mode == Mode::Recording) {
            if (newFrame == 0) {
                m.events.clear();
                m.frames.clear();
                m.supers.clear();
            } else {
                auto cutEvents = std::partition_point(m.events.begin(), m.events.end(), [newFrame](const InputEvent& ev) {
                    return ev.frame() <= newFrame;
                });
                m.events.erase(cutEvents, m.events.end());

                auto cutRows = std::partition_point(m.frames.begin(), m.frames.end(), [newFrame](const MacroFrame& row) {
                    return row.frame < newFrame;
                });
                m.frames.erase(cutRows, m.frames.end());

                auto cutSupers = std::partition_point(m.supers.begin(), m.supers.end(), [newFrame](const SuperFrame& sf) {
                    return sf.frame < newFrame;
                });
                m.supers.erase(cutSupers, m.supers.end());
            }
            m.totalFrames = newFrame;
        } else if (st.mode == Mode::Playing) {
            if (newFrame == 0) {
                st.playIndex = 0;
            } else {
                auto first = std::partition_point(m.events.begin(), m.events.end(), [newFrame](const InputEvent& ev) {
                    return ev.frame() <= newFrame;
                });
                st.playIndex = static_cast<size_t>(first - m.events.begin());
            }
        }

        st.frame = newFrame;
        rebuildHeld(newFrame);

        if (st.mode == Mode::Playing) indicatorReset(newFrame);
    }

    struct PendingButton {
        int button = 1;
        bool down = false;
        bool player2 = false;
    };

    std::vector<PendingButton> g_pendingSync;

    void flushPendingSync(GJBaseGameLayer* layer) {
        if (g_pendingSync.empty()) return;

        g_replayInject = true;
        for (const auto& pending : g_pendingSync) {
            layer->queueButton(pending.button, pending.down, pending.player2, 0.0);
        }
        g_replayInject = false;

        g_pendingSync.clear();
    }

    bool physicalHold(PlayLayer* pl, bool player2) {
        auto* ui = pl->m_uiLayer;
        if (!ui) return false;

        bool first = ui->m_p1Jumping || ui->m_p1TouchId != -1;
        bool second = ui->m_p2Jumping || ui->m_p2TouchId != -1;

        if (!pl->m_levelSettings || !pl->m_levelSettings->m_twoPlayerMode) {
            return !player2 && (first || second);
        }

        if (controlsFlipped()) std::swap(first, second);
        return player2 ? second : first;
    }

    bool isHolding(PlayerObject* player, int button) {
        auto it = player->m_holdingButtons.find(button);
        return it != player->m_holdingButtons.end() && it->second;
    }

    void syncHoldsAfterLoad(PlayLayer* pl, uint64_t frame) {
        auto& st = State::get();
        if (st.mode == Mode::Off || !pl || !pl->m_levelSettings || isPlatformer()) return;

        const bool twoPlayer = pl->m_levelSettings->m_twoPlayerMode;
        const int count = (twoPlayer && pl->m_gameState.m_isDualMode) ? 2 : 1;

        for (int i = 0; i < count; i++) {
            const bool isP2 = i == 1;
            PlayerObject* player = isP2 ? pl->m_player2 : pl->m_player1;
            if (!player) continue;

            const bool macroP2 = playerFlipped(isP2);
            const bool gameHold = isHolding(player, 1);
            const bool macroHold = st.held[macroP2 ? 1 : 0][1];

            if (st.mode == Mode::Recording) {
                if (macroHold != gameHold) addInput(frame + 1, macroP2, 1, gameHold);

                const bool physical = physicalHold(pl, isP2);
                if (physical != gameHold) g_pendingSync.push_back({1, physical, isP2});
            } else if (st.mode == Mode::Playing) {
                if (gameHold != macroHold) g_pendingSync.push_back({1, macroHold, isP2});
            }
        }
    }

    void pruneCheckpointData(PlayLayer* pl) {
        auto& frames = State::get().checkpointFrames;
        if (!pl || !pl->m_checkpointArray) return;

        const size_t live = static_cast<size_t>(pl->m_checkpointArray->count());
        if (frames.size() <= live + 32 && g_saved.size() <= live + 32) return;

        std::unordered_set<const void*> keep;
        for (auto* obj : CCArrayExt<CheckpointObject*>(pl->m_checkpointArray)) keep.insert(obj);

        std::erase_if(frames, [&keep](const auto& entry) { return !keep.contains(entry.first); });
        std::erase_if(g_saved, [&keep](const auto& entry) { return !keep.contains(entry.first); });
    }

    void injectReplayInputs(GJBaseGameLayer* layer) {
        auto& st = State::get();
        auto& events = st.current.events;

        if (ignoringInputs()) layer->m_queuedButtons.clear();

        flushPendingSync(layer);

        while (st.playIndex < events.size() && events[st.playIndex].frame() <= st.frame) {
            const auto& ev = events[st.playIndex];
            int button = ev.button();

            if (button >= 1 && button <= 3) {
                g_replayInject = true;
                layer->queueButton(button, ev.down(), playerFlipped(ev.player() == 2), 0.0);
                g_replayInject = false;
                st.held[ev.player() == 2 ? 1 : 0][button] = ev.down();
            }

            st.playIndex++;
        }
    }

    bool autosaveEnabled() {
        return NXRConfig::get().get<bool>("nxr.bot.autosave", true);
    }

    bool saveOnWinEnabled() {
        return autosaveEnabled() && NXRConfig::get().get<bool>("nxr.bot.autosave_win", true);
    }

    uint64_t autosaveInterval() {
        const int seconds = std::clamp(NXRConfig::get().get<int>("nxr.bot.autosave_seconds", 20), 5, 600);
        return static_cast<uint64_t>(seconds) * static_cast<uint64_t>(std::max(1.f, effectiveTps()));
    }

    void queueAutosave(const char* name) {
        auto& st = State::get();
        if (st.current.events.empty()) return;
        Macro snapshot = st.current;
        snapshot.name = name;
        NXR::Bot::saveMacroAsync(std::move(snapshot), NXR::Bot::macroPathFor(name));
    }

    void queueWinBackup() {
        auto& st = State::get();
        Macro snapshot = st.current;
        const std::string tag = NXR::Bot::backupNameNow("win");
        snapshot.name = tag;
        NXR::Bot::saveMacroAsync(std::move(snapshot), NXR::Bot::macroPathFor(tag));

        if (!st.selectedReplay.empty()) {
            Macro named = st.current;
            named.name = st.selectedReplay;
            NXR::Bot::saveMacroAsync(std::move(named), NXR::Bot::macroPathFor(st.selectedReplay));
        }

        NXR::Bot::pruneBackups(static_cast<size_t>(std::clamp(NXRConfig::get().get<int>("nxr.bot.autosave_keep", 10), 1, 100)));
        geode::queueInMainThread([] {
            geode::Notification::create("Replay auto-saved (level complete)", geode::NotificationIcon::Success)->show();
        });
    }

    bool playbackFinished() {
        auto& st = State::get();
        const uint64_t endFrame = st.current.endFrame();
        return st.playIndex >= st.current.events.size() && endFrame > 0 && st.frame > endFrame;
    }
}

namespace {
    void seedHeldInputs(GJBaseGameLayer* layer) {
        auto& st = NXR::Bot::State::get();
        const int count = layer->m_gameState.m_isDualMode ? 2 : 1;
        for (int i = 0; i < count; i++) {
            auto* player = i == 0 ? layer->m_player1 : layer->m_player2;
            if (!player) continue;
            for (int button = 1; button <= 3; button++) {
                if (isHolding(player, button)) addInput(st.frame, playerFlipped(i == 1), button, true);
            }
        }
    }
}

class $modify(NXRBotGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        NXR::Gui::get().getWindow("Bot");
        NXR::trySetPriority(self, "GJBaseGameLayer::processCommands", kProcessCommandsPriority);
    }

    void handleButton(bool down, int button, bool player1) {
        if (!g_inQueue && blockRealInput(this)) return;
        GJBaseGameLayer::handleButton(down, button, player1);
    }

    void processQueuedButtons(float dt, bool clearInputQueue) {
        if (!isActiveLayer(this) || State::get().mode == Mode::Off) {
            GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
            return;
        }

        auto& st = State::get();
        if (st.mode == Mode::Playing) injectReplayInputs(this);
        else if (st.mode == Mode::Recording) {
            flushPendingSync(this);
            recordQueued(this);
        }

        for (auto& cmd : m_queuedButtons) {
            cmd.m_step = 0;
            cmd.m_timestamp = 0.0;
        }

        g_inQueue = true;
        GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
        g_inQueue = false;
    }

    // One physics tick. Steps, in order:
    //   1. advance the frame counter (not on half-ticks, not while paused or finished)
    //   2. in playback, pre-apply the previous row so the tick starts from the right state
    //   3. run the game's tick
    //   4. probe once whether the tick counter is incremented inside the call
    //   5. record the new row (recording) or enforce it again (playback)
    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto* pl = PlayLayer::get();
        bool counted = false;
        const int progressBefore = this->m_gameState.m_currentProgress;
        if (pl && static_cast<GJBaseGameLayer*>(pl) == this) {
            auto& st = State::get();
            st.ticks++;

            if (st.pendingRestart && st.mode != Mode::Off && !pl->m_isPaused && !pl->m_levelEndAnimationStarted) {
                st.pendingRestart = false;
                if (pl->m_isPracticeMode && pl->m_checkpointArray && pl->m_checkpointArray->count() > 0) pl->fullReset();
                else pl->resetLevel();
                return;
            }

            if (st.mode != Mode::Off && !isHalfTick) {
                const bool active = !pl->m_isPaused && !pl->m_levelEndAnimationStarted;

                if (st.mode != g_anchorMode) {
                    g_anchorMode = st.mode;
                    g_needAnchor = true;
                    g_frameBias = 0;
                }

                if (active) {
                    const bool warped = std::abs(this->m_gameState.m_timeWarp - 1.f) > 1e-3f;
                    uint64_t next = (warped && !g_needAnchor) ? st.frame + 1 : anchoredFrame(this);

                    if (g_needAnchor) g_needAnchor = false;

                    st.frame = std::max(st.frame, next);
                    counted = true;

                    if (fixRandomEnabled()) GameToolbox::fast_srand(seedForFrame(st.frame));

                    if (st.mode == Mode::Playing && playbackFinished()) {
                        st.stop();
                    } else if (st.mode == Mode::Recording) {
                        if (st.frame < st.lastAutosaveFrame) st.lastAutosaveFrame = st.frame;

                        if (autosaveEnabled() && st.frame - st.lastAutosaveFrame >= autosaveInterval()) {
                            st.lastAutosaveFrame = st.frame;
                            queueAutosave("_autosave");
                        }
                    }
                }
            }
        }

        if (counted && State::get().mode == Mode::Playing) {
            const uint64_t frame = State::get().frame;
            if (frame >= 2) applyPlayback(this, frame - 1);
        }

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        if (counted && !g_progressProbed) {
            const int progressAfter = this->m_gameState.m_currentProgress;
            const bool liveTick = this->m_resumeTimer <= 0
                && std::abs(this->m_gameState.m_timeWarp - 1.f) <= 1e-3f
                && !(pl->m_player1 && pl->m_player1->m_isDead);

            if (liveTick) {
                if (progressAfter == progressBefore + 1) {
                    g_progressInside = true;
                    g_progressProbed = true;
                } else if (progressAfter == progressBefore && g_lastProgressAfter >= 0 && progressBefore == g_lastProgressAfter + 1) {
                    if (++g_outsideVotes >= 8) {
                        g_progressInside = false;
                        g_progressProbed = true;
                    }
                }
            }
            g_lastProgressAfter = progressAfter;
        }

        if (pl && static_cast<GJBaseGameLayer*>(pl) == this) {
            auto& st = State::get();
            const bool ending = pl->m_levelEndAnimationStarted;
            if (ending && !st.levelWasEnding && st.mode == Mode::Recording
                && !st.current.events.empty() && saveOnWinEnabled()) {
                queueWinBackup();
            }
            if (ending && !st.levelWasEnding && st.mode == Mode::Playing && st.rescues > 0) {
                const uint32_t n = st.rescues;
                const uint64_t first = st.firstRescueFrame;
                geode::queueInMainThread([n, first] {
                    geode::Notification::create(fmt::format("Desync Rescue: {} frame(s) fixed, first at frame {}", n, first), geode::NotificationIcon::Warning)->show();
                });
            }
            st.levelWasEnding = ending;
        }

        if (counted) {
            auto& st = State::get();
            if (st.mode == Mode::Recording) {
                if (st.pendingHere) {
                    st.pendingHere = false;
                    st.lastAutosaveFrame = st.frame;
                    stampHeader(pl);
                    seedHeldInputs(this);
                    recordRow(this, st.frame, true);
                } else {
                    recordRow(this, st.frame, false);
                }
            } else if (st.mode == Mode::Playing) {
                applyPlayback(this, st.frame, st.lastRescueFrame == st.frame && st.rescues > 0);
                indicatorUpdate();
            }
        }

        if (pl && static_cast<GJBaseGameLayer*>(pl) == this && State::get().mode != Mode::Playing) {
            const bool live = !isHalfTick && this->m_resumeTimer <= 0 && !pl->m_isPaused && !pl->m_levelEndAnimationStarted
                && !(pl->m_player1 && pl->m_player1->m_isDead);
            indicatorManualTick(live);
        }
    }
};

class $modify(NXRBotPlayLayer, PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (State::get().mode == Mode::Playing && !m_levelEndAnimationStarted) {
            if (!playbackDeathEnabled() && globalNoclipEnabled()) return;
            if (isDesyncDeath(this, object)) {
                noteRescue();
                return;
            }
        }
        PlayLayer::destroyPlayer(player, object);
    }

    void storeCheckpoint(CheckpointObject* obj) {
        PlayLayer::storeCheckpoint(obj);

        if (!obj) return;

        auto& st = State::get();
        st.checkpointFrames[obj] = st.frame;
        if (st.mode != Mode::Off && practiceFixEnabled()) g_saved[obj] = NXR::Practice::capturePair(this);

        pruneCheckpointData(this);
    }

    void loadFromCheckpoint(CheckpointObject* obj) {
        PlayLayer::loadFromCheckpoint(obj);

        if (State::get().mode != Mode::Off) m_extraDelta = 0.0;

        if (!obj || State::get().mode == Mode::Off || !practiceFixEnabled()) return;

        auto found = g_saved.find(obj);
        if (found != g_saved.end()) NXR::Practice::restorePair(this, found->second);
    }

    void fullReset() {
        State::get().checkpointFrames.clear();
        g_saved.clear();
        PlayLayer::fullReset();
    }

    // Runs on every death, restart and checkpoint respawn. Works out which macro frame the
    // attempt restarts from (0, or the frame stored with the last practice checkpoint) and
    // rewinds the recorder/player to it.
    void resetLevel() {
        auto& st = State::get();

        resetAttemptControls();
        g_pendingSync.clear();

        uint64_t newFrame = 0;
        bool hasCheckpoint = false;
        bool knownCheckpoint = true;

        if (m_isPracticeMode && m_checkpointArray && m_checkpointArray->count() > 0) {
            hasCheckpoint = true;
            auto found = st.checkpointFrames.find(m_checkpointArray->lastObject());
            if (found != st.checkpointFrames.end()) newFrame = found->second;
            else knownCheckpoint = false;
        }

        static bool s_scratch = false;
        if (hasCheckpoint && !knownCheckpoint && st.mode != Mode::Off && !s_scratch) {
            s_scratch = true;
            st.checkpointFrames.clear();
            g_saved.clear();
            this->removeAllCheckpoints();
            PlayLayer::fullReset();
            s_scratch = false;
            return;
        }

        int holdAtCheckpoint = -1;

        if (st.mode == Mode::Playing && newFrame == 0) st.resetRescues();

        if (st.mode != Mode::Off) {
            onReset(newFrame);
            if (st.mode == Mode::Recording && newFrame == 0) stampHeader(this);
            holdAtCheckpoint = heldMask();
        } else {
            st.resetRun();
        }

        PlayLayer::resetLevel();

        if (st.mode != Mode::Off) {
            m_extraDelta = 0.0;
            g_frameBias = static_cast<int64_t>(newFrame) - completedTicks(this);
            g_needAnchor = true;
            g_anchorMode = st.mode;
        }

        if (st.mode == Mode::Playing) {
            m_queuedButtons.clear();

            if (!hasCheckpoint) {
                if (m_player1) m_player1->releaseAllButtons();
                if (m_player2) m_player2->releaseAllButtons();
            }
        }

        if (hasCheckpoint) syncHoldsAfterLoad(this, newFrame);

        if (hasCheckpoint && newFrame > 0) {
            if (st.mode == Mode::Recording) recordRow(this, newFrame, true, holdAtCheckpoint);
            else if (st.mode == Mode::Playing) applyPlayback(this, newFrame, true);
        }

        syncControls();
    }

    void onQuit() {
        auto& st = State::get();
        if (st.mode == Mode::Recording && !st.current.events.empty()) {
            st.current.totalFrames = std::max(st.current.totalFrames, st.current.endFrame());
            if (autosaveEnabled()) {
                queueAutosave("_autosave");
                NXR::Bot::flushAsyncSaves();
            }
        }
        st.mode = Mode::Off;
        st.resetRun();
        st.checkpointFrames.clear();
        st.pendingRestart = false;
        g_saved.clear();
        g_pendingSync.clear();
        botSessionEnd();
        resetControls();

        PlayLayer::onQuit();

        syncControls();
    }
};

class $modify(NXRBotPlayerObject, PlayerObject) {
    static void onModify(auto& self) {
        NXR::trySetPriority(self, "PlayerObject::pushButton", kButtonPriority);
        NXR::trySetPriority(self, "PlayerObject::releaseButton", kButtonPriority);
    }

    bool pushButton(PlayerButton button) {
        bool ret = PlayerObject::pushButton(button);
        recordDirect(this, button, true);
        return ret;
    }

    bool releaseButton(PlayerButton button) {
        bool ret = PlayerObject::releaseButton(button);
        recordDirect(this, button, false);
        return ret;
    }
};

void NXR::Bot::botSessionBegin() {
    if (!cbfBypassEnabled() || g_cbfSuspended) return;

    auto* loader = geode::Loader::get();
    if (!loader) return;

    auto* mod = loader->getLoadedMod("syzzi.click_between_frames");
    if (!mod || !mod->hasSetting("soft-toggle")) return;

    g_cbfPrevious = mod->getSettingValue<bool>("soft-toggle");
    if (g_cbfPrevious) return;

    mod->setSettingValue("soft-toggle", true);
    g_cbfSuspended = true;
}

void NXR::Bot::botReanchor() {
    g_needAnchor = true;
    g_frameBias = 0;
    g_anchorMode = State::get().mode;
}

void NXR::Bot::botSessionEnd() {
    if (!g_cbfSuspended) return;
    g_cbfSuspended = false;

    auto* loader = geode::Loader::get();
    if (!loader) return;

    auto* mod = loader->getLoadedMod("syzzi.click_between_frames");
    if (!mod || !mod->hasSetting("soft-toggle")) return;

    mod->setSettingValue("soft-toggle", g_cbfPrevious);
}
