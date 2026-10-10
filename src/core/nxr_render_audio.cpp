#include "nxr_render_audio.hpp"
#include <Geode/Geode.hpp>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstring>

#if __has_include(<fmod_dsp.h>)
#include <fmod_dsp.h>
#define NXR_FMOD_TAP 1
#elif __has_include(<Geode/fmod/fmod_dsp.h>)
#include <Geode/fmod/fmod_dsp.h>
#define NXR_FMOD_TAP 1
#endif

#ifdef NXR_FMOD_TAP

namespace {
    FMOD_RESULT F_CALL tapRead(FMOD_DSP_STATE*, float* input, float* output, unsigned int length, int inputChannels, int* outputChannels) {
        *outputChannels = inputChannels;
        const bool mute = NXR::Render::AudioTap::get().process(input, length, inputChannels);
        const size_t bytes = static_cast<size_t>(length) * static_cast<size_t>(inputChannels) * sizeof(float);
        if (mute) std::memset(output, 0, bytes);
        else if (output != input) std::memcpy(output, input, bytes);
        return FMOD_OK;
    }

    FMOD_RESULT F_CALL tapShouldProcess(FMOD_DSP_STATE*, FMOD_BOOL, unsigned int, FMOD_CHANNELMASK, int, FMOD_SPEAKERMODE) {
        return FMOD_OK;
    }

    FMOD::System* audioSystem() {
        auto* engine = FMODAudioEngine::sharedEngine();
        return engine ? engine->m_system : nullptr;
    }

    FMOD::ChannelGroup* masterGroup() {
        auto* system = audioSystem();
        if (!system) return nullptr;
        FMOD::ChannelGroup* group = nullptr;
        if (system->getMasterChannelGroup(&group) != FMOD_OK) return nullptr;
        return group;
    }
}

namespace NXR::Render {
    AudioTap& AudioTap::get() {
        static AudioTap instance;
        return instance;
    }

    int AudioTap::probeSampleRate() {
        auto* system = audioSystem();
        if (!system) return 0;
        int rate = 0;
        FMOD_SPEAKERMODE mode;
        int raw = 0;
        if (system->getSoftwareFormat(&rate, &mode, &raw) != FMOD_OK) return 0;
        if (rate < 8000 || rate > 96000) return 0;
        m_rate = rate;
        return rate;
    }

    void AudioTap::resetCounters(int offsetMs) {
        if (m_ring.size() != kRingFrames * 2) m_ring.assign(kRingFrames * 2, 0.f);
        m_head.store(0);
        m_tail.store(0);
        {
            std::lock_guard lock(m_creditMutex);
            m_credit = 0;
            m_granted = 0;
        }
        m_written = 0;
        m_shiftFrames = static_cast<int64_t>(offsetMs) * m_rate / 1000;
        m_shiftApplied = false;
        m_discardPending = 0;
    }

    bool AudioTap::attach(int offsetMs) {
        if (m_dsp) return true;
        auto* system = audioSystem();
        auto* group = masterGroup();
        if (!system || !group) return false;

        resetCounters(offsetMs);

        FMOD_DSP_DESCRIPTION description {};
        std::strncpy(description.name, "NXRTap", sizeof(description.name) - 1);
        description.pluginsdkversion = FMOD_PLUGIN_SDK_VERSION;
        description.version = 1;
        description.numinputbuffers = 1;
        description.numoutputbuffers = 1;
        description.read = tapRead;
        description.shouldiprocess = tapShouldProcess;

        FMOD::DSP* dsp = nullptr;
        if (system->createDSP(&description, &dsp) != FMOD_OK || !dsp) return false;
        if (group->addDSP(FMOD_CHANNELCONTROL_DSP_TAIL, dsp) != FMOD_OK) {
            dsp->release();
            return false;
        }
        m_dsp = dsp;
        return true;
    }

    void AudioTap::detach() {
        setGate(false);
        m_gate.store(false);
        if (!m_dsp) return;
        auto* dsp = static_cast<FMOD::DSP*>(m_dsp);
        m_dsp = nullptr;
        if (auto* group = masterGroup()) group->removeDSP(dsp);
        dsp->release();
    }

    void AudioTap::setGate(bool open) {
        if (!m_dsp) return;
        m_gate.store(open);
        if (!open) {
            { std::lock_guard lock(m_creditMutex); }
            m_creditCv.notify_all();
        }
    }

    void AudioTap::grantFrame(uint64_t stepIndex, int fps) {
        if (!m_dsp || fps <= 0) return;
        const uint64_t target = (stepIndex + 1) * static_cast<uint64_t>(m_rate) / static_cast<uint64_t>(fps);
        {
            std::lock_guard lock(m_creditMutex);
            if (target <= m_granted) return;
            m_credit += static_cast<int64_t>(target - m_granted);
            m_granted = target;
        }
        m_creditCv.notify_all();
    }

    bool AudioTap::process(const float* input, unsigned frames, int channels) {
        if (frames == 0 || channels <= 0 || m_ring.empty()) return false;
        if (!m_gate.load(std::memory_order_acquire)) return false;
        {
            // The long timeout is only a deadlock guard. A slow frame must never break sync.
            std::unique_lock lock(m_creditMutex);
            m_creditCv.wait_for(lock, std::chrono::seconds(5), [&] {
                return !m_gate.load(std::memory_order_acquire) || m_credit >= static_cast<int64_t>(frames);
            });
            if (!m_gate.load(std::memory_order_acquire)) return false;
            m_credit -= static_cast<int64_t>(frames);
        }
        feed(input, frames, channels);
        return true;
    }

    void AudioTap::collect(int64_t targetUs, std::vector<float>& out) {
        if (m_ring.empty() || targetUs <= 0) return;

        if (!m_shiftApplied) {
            m_shiftApplied = true;
            if (m_shiftFrames > 0) {
                out.resize(static_cast<size_t>(m_shiftFrames) * 2, 0.f);
                m_written += static_cast<uint64_t>(m_shiftFrames);
            } else if (m_shiftFrames < 0) {
                m_discardPending = static_cast<uint64_t>(-m_shiftFrames);
            }
        }

        const size_t head = m_head.load(std::memory_order_acquire);
        size_t tail = m_tail.load(std::memory_order_relaxed);

        if (m_discardPending > 0) {
            const size_t drop = static_cast<size_t>(std::min<uint64_t>(m_discardPending, head - tail));
            tail += drop;
            m_discardPending -= drop;
            m_tail.store(tail, std::memory_order_release);
        }

        const uint64_t target = static_cast<uint64_t>(targetUs) * static_cast<uint64_t>(m_rate) / 1000000ULL;
        const uint64_t need = target > m_written ? target - m_written : 0;
        if (need == 0) return;

        const size_t backlog = head - tail;
        const size_t take = static_cast<size_t>(std::min<uint64_t>(need, backlog));
        const size_t base = out.size();
        out.resize(base + take * 2);
        for (size_t index = 0; index < take; ++index) {
            const size_t slot = (tail + index) & (kRingFrames - 1);
            out[base + index * 2] = m_ring[slot * 2];
            out[base + index * 2 + 1] = m_ring[slot * 2 + 1];
        }
        m_tail.store(tail + take, std::memory_order_release);
        m_written += take;
    }

    void AudioTap::feed(const float* input, unsigned frames, int channels) {
        if (frames == 0 || channels <= 0 || m_ring.empty()) return;
        if (!m_gate.load(std::memory_order_relaxed)) return;

        size_t head = m_head.load(std::memory_order_relaxed);
        const size_t tail = m_tail.load(std::memory_order_acquire);

        for (unsigned index = 0; index < frames; ++index) {
            if (head - tail >= kRingFrames) break;
            const float* frame = input + static_cast<size_t>(index) * static_cast<size_t>(channels);
            const size_t slot = head & (kRingFrames - 1);
            m_ring[slot * 2] = frame[0];
            m_ring[slot * 2 + 1] = channels > 1 ? frame[1] : frame[0];
            ++head;
        }

        m_head.store(head, std::memory_order_release);
    }
}
#else
namespace NXR::Render {
    AudioTap& AudioTap::get() {
        static AudioTap instance;
        return instance;
    }
    int AudioTap::probeSampleRate() { return 0; }
    bool AudioTap::attach(int) { return false; }
    void AudioTap::detach() {}
    void AudioTap::setGate(bool) {}
    void AudioTap::collect(int64_t, std::vector<float>&) {}
    void AudioTap::feed(const float*, unsigned, int) {}
    void AudioTap::grantFrame(uint64_t, int) {}
    bool AudioTap::process(const float*, unsigned, int) { return false; }
    void AudioTap::resetCounters(int) {}
}
#endif
