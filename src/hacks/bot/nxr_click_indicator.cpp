#include <Geode/Geode.hpp>
#include <algorithm>
#include <cmath>
#include <vector>
#include <map>
#include <string>
#include <string_view>
#include <filesystem>
#include "../../core/nxr_bot.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_utils.hpp"

using namespace geode::prelude;

namespace {
    using namespace NXR::Bot;

    constexpr const char* kEnabledKey = "nxr.bot.click_indicator";
    constexpr const char* kLineColorKey = "nxr.bot.click_indicator::line_color";
    constexpr const char* kBodyColorKey = "nxr.bot.click_indicator::body_color";
    constexpr const char* kBodyOpacityKey = "nxr.bot.click_indicator::body_opacity";
    constexpr const char* kPerfectKey = "nxr.bot.click_indicator::perfect";
    constexpr const char* kPerfectColorKey = "nxr.bot.click_indicator::perfect_color";
    constexpr const char* kFadeKey = "nxr.bot.click_indicator::fade_time";
    constexpr const char* kSlideKey = "nxr.bot.click_indicator::slide";
    constexpr const char* kEffectKey = "nxr.bot.click_indicator::effect";
    constexpr const char* kTrailColorKey = "nxr.bot.click_indicator::trail_color";
    constexpr const char* kTrailAheadKey = "nxr.bot.click_indicator::trail_ahead";
    constexpr const char* kTrailWidthKey = "nxr.bot.click_indicator::trail_width";
    constexpr const char* kHudKey = "nxr.bot.click_indicator::hud";
    constexpr const char* kHudSecondsKey = "nxr.bot.click_indicator::hud_seconds";
    constexpr const char* kHudXKey = "nxr.bot.click_indicator::hud_x";
    constexpr const char* kSoundKey = "nxr.bot.click_indicator::sound";
    constexpr const char* kSoundVolumeKey = "nxr.bot.click_indicator::sound_volume";
    constexpr const char* kP2ColorKey = "nxr.bot.click_indicator::p2_color";
    constexpr const char* kPlayerLineKey = "nxr.bot.click_indicator::player_line";
    constexpr const char* kHudScaleKey = "nxr.bot.click_indicator::hud_scale";
    constexpr const char* kHudInfoKey = "nxr.bot.click_indicator::hud_info";
    constexpr const char* kHideStatsKey = "nxr.bot.click_indicator::hide_stats";

    constexpr float kAheadSeconds = 3.75f;
    constexpr float kMinWidth = 4.f;
    constexpr float kLineWidth = 3.f;
    constexpr float kBottom = -1500.f;
    constexpr float kTop = 4500.f;
    constexpr int kTrailSegments = 120;
    constexpr int kMaxDrawMarks = 40;
    constexpr float kEndFadeDistance = 28.f;
    constexpr float kHudTargetY = 96.f;
    constexpr float kHudNoteSize = 8.f;
    constexpr float kHudBarHalf = 5.f;
    constexpr float kHudLaneGap = 30.f;
    // The path is sampled every few frames. Two neighbouring samples further apart than this
    // (in game units) are a teleport (portal, respawn) or stale data, never real movement,
    // so no segment is drawn between them.
    constexpr float kMaxSegmentJump = 160.f;

    enum Effect : int {
        EffectFadeout = 1,
        EffectTrail = 2,
    };

    struct Mark {
        uint32_t start = 0;
        uint32_t end = 0;
        uint8_t player = 1;
        float xs = 0.f;
        float xe = 0.f;
        // A player 2 click that repeats a player 1 click (same start and end). In a normal
        // dual level one tap presses both players, so only player 1's marker is drawn.
        bool mirrored = false;
    };

    struct Perfect {
        Ref<CCLabelBMFont> label;
        uint64_t born = 0;
        float x = 0.f;
        float y = 0.f;
        float dir = 1.f;
    };

    std::vector<Mark> g_marks;
    std::vector<Perfect> g_perfect;
    size_t g_first = 0;
    size_t g_perfectNext = 0;
    size_t g_builtCount = 0;
    bool g_built = false;
    // True when the macro has the dual flag on its rows (recorded by v1.4.5 or newer).
    bool g_hasDualInfo = false;
    uint64_t g_lastDraw = UINT64_MAX;
    Ref<CCNode> g_root;
    Ref<CCDrawNode> g_draw;
    Ref<CCDrawNode> g_hud;
    Ref<CCLabelBMFont> g_hudStats;
    std::string g_hudStatsText;
    Ref<CCLabelBMFont> g_hudInfo;
    std::string g_hudInfoText;
    Macro g_manual;
    std::string g_manualName;
    bool g_manualLoaded = false;
    bool g_manualOk = false;
    bool g_manualMono = true;
    uint64_t g_now = 0;
    uint64_t g_lastNow = 0;
    size_t g_soundNext = 0;

    const Macro& src() {
        auto& st = State::get();
        return st.mode == Mode::Playing ? st.current : g_manual;
    }

    float unitsPerSecond(float speed) {
        if (speed < 0.8f) return 251.16f;
        if (speed < 1.0f) return 311.58f;
        if (speed < 1.2f) return 387.42f;
        if (speed < 1.5f) return 468.0f;
        return 576.0f;
    }

    bool enabled() {
        return NXRConfig::get().get<bool>(kEnabledKey, false);
    }

    int effect() {
        return NXRConfig::get().get<int>(kEffectKey, EffectFadeout) == EffectTrail ? EffectTrail : EffectFadeout;
    }

    ccColor3B lineColor() {
        return NXR::Utils::hexToColor(NXRConfig::get().get<std::string>(kLineColorKey, "FFFFFF"));
    }

    ccColor3B bodyColor() {
        return NXR::Utils::hexToColor(NXRConfig::get().get<std::string>(kBodyColorKey, "00F0FF"));
    }

    ccColor3B perfectColor() {
        return NXR::Utils::hexToColor(NXRConfig::get().get<std::string>(kPerfectColorKey, "39FF6E"));
    }

    ccColor3B trailColor() {
        return NXR::Utils::hexToColor(NXRConfig::get().get<std::string>(kTrailColorKey, "39FF6E"));
    }

    ccColor3B p2Color() {
        return NXR::Utils::hexToColor(NXRConfig::get().get<std::string>(kP2ColorKey, "C84DFF"));
    }

    ccColor3B colorForPlayer(uint8_t player) {
        return player == 2 ? p2Color() : bodyColor();
    }

    bool playerLineEnabled() {
        return NXRConfig::get().get<bool>(kPlayerLineKey, true);
    }

    float hudScale() {
        return std::clamp(NXRConfig::get().get<float>(kHudScaleKey, 1.7f), 0.5f, 3.0f);
    }

    float bodyAlpha() {
        return static_cast<float>(std::clamp(NXRConfig::get().get<int>(kBodyOpacityKey, 70), 0, 255)) / 255.f;
    }

    bool showPerfect() {
        return NXRConfig::get().get<bool>(kPerfectKey, true);
    }

    float fadeSeconds() {
        return std::clamp(NXRConfig::get().get<float>(kFadeKey, 0.8f), 0.05f, 3.f);
    }

    float slideDistance() {
        return std::clamp(NXRConfig::get().get<float>(kSlideKey, 0.f), 0.f, 400.f);
    }

    bool hudEnabled() {
        return NXRConfig::get().get<bool>(kHudKey, true);
    }

    bool hudInfoEnabled() {
        return NXRConfig::get().get<bool>(kHudInfoKey, true);
    }

    bool hideStatsEnabled() {
        return NXRConfig::get().get<bool>(kHideStatsKey, false);
    }

    float hudSeconds() {
        return std::clamp(NXRConfig::get().get<float>(kHudSecondsKey, 1.2f), 0.3f, 4.f);
    }

    float hudX() {
        return std::clamp(NXRConfig::get().get<float>(kHudXKey, 70.f), 10.f, 300.f);
    }

    uint32_t trailAhead() {
        return static_cast<uint32_t>(std::clamp(NXRConfig::get().get<int>(kTrailAheadKey, 300), 30, 2400));
    }

    float trailWidth() {
        return std::clamp(NXRConfig::get().get<float>(kTrailWidthKey, 2.5f), 0.5f, 12.f);
    }

    double fadeFrames() {
        return std::max(1.0, static_cast<double>(fadeSeconds()) * static_cast<double>(std::max(1.f, effectiveTps())));
    }

    double aheadFrames() {
        return static_cast<double>(kAheadSeconds) * static_cast<double>(std::max(1.f, effectiveTps()));
    }

    ccColor4F premultiplied(const ccColor3B& c, float alpha) {
        const float a = std::clamp(alpha, 0.f, 1.f);
        return {c.r / 255.f * a, c.g / 255.f * a, c.b / 255.f * a, a};
    }

    void dropPerfect() {
        for (auto& p : g_perfect) {
            if (p.label && p.label->getParent()) p.label->removeFromParent();
            p.label = nullptr;
        }
        g_perfect.clear();
    }

    void dropHudLabels() {
        if (g_hudStats && g_hudStats->getParent()) g_hudStats->removeFromParent();
        g_hudStats = nullptr;
        g_hudStatsText.clear();
        if (g_hudInfo && g_hudInfo->getParent()) g_hudInfo->removeFromParent();
        g_hudInfo = nullptr;
        g_hudInfoText.clear();
    }

    void dropHud() {
        if (g_hud && g_hud->getParent()) g_hud->removeFromParent();
        g_hud = nullptr;
        dropHudLabels();
    }

    bool isStatsText(const char* text) {
        if (!text) return false;
        const std::string_view view(text);
        if (view.size() >= 3 && view.substr(0, 3) == "FPS") return true;
        return view.size() >= 3 && view.substr(view.size() - 3) == "CPS";
    }

    void hideStatsIn(CCNode* node) {
        if (!node) return;
        for (CCNode* child : node->getChildrenExt()) {
            auto* label = typeinfo_cast<CCLabelBMFont*>(child);
            if (label && label->isVisible() && isStatsText(label->getString())) label->setVisible(false);
        }
    }

    void hideStatsCounters(PlayLayer* pl) {
        auto* scene = CCDirector::sharedDirector()->getRunningScene();
        hideStatsIn(scene);
        hideStatsIn(pl);
        hideStatsIn(pl->m_uiLayer);

        if (!scene) return;
        for (CCNode* child : scene->getChildrenExt()) {
            if (child != static_cast<CCNode*>(pl)) hideStatsIn(child);
        }
    }

    void dropDraw() {
        if (g_draw && g_draw->getParent()) g_draw->removeFromParent();
        g_draw = nullptr;
        g_lastDraw = UINT64_MAX;
    }

    CCPoint pointAt(PlayLayer* pl, uint32_t frame, uint8_t player) {
        const auto& fixes = src().frames;
        auto* target = player == 2 ? pl->m_player2 : pl->m_player1;
        const CCPoint here = target ? target->getPosition() : CCPoint(0.f, 0.f);

        if (!fixes.empty()) {
            auto pickX = [player](const MacroFrame& fix) { return player == 2 ? fix.p2.x : fix.p1.x; };
            auto pickY = [player](const MacroFrame& fix) { return player == 2 ? fix.p2.y : fix.p1.y; };

            auto upper = std::partition_point(fixes.begin(), fixes.end(), [frame](const MacroFrame& fix) {
                return fix.frame <= frame;
            });

            if (upper == fixes.begin()) return {pickX(*upper), pickY(*upper)};
            if (upper == fixes.end()) return {pickX(*(upper - 1)), pickY(*(upper - 1))};

            const auto& a = *(upper - 1);
            const auto& b = *upper;
            const float span = static_cast<float>(b.frame - a.frame);
            if (span <= 0.f) return {pickX(a), pickY(a)};

            const float t = static_cast<float>(frame - a.frame) / span;
            return {pickX(a) + (pickX(b) - pickX(a)) * t, pickY(a) + (pickY(b) - pickY(a)) * t};
        }

        if (!target) return here;

        const float perFrame = unitsPerSecond(target->m_playerSpeed) / std::max(1.f, effectiveTps());
        return {here.x + (static_cast<float>(frame) - static_cast<float>(g_now)) * perFrame, here.y};
    }

    const MacroFrame* rowNear(uint32_t frame) {
        const auto& rows = src().frames;
        if (rows.empty()) return nullptr;

        auto upper = std::partition_point(rows.begin(), rows.end(), [frame](const MacroFrame& row) {
            return row.frame <= frame;
        });
        return upper == rows.begin() ? &*upper : &*(upper - 1);
    }

    // Was player 2 really in play at `frame`? In single mode the game keeps a hidden player 2
    // whose position is stale, and drawing its path is what produced the long stray line.
    bool dualAt(uint32_t frame) {
        const MacroFrame* row = rowNear(frame);
        if (!row) return false;

        if (g_hasDualInfo) return (row->p2.flags & NXR::Capture::kDualBit) != 0;

        // Older replays have no flag. Both players share the same x in a dual level.
        return row->p2.x != 0.f && std::fabs(row->p1.x - row->p2.x) < 1.f;
    }

    bool farApart(const CCPoint& a, const CCPoint& b) {
        return std::fabs(a.x - b.x) > kMaxSegmentJump || std::fabs(a.y - b.y) > kMaxSegmentJump;
    }

    void build(PlayLayer* pl) {
        g_marks.clear();
        g_first = 0;
        g_perfectNext = 0;
        g_soundNext = 0;

        std::map<int, size_t> pending;
        auto key = [](uint8_t player, uint8_t button) { return player * 8 + button; };

        for (const auto& ev : src().events) {
            const int k = key(ev.player(), ev.button());

            if (ev.down()) {
                auto found = pending.find(k);
                if (found != pending.end()) g_marks[found->second].end = static_cast<uint32_t>(ev.frame());

                Mark mark;
                mark.start = static_cast<uint32_t>(ev.frame());
                mark.end = mark.start + 1;
                mark.player = ev.player();
                g_marks.push_back(mark);
                pending[k] = g_marks.size() - 1;
            } else {
                auto found = pending.find(k);
                if (found == pending.end()) continue;
                g_marks[found->second].end = std::max<uint32_t>(g_marks[found->second].start + 1, static_cast<uint32_t>(ev.frame()));
                pending.erase(found);
            }
        }

        for (auto& mark : g_marks) {
            mark.xs = pointAt(pl, mark.start, mark.player).x;
            mark.xe = pointAt(pl, mark.end, mark.player).x;
            if (mark.xe < mark.xs) std::swap(mark.xs, mark.xe);
        }

        std::stable_sort(g_marks.begin(), g_marks.end(), [](const Mark& a, const Mark& b) { return a.start < b.start; });

        g_hasDualInfo = std::any_of(src().frames.begin(), src().frames.end(), [](const MacroFrame& row) {
            return (row.p2.flags & NXR::Capture::kDualBit) != 0;
        });

        // Marks are sorted by start, so a matching player 1 click is among the neighbours
        // whose start is within one frame.
        auto near = [](uint32_t a, uint32_t b) { return a > b ? a - b <= 1 : b - a <= 1; };
        for (size_t i = 0; i < g_marks.size(); i++) {
            auto& mark = g_marks[i];
            if (mark.player != 2) continue;

            auto repeatsP1 = [&](size_t j) {
                const auto& other = g_marks[j];
                return other.player == 1 && near(other.start, mark.start) && near(other.end, mark.end);
            };

            for (size_t j = i; j-- > 0 && g_marks[j].start + 1 >= mark.start;) {
                if (repeatsP1(j)) { mark.mirrored = true; break; }
            }
            for (size_t j = i + 1; !mark.mirrored && j < g_marks.size() && g_marks[j].start <= mark.start + 1; j++) {
                if (repeatsP1(j)) mark.mirrored = true;
            }
        }

        g_builtCount = src().events.size();
        g_built = true;
    }

    void ensureRoot(PlayLayer* pl) {
        if (!g_root || g_root->getParent() != pl->m_objectLayer) {
            dropPerfect();
            dropDraw();
            g_root = CCNode::create();
            pl->m_objectLayer->addChild(g_root, 5);
        }

        if (!g_draw || g_draw->getParent() != g_root.data()) {
            dropDraw();
            g_draw = CCDrawNode::create();
            g_root->addChild(g_draw, 2);
        }
    }

    float directionOf(PlayLayer* pl, uint8_t player) {
        auto* target = player == 2 ? pl->m_player2 : pl->m_player1;
        if (!target) return 1.f;
        return target->m_isGoingLeft ? -1.f : 1.f;
    }

    void advanceFirst(uint64_t now) {
        const double fade = fadeFrames();
        while (g_first < g_marks.size() && static_cast<double>(now) > static_cast<double>(g_marks[g_first].end) + fade) g_first++;
    }

    void drawBox(float x0, float x1, float lineX, float alpha, bool entryIsLeft, const ccColor3B& body) {
        const float width = std::max(1.f, x1 - x0);
        const ccColor3B line = lineColor();

        CCPoint verts[4] = {{x0, kBottom}, {x0 + width, kBottom}, {x0 + width, kTop}, {x0, kTop}};
        g_draw->drawPolygon(verts, 4, premultiplied(body, bodyAlpha() * alpha), 0.f, premultiplied(body, 0.f));

        const float farEdge = entryIsLeft ? x1 : x0;
        if (std::abs(farEdge - lineX) > 6.f) g_draw->drawSegment({farEdge, kBottom}, {farEdge, kTop}, kLineWidth * 0.4f, premultiplied(line, 0.4f * alpha));
        g_draw->drawSegment({lineX, kBottom}, {lineX, kTop}, kLineWidth * 0.5f, premultiplied(line, 0.9f * alpha));
    }

    uint64_t g_lastSound = 0;
    bool g_soundPlayed = false;
    uint64_t g_lastPerfect = 0;
    bool g_perfectSpawned = false;

    uint64_t minGapFrames(float perSecond) {
        return std::max<uint64_t>(1, static_cast<uint64_t>(std::max(1.f, effectiveTps()) / perSecond));
    }

    void spawnPerfect(PlayLayer* pl, const Mark& mark, uint64_t now) {
        if (!showPerfect() || !pl->m_objectLayer) return;

        auto* target = mark.player == 2 ? pl->m_player2 : pl->m_player1;
        Perfect perfect;
        perfect.label = CCLabelBMFont::create("Perfect", "bigFont.fnt");
        perfect.label->setScale(0.5f);
        perfect.label->setColor(perfectColor());
        perfect.label->setZOrder(1000);
        perfect.born = now;
        perfect.x = mark.xs;
        perfect.y = (target ? target->getPositionY() : 100.f) + 52.f;
        perfect.dir = directionOf(pl, mark.player);
        perfect.label->setPosition({perfect.x, perfect.y});
        pl->m_objectLayer->addChild(perfect.label);
        g_perfect.push_back(std::move(perfect));
    }

    void updatePerfect(PlayLayer* pl, uint64_t now) {
        const uint64_t perfectGap = minGapFrames(8.f);
        while (g_perfectNext < g_marks.size() && g_marks[g_perfectNext].start <= now) {
            const auto& mark = g_marks[g_perfectNext];
            if (!mark.mirrored && now - mark.start <= 4 && g_perfect.size() < 8
                && (!g_perfectSpawned || mark.start < g_lastPerfect || mark.start - g_lastPerfect >= perfectGap)) {
                spawnPerfect(pl, mark, now);
                g_lastPerfect = mark.start;
                g_perfectSpawned = true;
            }
            g_perfectNext++;
        }

        const double fade = fadeFrames();
        for (size_t i = 0; i < g_perfect.size();) {
            auto& p = g_perfect[i];
            const float t = static_cast<float>(std::clamp(static_cast<double>(now - p.born) / fade, 0.0, 1.0));
            const float eased = 1.f - (1.f - t) * (1.f - t);

            if (t >= 1.f || !p.label || !p.label->getParent()) {
                if (p.label && p.label->getParent()) p.label->removeFromParent();
                g_perfect.erase(g_perfect.begin() + static_cast<std::ptrdiff_t>(i));
                continue;
            }

            p.label->setPosition({p.x + p.dir * slideDistance() * eased, p.y + 18.f * eased});
            p.label->setOpacity(static_cast<GLubyte>(255.f * (1.f - t)));
            i++;
        }
    }

    void drawSquareMarker(const CCPoint& c, float half, const ccColor3B& color, float alpha) {
        const ccColor4F edge = premultiplied({255, 255, 255}, 0.95f * alpha);
        const CCPoint a = {c.x - half, c.y - half};
        const CCPoint b = {c.x + half, c.y - half};
        const CCPoint d = {c.x + half, c.y + half};
        const CCPoint e = {c.x - half, c.y + half};
        g_draw->drawSegment(a, b, 0.9f, edge);
        g_draw->drawSegment(b, d, 0.9f, edge);
        g_draw->drawSegment(d, e, 0.9f, edge);
        g_draw->drawSegment(e, a, 0.9f, edge);

        const float inner = half * 0.5f;
        CCPoint fill[4] = {{c.x - inner, c.y - inner}, {c.x + inner, c.y - inner}, {c.x + inner, c.y + inner}, {c.x - inner, c.y + inner}};
        g_draw->drawPolygon(fill, 4, premultiplied(color, 0.95f * alpha), 0.f, {0.f, 0.f, 0.f, 0.f});
    }

    void drawPlayerMarkers(PlayLayer* pl) {
        if (!playerLineEnabled()) return;

        const uint8_t players = pl->m_gameState.m_isDualMode ? 2 : 1;
        float lastX = -1.0e9f;

        for (uint8_t player = 1; player <= players; player++) {
            auto* target = player == 2 ? pl->m_player2 : pl->m_player1;
            if (!target) continue;

            const CCPoint pos = target->getPosition();
            if (std::abs(pos.x - lastX) > 0.5f) {
                g_draw->drawSegment({pos.x, kBottom}, {pos.x, kTop}, 0.9f, premultiplied({255, 255, 255}, 0.55f));
                lastX = pos.x;
            }

            const float half = 9.f;
            const ccColor4F edge = premultiplied({255, 255, 255}, 0.95f);
            const CCPoint a = {pos.x - half, pos.y - half};
            const CCPoint b = {pos.x + half, pos.y - half};
            const CCPoint d = {pos.x + half, pos.y + half};
            const CCPoint e = {pos.x - half, pos.y + half};
            g_draw->drawSegment(a, b, 1.1f, edge);
            g_draw->drawSegment(b, d, 1.1f, edge);
            g_draw->drawSegment(d, e, 1.1f, edge);
            g_draw->drawSegment(e, a, 1.1f, edge);
        }
    }

    void drawZones(PlayLayer* pl, uint64_t now) {
        const double fade = fadeFrames();
        const double holdFrames = 0.15 * static_cast<double>(std::max(1.f, effectiveTps()));

        advanceFirst(now);
        drawPlayerMarkers(pl);

        int drawn = 0;
        for (size_t i = g_first; i < g_marks.size() && static_cast<double>(g_marks[i].start) <= static_cast<double>(now) + aheadFrames() && drawn < kMaxDrawMarks; i++) {
            const auto& mark = g_marks[i];
            if (static_cast<double>(now) > static_cast<double>(mark.end) + fade) continue;
            if (mark.player == 2 && (mark.mirrored || !dualAt(mark.start))) continue;
            drawn++;

            auto* target = mark.player == 2 ? pl->m_player2 : pl->m_player1;
            const float dir = directionOf(pl, mark.player);
            const float lo = mark.xs;
            const float hi = std::max(mark.xe, mark.xs + kMinWidth);

            float x0 = lo;
            float x1 = hi;
            float lineX = dir >= 0.f ? lo : hi;
            float alpha = 1.f;

            if (now >= mark.start && target) {
                const float px = target->getPositionX();
                const float follow = std::clamp(px, lo, hi);

                if (dir >= 0.f) {
                    x0 = follow;
                    alpha = 1.f - std::clamp((px - hi) / kEndFadeDistance, 0.f, 1.f);
                } else {
                    x1 = follow;
                    alpha = 1.f - std::clamp((lo - px) / kEndFadeDistance, 0.f, 1.f);
                }

                lineX = follow;
                if (alpha <= 0.f) continue;
            }

            const ccColor3B playerColor = colorForPlayer(mark.player);
            drawBox(x0, x1, lineX, alpha, dir >= 0.f, playerColor);

            float markY = pointAt(pl, mark.start, mark.player).y;
            if (now >= mark.start && target) markY = target->getPositionY();
            drawSquareMarker({lineX, markY}, 6.5f, playerColor, alpha);

            if (static_cast<double>(mark.end - mark.start) > holdFrames && now < mark.end) {
                const float farX = dir >= 0.f ? x1 : x0;
                const CCPoint releasePoint = pointAt(pl, mark.end, mark.player);
                g_draw->drawSegment({lineX, markY}, {farX, releasePoint.y}, 0.8f, premultiplied(playerColor, 0.55f * alpha));
                drawSquareMarker({farX, releasePoint.y}, 4.f, playerColor, 0.8f * alpha);
            }
        }

        updatePerfect(pl, now);
    }

    void ensureHud(PlayLayer* pl) {
        if (!pl->m_uiLayer) return;

        if (!g_hud || g_hud->getParent() != pl->m_uiLayer) {
            dropHud();
            g_hud = CCDrawNode::create();
            pl->m_uiLayer->addChild(g_hud, 900);
        }
    }

    struct LaneGeo {
        float cx = 0.f;
        float targetY = 0.f;
        float height = 0.f;
        float halfWidth = 0.f;
        float scale = 1.f;

        float yAt(float t) const { return targetY + t * height; }
    };

    void hudRect(float x0, float y0, float x1, float y1, const ccColor4F& color) {
        CCPoint pts[4] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
        g_hud->drawPolygon(pts, 4, color, 0.f, {0.f, 0.f, 0.f, 0.f});
    }

    void hudChevron(const CCPoint& tip, float size, float dirX, float width, const ccColor4F& color) {
        g_hud->drawSegment({tip.x - dirX * size, tip.y + size}, tip, width, color);
        g_hud->drawSegment({tip.x - dirX * size, tip.y - size}, tip, width, color);
    }

    void ensureHudLabel(PlayLayer* pl, const LaneGeo& g) {
        if (!pl->m_uiLayer) return;

        if (!g_hudStats || g_hudStats->getParent() != pl->m_uiLayer) {
            g_hudStats = CCLabelBMFont::create("0 / 0", "bigFont.fnt");
            g_hudStats->setScale(0.2f);
            pl->m_uiLayer->addChild(g_hudStats, 901);
            g_hudStatsText.clear();
        }

        g_hudStats->setPosition({g.cx, g.targetY - 16.f * g.scale});

        if (!g_hudInfo || g_hudInfo->getParent() != pl->m_uiLayer) {
            g_hudInfo = CCLabelBMFont::create("", "bigFont.fnt");
            g_hudInfo->setScale(0.2f);
            g_hudInfo->setAnchorPoint({0.5f, 1.f});
            pl->m_uiLayer->addChild(g_hudInfo, 901);
            g_hudInfoText.clear();
        }

        g_hudInfo->setPosition({g.cx, g.targetY - 24.f * g.scale});
    }

    void drawHud(PlayLayer* pl, uint64_t now) {
        if (!g_hud) return;
        g_hud->clear();

        const auto win = CCDirector::sharedDirector()->getWinSize();
        const float tps = std::max(1.f, effectiveTps());
        const double window = std::max(1.0, static_cast<double>(hudSeconds()) * tps);
        const double holdFrames = 0.15 * tps;
        const double hitFade = 0.18 * tps;
        const double pulseFrames = 0.25 * tps;
        const bool dual = pl->m_gameState.m_isDualMode;

        LaneGeo g;
        g.scale = hudScale();
        g.cx = hudX();
        g.targetY = kHudTargetY;
        g.height = std::clamp(win.height - 56.f - g.targetY, 70.f, 240.f);
        g.halfWidth = 15.f * g.scale;

        const float left = g.cx - g.halfWidth;
        const float right = g.cx + g.halfWidth;
        const float top = g.yAt(1.f);
        const ccColor3B white = {255, 255, 255};

        const ccColor3B rungColor = {200, 200, 200};
        const ccColor3B targetRed = {232, 56, 48};
        const ccColor3B chevronGreen = {57, 255, 110};

        hudRect(left, g.targetY - 3.f * g.scale, right, top, {0.f, 0.f, 0.f, 0.38f});
        const float rail = 1.5f * g.scale;
        g_hud->drawSegment({left, g.targetY - 3.f * g.scale}, {left, top}, rail, premultiplied(white, 0.7f));
        g_hud->drawSegment({right, g.targetY - 3.f * g.scale}, {right, top}, rail, premultiplied(white, 0.7f));
        g_hud->drawSegment({left, top}, {right, top}, rail, premultiplied(white, 0.5f));
        for (int i = 1; i <= 19; i++) {
            const float y = g.yAt(static_cast<float>(i) / 20.f);
            const bool major = i % 5 == 0;
            g_hud->drawSegment({left, y}, {right, y}, (major ? 1.5f : 1.0f) * g.scale, premultiplied(rungColor, major ? 0.55f : 0.32f));
        }

        double lastHitDt = 1.0e9;
        uint8_t lastPlayer = 1;
        for (size_t i = g_first; i < g_marks.size() && g_marks[i].start <= now; i++) {
            const double dt = static_cast<double>(now) - static_cast<double>(g_marks[i].start);
            if (dt < lastHitDt) {
                lastHitDt = dt;
                lastPlayer = g_marks[i].player;
            }
        }

        float pulse = 0.f;
        if (lastHitDt < pulseFrames) pulse = std::sin(3.14159265f * static_cast<float>(lastHitDt / pulseFrames));

        struct LaneNote {
            double dtStart = 0.0;
            double dtEnd = 0.0;
            bool hold = false;
            int count = 1;
        };

        const float minGap = 6.f * g.scale;
        const float halfBar = 1.9f * g.scale;
        std::vector<LaneNote> notes;

        const uint8_t players = dual ? 2 : 1;
        for (uint8_t player = 1; player <= players; player++) {
            const ccColor3B noteColor = (dual && player == 2) ? p2Color() : white;
            float x0 = left + 2.f * g.scale;
            float x1 = right - 2.f * g.scale;
            if (dual) {
                if (player == 1) x1 = g.cx - 1.f * g.scale;
                else x0 = g.cx + 1.f * g.scale;
            }

            notes.clear();
            for (size_t i = g_first; i < g_marks.size() && static_cast<double>(g_marks[i].start) <= static_cast<double>(now) + window; i++) {
                const auto& mark = g_marks[i];
                if (mark.player != player) continue;

                LaneNote note;
                note.dtStart = static_cast<double>(mark.start) - static_cast<double>(now);
                note.dtEnd = static_cast<double>(mark.end) - static_cast<double>(now);
                note.hold = static_cast<double>(mark.end - mark.start) > holdFrames;
                if (note.dtEnd < -hitFade) continue;

                if (!notes.empty()) {
                    auto& last = notes.back();
                    const double gapPoints = (note.dtStart - last.dtEnd) / window * static_cast<double>(g.height);
                    if (gapPoints < static_cast<double>(minGap)) {
                        last.dtEnd = std::max(last.dtEnd, note.dtEnd);
                        last.hold = last.hold || note.hold;
                        last.count++;
                        continue;
                    }
                }
                notes.push_back(note);
            }

            for (const auto& note : notes) {
                const float tStart = static_cast<float>(std::clamp(note.dtStart / window, 0.0, 1.0));
                const float tEnd = static_cast<float>(std::clamp(note.dtEnd / window, 0.0, 1.0));
                const float yStart = g.yAt(tStart);
                const float yEnd = g.yAt(tEnd);
                const bool band = note.hold || note.count > 1;

                if (note.dtStart > 0.0) {
                    if (band) hudRect(x0, yStart, x1, yEnd, premultiplied(noteColor, 0.3f));
                    hudRect(x0, yStart - halfBar, x1, yStart + halfBar, premultiplied(noteColor, 0.95f));
                    if (band && yEnd - yStart > 2.f) hudRect(x0, yEnd - halfBar * 0.7f, x1, yEnd + halfBar * 0.7f, premultiplied(noteColor, 0.7f));
                } else if (note.dtEnd > 0.0) {
                    if (band) hudRect(x0, g.targetY, x1, yEnd, premultiplied(noteColor, 0.5f));
                    hudRect(x0, g.targetY - halfBar * 1.5f, x1, g.targetY + halfBar * 1.5f, premultiplied(white, 1.f));
                    if (band && yEnd - g.targetY > 2.f) hudRect(x0, yEnd - halfBar * 0.7f, x1, yEnd + halfBar * 0.7f, premultiplied(noteColor, 0.8f));
                } else if (-note.dtEnd < hitFade) {
                    const float a = 1.f - static_cast<float>(-note.dtEnd / hitFade);
                    hudRect(x0, g.targetY - halfBar * 1.5f, x1, g.targetY + halfBar * 1.5f, premultiplied(white, a));
                }
            }
        }

        const ccColor3B pulseColor = (dual && lastPlayer == 2) ? p2Color() : white;
        const ccColor4F lineColor = pulse > 0.f ? premultiplied(pulseColor, 1.f) : premultiplied(targetRed, 1.f);
        const float size = (6.4f + 2.2f * pulse) * g.scale;
        const float chevronWidth = (2.3f + 0.8f * pulse) * g.scale;
        const float reach = 6.f * g.scale;

        hudRect(left, g.targetY - 2.2f * g.scale, right, g.targetY + 2.2f * g.scale, lineColor);
        hudChevron({left - reach, g.targetY}, size, 1.f, chevronWidth, premultiplied(chevronGreen, 1.f));
        hudChevron({right + reach, g.targetY}, size, -1.f, chevronWidth, premultiplied(chevronGreen, 1.f));

        if (hudInfoEnabled()) ensureHudLabel(pl, g);
        else dropHudLabels();

        if (g_hudStats) {
            auto passedIt = std::partition_point(g_marks.begin(), g_marks.end(), [now](const Mark& mark) {
                return static_cast<uint64_t>(mark.start) <= now;
            });
            const size_t passed = static_cast<size_t>(passedIt - g_marks.begin());
            const std::string text = fmt::format("{} / {}", passed, g_marks.size());
            if (text != g_hudStatsText) {
                g_hudStatsText = text;
                g_hudStats->setString(text.c_str());
            }
            g_hudStats->setColor({200, 200, 200});
            g_hudStats->setOpacity(170);
        }

        if (g_hudInfo) {
            const auto& macro = src();
            const std::string info = fmt::format(
                "Frame {} / {}\nFrames {}\nSuper {}",
                now, macro.endFrame(), macro.frames.size(), macro.supers.size()
            );
            if (info != g_hudInfoText) {
                g_hudInfoText = info;
                g_hudInfo->setString(info.c_str());
            }
            g_hudInfo->setColor({200, 200, 200});
            g_hudInfo->setOpacity(170);
        }
    }

    bool heldAt(uint32_t frame, uint8_t player, size_t from) {
        for (size_t i = from; i < g_marks.size() && g_marks[i].start <= frame; i++) {
            const auto& mark = g_marks[i];
            if (mark.player == player && frame >= mark.start && frame <= mark.end) return true;
        }
        return false;
    }

    // Draws where each player will go over the next `trail_ahead` frames, plus a marker at
    // every click. Player 1 uses the trail color (green), player 2 the P2 color (purple).
    void drawTrail(PlayLayer* pl, uint64_t now64) {
        const uint32_t now = static_cast<uint32_t>(now64);
        const uint32_t ahead = trailAhead();
        const uint32_t stride = std::max<uint32_t>(1u, ahead / kTrailSegments);
        const float width = trailWidth();

        const ccColor3B pathColor[2] = {trailColor(), p2Color()};
        const ccColor3B bodyColorOf[2] = {bodyColor(), p2Color()};
        const ccColor4F dotColor = premultiplied({255, 255, 255}, 0.95f);

        size_t first = g_first;
        while (first < g_marks.size() && g_marks[first].end < now) first++;

        for (uint8_t player = 1; player <= 2; player++) {
            const int slot = player - 1;
            const ccColor4F baseColor = premultiplied(pathColor[slot], 0.7f);
            const ccColor4F holdColor = premultiplied(bodyColorOf[slot], 0.95f);

            CCPoint prev = pointAt(pl, now, player);
            bool prevValid = player == 1 || dualAt(now);

            for (uint32_t frame = now + stride; frame <= now + ahead; frame += stride) {
                const bool valid = player == 1 || dualAt(frame);
                const CCPoint cur = valid ? pointAt(pl, frame, player) : prev;

                if (valid && prevValid && !farApart(prev, cur)) {
                    const bool held = heldAt(frame, player, first);
                    g_draw->drawSegment(prev, cur, held ? width * 1.6f : width * 0.6f, held ? holdColor : baseColor);
                }

                prev = cur;
                prevValid = valid;
            }
        }

        int drawnMarks = 0;
        for (size_t i = first; i < g_marks.size() && g_marks[i].start <= now + ahead && drawnMarks < kMaxDrawMarks; i++) {
            const auto& mark = g_marks[i];

            // Player 2 only gets its own marker when it clicked on its own (split input);
            // a repeated tap already has player 1's marker, and a P2 click outside dual mode
            // belongs to the hidden player.
            if (mark.player == 2 && (mark.mirrored || !dualAt(mark.start))) continue;
            drawnMarks++;

            const int slot = mark.player - 1;
            const ccColor4F holdColor = premultiplied(bodyColorOf[slot], 0.95f);
            const ccColor4F releaseColor = premultiplied(pathColor[slot], 0.9f);

            if (mark.start >= now) {
                const CCPoint p = pointAt(pl, mark.start, mark.player);
                g_draw->drawDot(p, width * 2.8f, dotColor);
                g_draw->drawDot(p, width * 1.8f, holdColor);
            }

            if (mark.end > now && mark.end <= now + ahead) {
                const CCPoint p = pointAt(pl, mark.end, mark.player);
                g_draw->drawDot(p, width * 1.6f, releaseColor);
            }
        }
    }

    bool soundEnabled() {
        return NXRConfig::get().get<bool>(kSoundKey, true);
    }

    float soundVolume() {
        return static_cast<float>(std::clamp(NXRConfig::get().get<int>(kSoundVolumeKey, 80), 0, 100)) / 100.f;
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

    void playClick() {
        auto* engine = FMODAudioEngine::get();
        if (!engine) return;
        engine->playEffect(clickSoundPath(), 1.f, 0.f, soundVolume());
    }

    void updateSound(uint64_t now) {
        const bool on = soundEnabled();
        const uint64_t gap = minGapFrames(18.f);
        while (g_soundNext < g_marks.size() && g_marks[g_soundNext].start <= now) {
            const uint32_t start = g_marks[g_soundNext].start;
            if (on && now - start <= 6 && (!g_soundPlayed || start < g_lastSound || start - g_lastSound >= gap)) {
                playClick();
                g_lastSound = start;
                g_soundPlayed = true;
            }
            g_soundNext++;
        }
    }

    void resync(uint64_t frame) {
        dropPerfect();
        if (g_draw) g_draw->clear();
        if (g_hud) g_hud->clear();
        g_lastDraw = UINT64_MAX;

        g_first = 0;
        const double fade = fadeFrames();
        while (g_first < g_marks.size() && static_cast<double>(frame) > static_cast<double>(g_marks[g_first].start) + fade) g_first++;

        g_perfectNext = 0;
        while (g_perfectNext < g_marks.size() && g_marks[g_perfectNext].start < frame) g_perfectNext++;

        g_soundNext = 0;
        while (g_soundNext < g_marks.size() && g_marks[g_soundNext].start < frame) g_soundNext++;

        g_soundPlayed = false;
        g_perfectSpawned = false;
    }

    void run(PlayLayer* pl, uint64_t now) {
        g_now = now;

        if (!g_built || g_builtCount != src().events.size()) build(pl);
        ensureRoot(pl);

        if (now + 30 < g_lastNow) resync(now);
        g_lastNow = now;

        updateSound(now);

        if (hideStatsEnabled()) {
            static uint32_t s_statsTick = 0;
            if (s_statsTick++ % 12 == 0) hideStatsCounters(pl);
        }

        const uint64_t period = std::max<uint64_t>(1, static_cast<uint64_t>(std::lround(std::max(1.f, effectiveTps()) / 60.f)));
        if (g_lastDraw != UINT64_MAX && now >= g_lastDraw && now - g_lastDraw < period) return;
        g_lastDraw = now;

        g_draw->clear();

        const int mode = effect();

        if (mode == EffectTrail) {
            advanceFirst(now);
            drawTrail(pl, now);
            updatePerfect(pl, now);
        } else {
            drawZones(pl, now);
        }

        if (hudEnabled()) {
            ensureHud(pl);
            drawHud(pl, now);
        } else {
            dropHud();
        }
    }

    void loadManual() {
        auto& st = State::get();
        const bool sameName = g_manualLoaded && g_manualName == st.selectedReplay;
        const bool memoryNewer = st.current.name == st.selectedReplay && !st.current.events.empty()
            && st.current.events.size() != g_manual.events.size();
        if (sameName && !memoryNewer) return;

        g_manualName = st.selectedReplay;
        g_manualLoaded = true;
        g_manualOk = false;
        g_manual.clear();
        g_built = false;

        Macro macro;
        if (st.current.name == g_manualName && !st.current.events.empty()) macro = st.current;
        else if (!loadMacro(macro, macroPathFor(g_manualName))) return;

        size_t drops = 0;
        for (size_t i = 1; i < macro.frames.size(); i++) {
            if (macro.frames[i].p1.x + 1.f < macro.frames[i - 1].p1.x) drops++;
        }
        g_manualMono = drops * 100 <= macro.frames.size();

        g_manual = std::move(macro);
        g_manualOk = !g_manual.events.empty();
    }

    uint64_t manualFrame(PlayLayer* pl) {
        const auto& fixes = g_manual.frames;
        auto* player = pl->m_player1;

        if (!fixes.empty() && g_manualMono && player) {
            const float x = player->getPositionX();
            auto upper = std::partition_point(fixes.begin(), fixes.end(), [x](const MacroFrame& fix) { return fix.p1.x <= x; });

            if (upper == fixes.begin()) return 0;
            if (upper == fixes.end()) return fixes.back().frame;

            const auto& a = *(upper - 1);
            const auto& b = *upper;
            const float span = b.p1.x - a.p1.x;
            if (span <= 0.001f) return a.frame;

            const float t = std::clamp((x - a.p1.x) / span, 0.f, 1.f);
            return static_cast<uint64_t>(std::llround(static_cast<double>(a.frame) + static_cast<double>(t) * static_cast<double>(b.frame - a.frame)));
        }

        const float tps = g_manual.tps > 0.f ? g_manual.tps : std::max(1.f, effectiveTps());
        const double time = std::max(0.0, static_cast<double>(pl->m_gameState.m_levelTime));
        return static_cast<uint64_t>(std::llround(time * static_cast<double>(tps)));
    }
}

void NXR::Bot::indicatorClear() {
    dropPerfect();
    dropDraw();
    dropHud();
    if (g_root && g_root->getParent()) g_root->removeFromParent();
    g_root = nullptr;
    g_marks.clear();
    g_first = 0;
    g_perfectNext = 0;
    g_soundNext = 0;
    g_builtCount = 0;
    g_lastNow = 0;
    g_built = false;
}

void NXR::Bot::indicatorReset(uint64_t frame) {
    resync(frame);
}

void NXR::Bot::indicatorUpdate() {
    auto& st = State::get();
    auto* pl = PlayLayer::get();

    if (!pl || !pl->m_objectLayer || st.mode != Mode::Playing || !enabled()) {
        if (g_root || g_built) indicatorClear();
        return;
    }

    run(pl, st.frame);
}

void NXR::Bot::indicatorManualTick(bool) {
    auto& st = State::get();
    if (st.mode == Mode::Playing) return;

    auto* pl = PlayLayer::get();
    const bool wanted = pl && pl->m_objectLayer && st.mode == Mode::Off && enabled() && !st.selectedReplay.empty();

    if (!wanted) {
        if (g_root || g_built) indicatorClear();
        return;
    }

    loadManual();
    if (!g_manualOk) {
        if (g_root || g_built) indicatorClear();
        return;
    }

    run(pl, manualFrame(pl));
}
