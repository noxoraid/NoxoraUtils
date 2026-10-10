#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <vector>

namespace NXR::Render {
    class AudioTap {
    public:
        static AudioTap& get();

        int probeSampleRate();
        bool attach(int offsetMs);
        void detach();
        void setGate(bool open);
        void collect(int64_t targetUs, std::vector<float>& out);
        void feed(const float* input, unsigned frames, int channels);

        // Lockstep: the FMOD mixer may only produce as many samples as the game has
        // rendered video frames for, so audio length always equals video length.
        void grantFrame(uint64_t stepIndex, int fps);
        // Called on the FMOD mixer thread. Blocks until enough credit exists, records the
        // block and returns true when the speaker output must be muted.
        bool process(const float* input, unsigned frames, int channels);

    private:
        AudioTap() = default;

        void resetCounters(int offsetMs);

        static constexpr size_t kRingFrames = size_t(1) << 18;

        void* m_dsp = nullptr;
        int m_rate = 48000;
        std::vector<float> m_ring;
        std::atomic<size_t> m_head { 0 };
        std::atomic<size_t> m_tail { 0 };
        std::atomic<bool> m_gate { false };

        std::mutex m_creditMutex;
        std::condition_variable m_creditCv;
        int64_t m_credit = 0;
        uint64_t m_granted = 0;

        uint64_t m_written = 0;
        int64_t m_shiftFrames = 0;
        bool m_shiftApplied = false;
        uint64_t m_discardPending = 0;
    };
}
