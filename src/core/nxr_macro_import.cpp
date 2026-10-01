#include "nxr_macro_import.hpp"
#include "nxr_config.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <json.hpp>

using namespace NXR::Bot;

namespace {
    constexpr uintmax_t kMaxFileSize = 256ull * 1024ull * 1024ull;
    constexpr size_t kMaxResults = 400;
    constexpr int kMaxDepth = 3;

    std::string lowerExt(const std::filesystem::path& path) {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return ext;
    }

    std::string lowerName(const std::filesystem::path& path) {
        std::string name = path.filename().string();
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return name;
    }

    bool endsWith(const std::string& text, const std::string& suffix) {
        return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    bool readAll(const std::filesystem::path& path, std::vector<uint8_t>& data) {
        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec || size == 0 || size > kMaxFileSize) return false;

        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) return false;

        data.resize(static_cast<size_t>(size));
        in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
        return static_cast<bool>(in);
    }

    void finish(Macro& macro, const std::filesystem::path& path, float tps) {
        macro.name = path.stem().string();
        if (endsWith(lowerName(path), ".gdr.json")) macro.name = path.stem().stem().string();
        macro.tps = tps > 0.f ? tps : 240.f;
        macro.totalFrames = 0;
        macro.finalize(true);
    }

    bool addEvent(Macro& macro, int64_t frame, int button, bool player2, bool down) {
        if (frame < 0 || frame > static_cast<int64_t>(InputEvent::kMaxFrame)) return false;
        if (button < 1 || button > 3) return false;
        macro.events.push_back(InputEvent::make(static_cast<uint64_t>(frame), player2 ? 2 : 1, static_cast<uint8_t>(button), down));
        return true;
    }

    bool parseSlc(const std::vector<uint8_t>& data, Macro& out, const std::filesystem::path& path) {
        if (data.size() < 12) return false;

        double fps = 0.0;
        uint32_t count = 0;
        std::memcpy(&fps, data.data(), sizeof(double));
        std::memcpy(&count, data.data() + sizeof(double), sizeof(uint32_t));

        const size_t body = 12 + static_cast<size_t>(count) * 4;
        if (data.size() != body && data.size() != body + 8) return false;
        if (!(fps >= 1.0 && fps <= 5000000.0)) return false;

        Macro macro;
        macro.events.reserve(count);

        for (uint32_t i = 0; i < count; i++) {
            uint32_t state = 0;
            std::memcpy(&state, data.data() + 12 + static_cast<size_t>(i) * 4, sizeof(uint32_t));
            const uint32_t frame = (state & 0xfffffff0u) >> 4;
            const bool player2 = ((state >> 3) & 1u) != 0;
            const int button = static_cast<int>((state >> 1) & 3u);
            const bool down = (state & 1u) != 0;
            addEvent(macro, frame, button, player2, down);
        }

        if (macro.events.empty()) return false;

        finish(macro, path, static_cast<float>(fps));
        out = std::move(macro);
        return true;
    }

    bool parseXd(const std::string& text, Macro& out, const std::filesystem::path& path) {
        std::istringstream stream(text);
        std::string line;
        float multiplier = 1.f;
        Macro macro;
        int lines = 0;

        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            std::vector<std::string> parts;
            std::stringstream ss(line);
            std::string item;
            while (std::getline(ss, item, '|')) parts.push_back(item);
            if (parts.empty()) continue;

            try {
                if (parts.size() < 4) {
                    if (parts[0] == "android") multiplier = 4.f;
                    else {
                        const int fps = std::stoi(parts[0]);
                        if (fps <= 0) return false;
                        multiplier = 240.f / static_cast<float>(fps);
                    }
                    continue;
                }

                lines++;
                const int64_t frame = static_cast<int64_t>(std::lround(std::stod(parts[0]) * multiplier)) + 1;
                const int button = std::stoi(parts[2]);
                const bool hold = parts[1] == "1";
                const bool player2 = parts[3] == "1";
                const bool posOnly = parts.size() > 4 && parts[4] == "1";

                if (!posOnly) {
                    addEvent(macro, frame, button, player2, hold);
                } else if (parts.size() >= 13) {
                    MacroFrame row;
                    row.frame = static_cast<uint32_t>(std::max<int64_t>(0, frame - 1));
                    row.p1.x = std::stof(parts[5]);
                    row.p1.y = std::stof(parts[6]);
                    row.p2.x = std::stof(parts[11]);
                    row.p2.y = std::stof(parts[12]);
                    row.full = false;
                    macro.frames.push_back(row);
                }
            } catch (...) {
                return false;
            }
        }

        if (lines == 0 || macro.events.empty()) return false;

        finish(macro, path, 240.f);
        out = std::move(macro);
        return true;
    }

    float readFloat(const nlohmann::json& object, const char* key) {
        if (!object.is_object() || !object.contains(key)) return 0.f;
        const auto& value = object[key];
        return value.is_number() ? value.get<float>() : 0.f;
    }

    bool parseGdr(nlohmann::json& root, Macro& out, const std::filesystem::path& path, std::string& format) {
        if (!root.is_object() || !root.contains("inputs") || !root["inputs"].is_array()) return false;

        std::string botName;
        std::string botVersion;
        if (root.contains("bot") && root["bot"].is_object()) {
            if (root["bot"].contains("name") && root["bot"]["name"].is_string()) botName = root["bot"]["name"].get<std::string>();
            if (root["bot"].contains("version") && root["bot"]["version"].is_string()) botVersion = root["bot"]["version"].get<std::string>();
        }

        float framerate = 240.f;
        if (root.contains("framerate") && root["framerate"].is_number()) framerate = root["framerate"].get<float>();

        int64_t inputShift = 0;
        int64_t fixShift = 0;
        if (botName == "xdBot") {
            inputShift = 1;
            std::string version = botVersion;
            if (!version.empty() && version.front() == 'v') version.erase(version.begin());
            std::vector<int> nums;
            std::stringstream ss(version);
            std::string part;
            while (std::getline(ss, part, '.')) {
                try { nums.push_back(std::stoi(part)); } catch (...) { nums.push_back(0); }
            }
            while (nums.size() < 3) nums.push_back(0);
            const bool modern = nums[0] > 2 || (nums[0] == 2 && (nums[1] > 3 || (nums[1] == 3 && nums[2] >= 6)));
            if (!modern) {
                inputShift += 1;
                fixShift = 1;
            }
        }

        Macro macro;
        for (const auto& input : root["inputs"]) {
            if (!input.is_object() || !input.contains("frame") || !input["frame"].is_number_integer()) continue;
            const int64_t frame = input["frame"].get<int64_t>() + inputShift;
            const int button = input.contains("btn") && input["btn"].is_number_integer() ? input["btn"].get<int>() : 1;
            const bool player2 = input.contains("2p") && input["2p"].is_boolean() && input["2p"].get<bool>();
            const bool down = input.contains("down") && input["down"].is_boolean() && input["down"].get<bool>();
            addEvent(macro, frame, button, player2, down);
        }

        if (root.contains("frameFixes") && root["frameFixes"].is_array()) {
            for (const auto& item : root["frameFixes"]) {
                if (!item.is_object() || !item.contains("frame") || !item["frame"].is_number_integer()) continue;
                const int64_t frame = item["frame"].get<int64_t>() + fixShift;
                if (frame < 0 || frame > static_cast<int64_t>(InputEvent::kMaxFrame)) continue;

                MacroFrame row;
                row.frame = static_cast<uint32_t>(frame);

                if (item.contains("p1") && item["p1"].is_object()) {
                    row.p1.x = readFloat(item["p1"], "x");
                    row.p1.y = readFloat(item["p1"], "y");
                    row.p1.rot = readFloat(item["p1"], "r");
                } else if (item.contains("player1X")) {
                    row.p1.x = readFloat(item, "player1X");
                    row.p1.y = readFloat(item, "player1Y");
                } else {
                    continue;
                }

                if (item.contains("p2") && item["p2"].is_object()) {
                    row.p2.x = readFloat(item["p2"], "x");
                    row.p2.y = readFloat(item["p2"], "y");
                    row.p2.rot = readFloat(item["p2"], "r");
                } else if (item.contains("player2X")) {
                    row.p2.x = readFloat(item, "player2X");
                    row.p2.y = readFloat(item, "player2Y");
                }

                row.full = false;
                macro.frames.push_back(row);
            }
        }

        if (macro.events.empty()) return false;

        finish(macro, path, framerate);
        format = botName.empty() ? "GDR" : "GDR (" + botName + ")";
        out = std::move(macro);
        return true;
    }

    bool looksLikeXd(const std::vector<uint8_t>& data) {
        const size_t limit = std::min<size_t>(data.size(), 200);
        size_t pipes = 0;
        for (size_t i = 0; i < limit; i++) {
            const uint8_t c = data[i];
            if (c == '|') pipes++;
            if (c == 0) return false;
            if (c != '\n' && c != '\r' && c != '\t' && (c < 32 || c > 126)) return false;
        }
        return pipes > 0 || (data.size() < 12 && std::isdigit(static_cast<unsigned char>(data[0])));
    }

    void scanRoot(const std::filesystem::path& root, int depthLimit, bool allowJson, std::vector<ReplayFile>& out) {
        std::error_code ec;
        if (!std::filesystem::is_directory(root, ec)) return;

        std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec);
        std::filesystem::recursive_directory_iterator end;
        if (ec) return;

        for (; it != end && out.size() < kMaxResults; it.increment(ec)) {
            if (ec) break;
            if (it.depth() >= depthLimit) it.disable_recursion_pending();
            if (!it->is_regular_file(ec)) continue;

            const auto& p = it->path();
            const std::string ext = lowerExt(p);
            const std::string name = lowerName(p);
            const std::string stem = p.stem().string();

            const bool gdrJson = endsWith(name, ".gdr.json");
            const bool known = ext == ".nxr" || ext == ".gdr" || ext == ".xd" || ext == ".slc" || gdrJson || (allowJson && ext == ".json");
            if (!known) continue;
            if (ext == ".nxr" && (stem == "_autosave" || stem.rfind("_backup_", 0) == 0)) continue;

            ReplayFile file;
            file.path = p;
            file.ext = gdrJson ? "gdr.json" : ext.substr(1);
            file.size = it->file_size(ec);

            std::error_code rel;
            auto relative = std::filesystem::relative(p, root, rel);
            file.label = rel ? p.filename().string() : relative.string();
            out.push_back(std::move(file));
        }
    }
}

std::vector<ReplayFile> NXR::Bot::scanReplayFiles() {
    std::vector<ReplayFile> files;
    const auto macroDir = getFolderMacroPath();
    scanRoot(macroDir, kMaxDepth, true, files);

    const auto parent = macroDir.parent_path();
    scanRoot(parent / "geode" / "config", kMaxDepth + 1, false, files);
    scanRoot(parent / "macros", kMaxDepth, true, files);

    std::sort(files.begin(), files.end(), [](const ReplayFile& a, const ReplayFile& b) {
        return a.label < b.label;
    });

    files.erase(std::unique(files.begin(), files.end(), [](const ReplayFile& a, const ReplayFile& b) {
        return a.path == b.path;
    }), files.end());

    return files;
}

bool NXR::Bot::importReplay(const std::filesystem::path& path, Macro& out, std::string& format) {
    std::vector<uint8_t> data;
    if (!readAll(path, data)) return false;

    if (data.size() >= 4 && data[0] == 'N' && data[1] == 'X' && data[2] == 'R') {
        Macro macro;
        if (!loadMacro(macro, path)) return false;
        if (macro.name.empty()) macro.name = path.stem().string();
        out = std::move(macro);
        format = "NXR";
        return true;
    }

    const std::string ext = lowerExt(path);

    if (ext == ".xd" || looksLikeXd(data)) {
        std::string text(data.begin(), data.end());
        if (parseXd(text, out, path)) {
            format = "XD (xdBot legacy)";
            return true;
        }
    }

    if (ext == ".slc") {
        if (parseSlc(data, out, path)) {
            format = "SLC (Silicate)";
            return true;
        }
    }

    {
        nlohmann::json root = nlohmann::json::from_msgpack(data, true, false);
        if (!root.is_discarded() && parseGdr(root, out, path, format)) return true;
    }

    {
        nlohmann::json root = nlohmann::json::parse(data.begin(), data.end(), nullptr, false);
        if (!root.is_discarded() && parseGdr(root, out, path, format)) return true;
    }

    if (ext != ".slc" && parseSlc(data, out, path)) {
        format = "SLC (Silicate)";
        return true;
    }

    return false;
}
