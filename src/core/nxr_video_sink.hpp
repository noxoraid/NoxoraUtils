#pragma once
#include <Geode/Geode.hpp>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace NXR::Render {
    struct SinkConfig {
        std::filesystem::path outputPath;
        int width = 0;
        int height = 0;
        int fps = 60;
        int bitrateMbps = 16;
        int audioSampleRate = 0;
    };

    class VideoSink {
    public:
        virtual ~VideoSink() = default;
        virtual geode::Result<> open(const SinkConfig& config) = 0;
        virtual void write(const std::vector<uint8_t>& topDownRgba) = 0;
        virtual void writeAudio(const float* interleavedStereo, size_t frames) = 0;
        virtual void close() = 0;
    };

    std::unique_ptr<VideoSink> makeMediaCodecSink();
}
