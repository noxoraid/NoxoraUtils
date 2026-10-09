#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <vector>

namespace NXR::Render {
    class AudioTap {
    public:
        static AudioTap& get();

        int probeSampleRate();
        bool attach();
        void detach();
        void setGate(bool open);
        void onVideoFrame(uint64_t framesQueued, int fps);
        void collect(uint64_t frameIndex, int fps, std::vector<float>& out);
        void feed(const float* input, unsigned frames, int channels);

    private:
        AudioTap() = default;

        void applyPitch(float pitch);
        void resetCounters();
        size_t available() const;

        static constexpr size_t kRingFrames = size_t(1) << 18;

        void* m_dsp = nullptr;
        int m_rate = 48000;
        std::vector<float> m_ring;
        std::atomic<size_t> m_head { 0 };
        std::atomic<size_t> m_tail { 0 };
        std::atomic<bool> m_gate { false };
        std::atomic<float> m_pitch { 1.f };
        std::atomic<uint64_t> m_produced { 0 };
        std::atomic<uint64_t> m_padded { 0 };

        double m_phase = 0.0;
        float m_prevLeft = 0.f;
        float m_prevRight = 0.f;
        bool m_primed = false;

        uint64_t m_written = 0;

        double m_speed = 1.0;
        bool m_haveLast = false;
        std::chrono::steady_clock::time_point m_last;
    };
}
