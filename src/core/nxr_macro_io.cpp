#include "nxr_bot.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <json.hpp>

using namespace NXR::Bot;

namespace {
    constexpr char kMagicV10[4] = {'N', 'X', 'R', 'A'};
    constexpr char kMagicV9[4] = {'N', 'X', 'R', '9'};
    constexpr char kMagicV8[4] = {'N', 'X', 'R', '8'};
    constexpr char kMagicV7[4] = {'N', 'X', 'R', '7'};
    constexpr uint32_t kMaxText = 4096;
    constexpr uint32_t kMaxBlob = 1u << 22;
    constexpr uint64_t kMaxItems = 50000000ULL;

    template <typename T>
    void put(std::ofstream& out, const T& v) { out.write(reinterpret_cast<const char*>(&v), sizeof(T)); }

    template <typename T>
    bool get(std::ifstream& in, T& v) { in.read(reinterpret_cast<char*>(&v), sizeof(T)); return static_cast<bool>(in); }

    void putText(std::ofstream& out, const std::string& s) {
        const uint32_t n = static_cast<uint32_t>(std::min<size_t>(s.size(), kMaxText));
        put(out, n);
        out.write(s.data(), n);
    }

    bool getText(std::ifstream& in, std::string& s) {
        uint32_t n = 0;
        if (!get(in, n) || n > kMaxText) return false;
        s.assign(n, '\0');
        if (n) in.read(s.data(), n);
        return static_cast<bool>(in);
    }

    void putState(std::ofstream& out, const NXR::Capture::PlayerState& s) {
        put(out, s.x);
        put(out, s.y);
        put(out, s.rot);
        put(out, s.yVel);
        put(out, s.xVel);
        put(out, s.flags);
    }

    bool getState(std::ifstream& in, NXR::Capture::PlayerState& s) {
        return get(in, s.x) && get(in, s.y) && get(in, s.rot) && get(in, s.yVel) && get(in, s.xVel) && get(in, s.flags);
    }

    void putBlobData(std::ofstream& out, const std::vector<uint8_t>& blob) {
        const uint32_t n = static_cast<uint32_t>(blob.size());
        put(out, n);
        if (n) out.write(reinterpret_cast<const char*>(blob.data()), n);
    }

    bool getBlobData(std::ifstream& in, std::vector<uint8_t>& blob) {
        uint32_t n = 0;
        if (!get(in, n) || n > kMaxBlob) return false;
        blob.assign(n, 0);
        if (n) in.read(reinterpret_cast<char*>(blob.data()), n);
        return static_cast<bool>(in);
    }

    bool loadV7(std::ifstream& in, Macro& macro) {
        Macro r;
        if (!getText(in, r.version)) return false;
        if (!getText(in, r.levelName)) return false;
        if (!get(in, r.levelId)) return false;
        if (!get(in, r.tps)) return false;
        if (!get(in, r.fps)) return false;
        if (!getText(in, r.name)) return false;
        if (!get(in, r.totalFrames)) return false;
        if (!(r.tps >= 1.f && r.tps <= 5000000.f)) return false;

        uint8_t flags = 0;
        if (!get(in, flags)) return false;
        const bool full = (flags & 3) != 0;
        const bool hasXVel = (flags & 4) != 0;

        uint64_t count = 0;
        if (!get(in, count) || count > kMaxItems) return false;
        r.events.reserve(static_cast<size_t>(count));

        for (uint64_t i = 0; i < count; i++) {
            uint32_t f1, f2, frame;
            uint8_t p1, p2, hold, jump, button, down;
            if (!get(in, f1) || !get(in, f2) || !get(in, frame)) return false;
            if (!get(in, p1) || !get(in, p2) || !get(in, hold) || !get(in, jump) || !get(in, button) || !get(in, down)) return false;

            if (frame > InputEvent::kMaxFrame) return false;
            if (button < 1 || button > 3) return false;
            if ((p1 != 0) == (p2 != 0)) return false;

            r.events.push_back(InputEvent::make(frame, p2 ? 2 : 1, button, down != 0));
        }

        uint64_t fixCount = 0;
        if (!get(in, fixCount) || fixCount > kMaxItems) return false;
        r.frames.reserve(static_cast<size_t>(fixCount));

        for (uint64_t i = 0; i < fixCount; i++) {
            MacroFrame row;
            float p1x, p1y, p1r, p2x, p2y, p2r, p1v, p2v, p1xv, p2xv;
            uint32_t p1f, p2f;
            if (!get(in, row.frame)) return false;
            if (!get(in, p1x) || !get(in, p1y) || !get(in, p1r)) return false;
            if (!get(in, p2x) || !get(in, p2y) || !get(in, p2r)) return false;
            if (!get(in, p1v) || !get(in, p2v)) return false;
            if (!get(in, p1f) || !get(in, p2f)) return false;
            if (!get(in, p1xv) || !get(in, p2xv)) return false;

            const float unknown = std::numeric_limits<float>::quiet_NaN();
            row.p1 = {p1x, p1y, p1r, p1v, hasXVel ? p1xv : unknown, p1f};
            row.p2 = {p2x, p2y, p2r, p2v, hasXVel ? p2xv : unknown, p2f};
            row.full = full;
            r.frames.push_back(row);
        }

        char trailing = 0;
        if (in.read(&trailing, 1)) return false;

        r.finalize(true);
        macro = std::move(r);
        return true;
    }
}

bool NXR::Bot::saveMacro(const Macro& macro, const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    const auto tempPath = path.string() + ".tmp";
    std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;

    out.write(kMagicV10, 4);
    putText(out, macro.version);
    putText(out, macro.levelName);
    put(out, macro.levelId);
    put(out, macro.tps);
    put(out, macro.fps);
    putText(out, macro.name);
    put(out, macro.totalFrames);
    put(out, macro.layout);

    const uint64_t eventCount = macro.events.size();
    put(out, eventCount);
    for (const auto& ev : macro.events) put(out, ev.packed);

    const uint64_t frameCount = macro.frames.size();
    put(out, frameCount);
    for (const auto& row : macro.frames) {
        put(out, row.frame);
        putState(out, row.p1);
        putState(out, row.p2);
        put(out, row.hold);
        put(out, static_cast<uint8_t>(row.full ? 1 : 0));
    }

    const uint64_t superCount = macro.supers.size();
    put(out, superCount);
    for (const auto& sf : macro.supers) {
        put(out, sf.frame);
        putBlobData(out, sf.p1);
        putBlobData(out, sf.p2);
    }

    NXR::Stats::LevelStats stats = macro.stats;
    bool hasStats = macro.hasStats;
    if (!hasStats && macro.levelId != 0) hasStats = NXR::Stats::cachedFor(macro.levelId, stats);
    put(out, static_cast<uint8_t>(hasStats ? 1 : 0));
    if (hasStats) {
        for (uint32_t value : stats.toArray()) put(out, value);
    }

    put(out, static_cast<uint8_t>(macro.noclip ? 1 : 0));

    out.flush();
    if (!out.good()) {
        out.close();
        std::filesystem::remove(tempPath, ec);
        return false;
    }
    out.close();

    std::filesystem::rename(tempPath, path, ec);
    if (ec) {
        ec.clear();
        std::filesystem::remove(path, ec);
        ec.clear();
        std::filesystem::rename(tempPath, path, ec);
        if (ec) {
            std::filesystem::remove(tempPath, ec);
            return false;
        }
    }
    return true;
}

bool NXR::Bot::loadMacro(Macro& macro, const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;

    char magic[4];
    in.read(magic, 4);
    if (!in) return false;

    if (std::memcmp(magic, kMagicV7, 4) == 0) return loadV7(in, macro);

    const bool v10 = std::memcmp(magic, kMagicV10, 4) == 0;
    const bool v9 = v10 || std::memcmp(magic, kMagicV9, 4) == 0;
    if (!v9 && std::memcmp(magic, kMagicV8, 4) != 0) {
        in.close();
        return loadMacroLegacy(macro, path);
    }

    Macro r;
    if (!getText(in, r.version)) return false;
    if (!getText(in, r.levelName)) return false;
    if (!get(in, r.levelId)) return false;
    if (!get(in, r.tps)) return false;
    if (!get(in, r.fps)) return false;
    if (!getText(in, r.name)) return false;
    if (!get(in, r.totalFrames)) return false;
    if (!get(in, r.layout)) return false;
    if (!(r.tps >= 1.f && r.tps <= 5000000.f)) return false;

    uint64_t eventCount = 0;
    if (!get(in, eventCount) || eventCount > kMaxItems) return false;
    r.events.reserve(static_cast<size_t>(eventCount));

    for (uint64_t i = 0; i < eventCount; i++) {
        InputEvent ev;
        if (!get(in, ev.packed)) return false;
        const int button = ev.button();
        if (button < 1 || button > 3) return false;
        r.events.push_back(ev);
    }

    uint64_t frameCount = 0;
    if (!get(in, frameCount) || frameCount > kMaxItems) return false;
    r.frames.reserve(static_cast<size_t>(frameCount));

    for (uint64_t i = 0; i < frameCount; i++) {
        MacroFrame row;
        uint8_t full = 0;
        if (!get(in, row.frame)) return false;
        if (!getState(in, row.p1) || !getState(in, row.p2)) return false;
        if (!get(in, row.hold) || !get(in, full)) return false;
        row.full = full != 0;
        r.frames.push_back(row);
    }

    uint64_t superCount = 0;
    if (!get(in, superCount) || superCount > kMaxItems) return false;
    r.supers.reserve(static_cast<size_t>(superCount));

    for (uint64_t i = 0; i < superCount; i++) {
        SuperFrame sf;
        if (!get(in, sf.frame)) return false;
        if (!getBlobData(in, sf.p1) || !getBlobData(in, sf.p2)) return false;
        r.supers.push_back(std::move(sf));
    }

    if (v9) {
        uint8_t has = 0;
        if (!get(in, has)) return false;
        if (has) {
            std::array<uint32_t, NXR::Stats::LevelStats::kFieldCount> values{};
            for (auto& value : values) {
                if (!get(in, value)) return false;
            }
            r.stats = NXR::Stats::LevelStats::fromArray(values);
            r.hasStats = true;
        }
    }

    if (v10) {
        uint8_t flags = 0;
        if (!get(in, flags)) return false;
        r.noclip = (flags & 1) != 0;
    }

    char trailing = 0;
    if (in.read(&trailing, 1)) return false;

    r.finalize(false);
    macro = std::move(r);
    return true;
}

bool NXR::Bot::saveMacroJson(const Macro& macro, const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    nlohmann::json root = nlohmann::json::object();
    root["bot"] = {{"name", "NXR"}, {"version", macro.version}};
    root["name"] = macro.name;
    root["framerate"] = macro.tps;
    root["duration"] = macro.tps > 0.f ? static_cast<double>(macro.endFrame()) / static_cast<double>(macro.tps) : 0.0;
    root["levelInfo"] = {{"id", macro.levelId}, {"name", macro.levelName}};
    {
        NXR::Stats::LevelStats stats = macro.stats;
        bool hasStats = macro.hasStats;
        if (!hasStats && macro.levelId != 0) hasStats = NXR::Stats::cachedFor(macro.levelId, stats);
        if (hasStats) {
            root["levelInfo"]["stats"] = {
                {"objects", stats.objects}, {"solids", stats.solids}, {"hazards", stats.hazards},
                {"decorations", stats.decorations}, {"orbs", stats.orbs}, {"pads", stats.pads},
                {"portals", stats.portals}, {"gravityPortals", stats.gravityPortals},
                {"speedPortals", stats.speedPortals}, {"modePortals", stats.modePortals},
                {"sizePortals", stats.sizePortals}, {"mirrorPortals", stats.mirrorPortals},
                {"dualPortals", stats.dualPortals}, {"coins", stats.coins},
                {"triggers", stats.triggers}, {"startPositions", stats.startPositions}
            };
        }
    }

    nlohmann::json inputs = nlohmann::json::array();
    for (const auto& ev : macro.events) {
        inputs.push_back({
            {"frame", ev.frame()},
            {"btn", static_cast<int>(ev.button())},
            {"2p", ev.player() == 2},
            {"down", ev.down()}
        });
    }
    root["inputs"] = std::move(inputs);

    nlohmann::json fixes = nlohmann::json::array();
    for (const auto& row : macro.frames) {
        fixes.push_back({
            {"frame", row.frame},
            {"p1", {{"x", row.p1.x}, {"y", row.p1.y}, {"r", row.p1.rot}}},
            {"p2", {{"x", row.p2.x}, {"y", row.p2.y}, {"r", row.p2.rot}}}
        });
    }
    root["frameFixes"] = std::move(fixes);

    const auto tempPath = path.string() + ".tmp";
    std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    out << root.dump();
    out.flush();
    if (!out.good()) {
        out.close();
        std::filesystem::remove(tempPath, ec);
        return false;
    }
    out.close();

    std::filesystem::rename(tempPath, path, ec);
    if (ec) {
        ec.clear();
        std::filesystem::remove(path, ec);
        ec.clear();
        std::filesystem::rename(tempPath, path, ec);
        if (ec) {
            std::filesystem::remove(tempPath, ec);
            return false;
        }
    }
    return true;
}
