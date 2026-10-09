#include "nxr_render_audio.hpp"
#include <Geode/Geode.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>

#ifdef GEODE_IS_ANDROID
#if __has_include(<fmod_dsp.h>)
#include <fmod_dsp.h>
#elif __has_include(<Geode/fmod/fmod_dsp.h>)
#include <Geode/fmod/fmod_dsp.h>
#endif

namespace {
    FMOD_RESULT F_CALL tapRead(FMOD_DSP_STATE*, float* input, float* output, unsigned int length, int inputChannels, int* outputChannels) {
        if (output != input) std::memcpy(output, input, static_cast<size_t>(length) * static_cast<size_t>(inputChannels) * sizeof(float));
        *outputChannels = inputChannels;
        NXR::Render::AudioTap::get().feed(input, length, inputChannels);
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

    void AudioTap::resetCounters() {
        if (m_ring.size() != kRingFrames * 2) m_ring.assign(kRingFrames * 2, 0.f);
        m_head.store(0);
        m_tail.store(0);
        m_produced.store(0);
        m_padded.store(0);
        m_written = 0;
        m_phase = 0.0;
        m_prevLeft = 0.f;
        m_prevRight = 0.f;
        m_primed = false;
        m_speed = 1.0;
        m_haveLast = false;
        m_pitch.store(1.f);
    }

    bool AudioTap::attach() {
        if (m_dsp) return true;
        auto* system = audioSystem();
        auto* group = masterGroup();
        if (!system || !group) return false;

        resetCounters();

        FMOD_DSP_DESCRIPTION description {};
        std::strncpy(description.name, "NXRTap", sizeof(description.name) - 1);
        description.pluginsdkversion = FMOD_PLUGIN_SDK_VERSION;
        description.version = 1;
        description.numinputbuffers = 1;
        description.numoutputbuffers = 1;
        description.read = tapRead;

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
        m_gate.store(false);
        if (m_dsp) {
            auto* dsp = static_cast<FMOD::DSP*>(m_dsp);
            m_dsp = nullptr;
            if (auto* group = masterGroup()) group->removeDSP(dsp);
            dsp->release();
        }
        applyPitch(1.f);
    }

    void AudioTap::applyPitch(float pitch) {
        if (auto* group = masterGroup()) group->setPitch(pitch);
        m_pitch.store(pitch);
    }

    void AudioTap::setGate(bool open) {
        if (!m_dsp) return;
        if (m_gate.load() == open) return;
        m_gate.store(open);
        if (open) return;
        m_haveLast = false;
        m_speed = 1.0;
        if (std::fabs(m_pitch.load() - 1.f) > 0.001f) applyPitch(1.f);
    }

    void AudioTap::onVideoFrame(uint64_t framesQueued, int fps) {
        if (!m_dsp || fps <= 0) return;

        const auto now = std::chrono::steady_clock::now();
        if (m_haveLast) {
            const double realDelta = std::chrono::duration<double>(now - m_last).count();
            if (realDelta > 0.0) {
                const double instant = std::clamp((1.0 / fps) / realDelta, 0.2, 4.0);
                m_speed = m_speed * 0.92 + instant * 0.08;
            }
        }
        m_last = now;
        m_haveLast = true;

        const double gameTime = static_cast<double>(framesQueued) / fps;
        const double audioTime = static_cast<double>(m_produced.load() + m_padded.load()) / m_rate;
        const double drift = gameTime - audioTime;

        double target = m_speed * (1.0 + std::clamp(drift * 0.8, -0.1, 0.1));
        if (std::fabs(m_speed - 1.0) < 0.04 && std::fabs(drift) < 0.03) target = 1.0;
        target = std::clamp(target, 0.25, 4.0);

        const float current = m_pitch.load();
        if (std::fabs(target - current) / current > 0.02) applyPitch(static_cast<float>(target));
    }

    size_t AudioTap::available() const {
        return m_head.load(std::memory_order_acquire) - m_tail.load(std::memory_order_relaxed);
    }

    void AudioTap::collect(uint64_t frameIndex, int fps, std::vector<float>& out) {
        if (fps <= 0 || m_ring.empty()) return;

        const uint64_t target = (frameIndex + 1) * static_cast<uint64_t>(m_rate) / static_cast<uint64_t>(fps);
        const uint64_t need = target > m_written ? target - m_written : 0;
        if (need == 0) return;

        size_t backlog = available();
        const size_t limit = static_cast<size_t>(m_rate);
        if (backlog > limit) {
            const size_t drop = backlog - static_cast<size_t>(m_rate / 4);
            m_tail.fetch_add(drop, std::memory_order_release);
            backlog -= drop;
        }

        const size_t take = static_cast<size_t>(std::min<uint64_t>(need, backlog));
        size_t tail = m_tail.load(std::memory_order_relaxed);
        const size_t base = out.size();
        out.resize(base + take * 2);
        for (size_t index = 0; index < take; ++index) {
            const size_t slot = (tail + index) & (kRingFrames - 1);
            out[base + index * 2] = m_ring[slot * 2];
            out[base + index * 2 + 1] = m_ring[slot * 2 + 1];
        }
        m_tail.store(tail + take, std::memory_order_release);
        m_written += take;

        const uint64_t shortfall = need - take;
        if (shortfall > static_cast<uint64_t>(m_rate) * 12 / 100) {
            out.resize(out.size() + static_cast<size_t>(shortfall) * 2, 0.f);
            m_written += shortfall;
            m_padded.fetch_add(shortfall);
        }
    }

    void AudioTap::feed(const float* input, unsigned frames, int channels) {
        if (frames == 0 || channels <= 0 || m_ring.empty()) return;
        if (!m_gate.load(std::memory_order_relaxed)) {
            m_primed = false;
            return;
        }

        auto sample = [&](size_t index, float& left, float& right) {
            const float* frame = input + index * static_cast<size_t>(channels);
            left = frame[0];
            right = channels > 1 ? frame[1] : frame[0];
        };

        if (!m_primed) {
            m_primed = true;
            m_phase = 0.0;
            sample(0, m_prevLeft, m_prevRight);
        }

        const float pitch = std::clamp(m_pitch.load(std::memory_order_relaxed), 0.05f, 8.f);
        const double step = 1.0 / pitch;
        const double limit = static_cast<double>(frames) - 1.0;

        size_t head = m_head.load(std::memory_order_relaxed);
        const size_t tail = m_tail.load(std::memory_order_acquire);
        uint64_t produced = 0;
        double position = m_phase;

        while (position < limit) {
            if (head - tail >= kRingFrames) break;

            const long whole = static_cast<long>(std::floor(position));
            const float fraction = static_cast<float>(position - static_cast<double>(whole));

            float leftA, rightA, leftB, rightB;
            if (whole < 0) {
                leftA = m_prevLeft;
                rightA = m_prevRight;
            } else {
                sample(static_cast<size_t>(whole), leftA, rightA);
            }
            sample(static_cast<size_t>(whole + 1), leftB, rightB);

            const size_t slot = head & (kRingFrames - 1);
            m_ring[slot * 2] = leftA + (leftB - leftA) * fraction;
            m_ring[slot * 2 + 1] = rightA + (rightB - rightA) * fraction;
            ++head;
            ++produced;
            position += step;
        }

        m_head.store(head, std::memory_order_release);
        m_produced.fetch_add(produced, std::memory_order_relaxed);
        m_phase = position - static_cast<double>(frames);
        sample(frames - 1, m_prevLeft, m_prevRight);
    }
}
#else
namespace NXR::Render {
    AudioTap& AudioTap::get() {
        static AudioTap instance;
        return instance;
    }
    int AudioTap::probeSampleRate() { return 0; }
    bool AudioTap::attach() { return false; }
    void AudioTap::detach() {}
    void AudioTap::setGate(bool) {}
    void AudioTap::onVideoFrame(uint64_t, int) {}
    void AudioTap::collect(uint64_t, int, std::vector<float>&) {}
    void AudioTap::feed(const float*, unsigned, int) {}
    void AudioTap::applyPitch(float) {}
    void AudioTap::resetCounters() {}
    size_t AudioTap::available() const { return 0; }
}
#endif
