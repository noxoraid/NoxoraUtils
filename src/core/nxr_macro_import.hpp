#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "nxr_bot.hpp"

namespace NXR::Bot {

    struct ReplayFile {
        std::filesystem::path path;
        std::string label;
        std::string ext;
        uintmax_t size = 0;
    };

    std::vector<ReplayFile> scanReplayFiles();
    bool importReplay(const std::filesystem::path& path, Macro& out, std::string& format);
}
