#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "nxr_render_audio.hpp"
#include "nxr_render_readback.hpp"
#include "nxr_video_sink.hpp"

namespace NXR::Render {
    struct VideoSettings {
        int fps = 60;
        int bitrateMbps = 16;
    };

    class GameplayVideoSession {
    public:
        static GameplayVideoSession& get();
        ~GameplayVideoSession();

        geode::Result<std::filesystem::path> begin(const VideoSettings& settings);
        void finish();
        void armTail(int frames);
        void captureFromBackBuffer();
        void tickAudioGate();

        bool isActive() const { return m_active.load(); }
        bool isAdvancing() const;
        int frameRate() const { return std::max(m_settings.fps, 1); }
        float stepSeconds() const { return 1.f / static_cast<float>(frameRate()); }

    private:
        GameplayVideoSession();

        void encodeLoop();
        void flipRows(std::vector<uint8_t>& pixels) const;
        void enqueue(std::vector<uint8_t>&& pixels);
        void recycle(std::vector<uint8_t>&& pixels);
        void joinFinalizer();
        std::vector<uint8_t> takeSpareBuffer();

        std::atomic<bool> m_active { false };
        std::atomic<bool> m_saving { false };
        VideoSettings m_settings;
        int m_width = 0;
        int m_height = 0;
        std::filesystem::path m_outputPath;
        std::unique_ptr<VideoSink> m_encoder;
        FramebufferReadback m_readback;

        std::thread m_encodeThread;
        std::thread m_finalizeThread;
        std::mutex m_queueMutex;
        std::condition_variable m_frameReady;
        std::condition_variable m_spaceFreed;
        std::deque<std::vector<uint8_t>> m_pending;
        bool m_closing = false;
        std::atomic<bool> m_audioActive { false };
        std::vector<float> m_audioScratch;

        std::mutex m_spareMutex;
        std::vector<std::vector<uint8_t>> m_spare;

        uint64_t m_framesQueued = 0;
        int m_tailFrames = 0;
        bool m_tailArmed = false;
    };
}
