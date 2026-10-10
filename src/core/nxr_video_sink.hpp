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
        int bitrateMode = 1;        // 1 = variable (VBR), 2 = constant (CBR)
        bool profileHigh = true;    // H.264 High profile, falls back to the device default
        bool bt709 = true;          // BT.709 (HD) or BT.601 color matrix
        bool fullRange = false;     // full range (PC) or limited range (TV)
        int encoder = 0;            // desktop only: 0 auto, 1 CPU, 2 NVIDIA, 3 AMD, 4 Intel, 5 Apple
    };

    class VideoSink {
    public:
        virtual ~VideoSink() = default;
        virtual geode::Result<> open(const SinkConfig& config) = 0;
        virtual void write(const std::vector<uint8_t>& topDownRgba, int64_t ptsUs) = 0;
        virtual void writeAudio(const float* interleavedStereo, size_t frames) = 0;
        virtual void close() = 0;
    };

    std::unique_ptr<VideoSink> makeMediaCodecSink();   // Android
    std::unique_ptr<VideoSink> makeFfmpegSink();       // Windows and macOS
    std::unique_ptr<VideoSink> makeVideoSink();        // picks the right one for this platform
}
