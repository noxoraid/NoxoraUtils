#include "nxr_video_sink.hpp"

#ifdef GEODE_IS_ANDROID
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <media/NdkMediaMuxer.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <dlfcn.h>
#include <thread>
#include <vector>

namespace {
    constexpr int32_t kColorFormatSemiPlanar = 21;
    constexpr int32_t kBitrateModeVariable = 1;
    constexpr int64_t kDequeueTimeoutUs = 10000;

    uint8_t clampByte(int value) {
        return static_cast<uint8_t>(std::clamp(value, 0, 255));
    }

    // Integer RGB -> YCbCr coefficients (16 bit fixed point) for the chosen matrix and range.
    struct ColorMatrix {
        int yr = 0, yg = 0, yb = 0;
        int cbr = 0, cbg = 0, cbb = 0;
        int crr = 0, crg = 0, crb = 0;
        int yOffset = 16;
    };

    ColorMatrix buildColorMatrix(bool bt709, bool fullRange) {
        const double kr = bt709 ? 0.2126 : 0.299;
        const double kb = bt709 ? 0.0722 : 0.114;
        const double kg = 1.0 - kr - kb;
        const double yScale = fullRange ? 1.0 : 219.0 / 255.0;
        const double cScale = fullRange ? 1.0 : 224.0 / 255.0;
        const double cbDiv = 2.0 * (1.0 - kb);
        const double crDiv = 2.0 * (1.0 - kr);
        auto fixed = [](double value) { return static_cast<int>(std::lround(value * 65536.0)); };

        ColorMatrix m;
        m.yr = fixed(kr * yScale);
        m.yg = fixed(kg * yScale);
        m.yb = fixed(kb * yScale);
        m.cbr = fixed(-kr / cbDiv * cScale);
        m.cbg = fixed(-kg / cbDiv * cScale);
        m.cbb = fixed(0.5 * cScale);
        m.crr = fixed(0.5 * cScale);
        m.crg = fixed(-kg / crDiv * cScale);
        m.crb = fixed(-kb / crDiv * cScale);
        m.yOffset = fullRange ? 0 : 16;
        return m;
    }

    class MediaCodecSink final : public NXR::Render::VideoSink {
    public:
        ~MediaCodecSink() override { close(); }

        geode::Result<> open(const NXR::Render::SinkConfig& config) override {
            m_width = config.width;
            m_height = config.height;
            m_fps = std::max(config.fps, 1);
            m_audioRate = config.audioSampleRate;

            m_fileDescriptor = ::open(config.outputPath.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0644);
            if (m_fileDescriptor < 0) return geode::Err("Cannot open the output file");

            m_muxer = AMediaMuxer_new(m_fileDescriptor, AMEDIAMUXER_OUTPUT_FORMAT_MPEG_4);
            if (!m_muxer) return failOpen("Cannot create the MP4 muxer");

            m_matrix = buildColorMatrix(config.bt709, config.fullRange);

            // Tries the requested format first. If the device rejects the High profile it
            // is retried once with the device default, so recording never fails because of it.
            bool configured = false;
            for (int attempt = 0; attempt < 2 && !configured; ++attempt) {
                const bool withProfile = config.profileHigh && attempt == 0;
                if (attempt == 1 && !config.profileHigh) break;

                m_codec = AMediaCodec_createEncoderByType("video/avc");
                if (!m_codec) return failOpen("No H.264 hardware encoder on this device");

                AMediaFormat* format = AMediaFormat_new();
                AMediaFormat_setString(format, "mime", "video/avc");
                AMediaFormat_setInt32(format, "width", m_width);
                AMediaFormat_setInt32(format, "height", m_height);
                AMediaFormat_setInt32(format, "bitrate", config.bitrateMbps * 1000000);
                AMediaFormat_setInt32(format, "bitrate-mode", config.bitrateMode == 2 ? 2 : kBitrateModeVariable);
                AMediaFormat_setInt32(format, "frame-rate", m_fps);
                AMediaFormat_setInt32(format, "color-format", kColorFormatSemiPlanar);
                AMediaFormat_setInt32(format, "i-frame-interval", 1);
                // Tell players which matrix and range the pixels use, so colors match the game.
                AMediaFormat_setInt32(format, "color-standard", config.bt709 ? 1 : 4);
                AMediaFormat_setInt32(format, "color-range", config.fullRange ? 1 : 2);
                AMediaFormat_setInt32(format, "color-transfer", 3);
                if (withProfile) AMediaFormat_setInt32(format, "profile", 8);

                configured = AMediaCodec_configure(m_codec, format, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE) == AMEDIA_OK;
                AMediaFormat_delete(format);
                if (!configured) {
                    AMediaCodec_delete(m_codec);
                    m_codec = nullptr;
                }
            }
            if (!configured) return failOpen("The encoder rejected this resolution or bitrate");
            if (AMediaCodec_start(m_codec) != AMEDIA_OK) return failOpen("The encoder did not start");

            m_stride = m_width;
            m_sliceHeight = m_height;
            using InputFormatFn = AMediaFormat* (*)(AMediaCodec*);
            static auto inputFormatOf = reinterpret_cast<InputFormatFn>(dlsym(RTLD_DEFAULT, "AMediaCodec_getInputFormat"));
            if (AMediaFormat* input = inputFormatOf ? inputFormatOf(m_codec) : nullptr) {
                int32_t stride = 0;
                int32_t slice = 0;
                if (AMediaFormat_getInt32(input, "stride", &stride) && stride >= m_width) m_stride = stride;
                if (AMediaFormat_getInt32(input, "slice-height", &slice) && slice >= m_height) m_sliceHeight = slice;
                AMediaFormat_delete(input);
            }

            m_yuv.assign(static_cast<size_t>(m_stride) * m_sliceHeight * 3 / 2, 0);
            m_lastPtsUs = -1;

            if (m_audioRate > 0 && !openAudio()) m_audioRate = 0;
            return geode::Ok();
        }

        void writeAudio(const float* stereo, size_t frames) override {
            if (!m_audioCodec || frames == 0) return;

            size_t done = 0;
            while (done < frames) {
                const ssize_t slot = AMediaCodec_dequeueInputBuffer(m_audioCodec, kDequeueTimeoutUs);
                if (slot < 0) {
                    drainEncoded(m_audioCodec, m_audioTrack, false);
                    drainEncoded(m_codec, m_videoTrack, false);
                    continue;
                }

                size_t capacity = 0;
                uint8_t* target = AMediaCodec_getInputBuffer(m_audioCodec, static_cast<size_t>(slot), &capacity);
                const size_t chunk = target ? std::min(frames - done, capacity / 4) : 0;
                if (chunk == 0) {
                    AMediaCodec_queueInputBuffer(m_audioCodec, static_cast<size_t>(slot), 0, 0, 0, 0);
                    break;
                }

                auto* pcm = reinterpret_cast<int16_t*>(target);
                const float* source = stereo + done * 2;
                for (size_t index = 0; index < chunk * 2; ++index) {
                    const long value = std::lround(source[index] * 32767.f);
                    pcm[index] = static_cast<int16_t>(std::clamp<long>(value, -32768, 32767));
                }

                const int64_t timestampUs = m_audioFrames * 1000000LL / m_audioRate;
                AMediaCodec_queueInputBuffer(m_audioCodec, static_cast<size_t>(slot), 0, chunk * 4, static_cast<uint64_t>(timestampUs), 0);
                m_audioFrames += static_cast<int64_t>(chunk);
                done += chunk;
                drainEncoded(m_audioCodec, m_audioTrack, false);
            }
        }

        void write(const std::vector<uint8_t>& topDownRgba, int64_t ptsUs) override {
            if (!m_codec) return;

            convertToSemiPlanar(topDownRgba);

            for (;;) {
                const ssize_t slot = AMediaCodec_dequeueInputBuffer(m_codec, kDequeueTimeoutUs);
                if (slot >= 0) {
                    size_t capacity = 0;
                    uint8_t* target = AMediaCodec_getInputBuffer(m_codec, static_cast<size_t>(slot), &capacity);
                    const size_t bytes = std::min(capacity, m_yuv.size());
                    if (target) std::memcpy(target, m_yuv.data(), bytes);
                    const int64_t timestampUs = std::max(ptsUs, m_lastPtsUs + 1);
                    AMediaCodec_queueInputBuffer(m_codec, static_cast<size_t>(slot), 0, bytes, static_cast<uint64_t>(timestampUs), 0);
                    m_lastPtsUs = timestampUs;
                    drainEncoded(m_codec, m_videoTrack, false);
                    return;
                }
                drainEncoded(m_codec, m_videoTrack, false);
            }
        }

        void close() override {
            if (m_codec) {
                const int64_t endUs = std::max<int64_t>(m_lastPtsUs, 0) + 1000000LL / m_fps;
                signalEndOfStream(m_codec, m_videoTrack, endUs);
            }
            if (m_audioCodec) {
                const int64_t endUs = m_audioFrames * 1000000LL / std::max(m_audioRate, 1);
                signalEndOfStream(m_audioCodec, m_audioTrack, endUs);
            }
            if (m_codec) {
                AMediaCodec_stop(m_codec);
                AMediaCodec_delete(m_codec);
                m_codec = nullptr;
            }
            if (m_audioCodec) {
                AMediaCodec_stop(m_audioCodec);
                AMediaCodec_delete(m_audioCodec);
                m_audioCodec = nullptr;
            }
            if (m_muxer) {
                if (!m_muxerStarted && m_videoTrack >= 0) startMuxer();
                if (m_muxerStarted) AMediaMuxer_stop(m_muxer);
                AMediaMuxer_delete(m_muxer);
                m_muxer = nullptr;
                m_muxerStarted = false;
            }
            if (m_fileDescriptor >= 0) {
                ::close(m_fileDescriptor);
                m_fileDescriptor = -1;
            }
        }

    private:
        geode::Result<> failOpen(const char* reason) {
            close();
            return geode::Err(reason);
        }

        bool openAudio() {
            m_audioCodec = AMediaCodec_createEncoderByType("audio/mp4a-latm");
            if (!m_audioCodec) return false;

            AMediaFormat* format = AMediaFormat_new();
            AMediaFormat_setString(format, "mime", "audio/mp4a-latm");
            AMediaFormat_setInt32(format, "sample-rate", m_audioRate);
            AMediaFormat_setInt32(format, "channel-count", 2);
            AMediaFormat_setInt32(format, "bitrate", 192000);
            AMediaFormat_setInt32(format, "aac-profile", 2);
            AMediaFormat_setInt32(format, "max-input-size", 16384);

            const media_status_t configured = AMediaCodec_configure(m_audioCodec, format, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
            AMediaFormat_delete(format);
            if (configured == AMEDIA_OK && AMediaCodec_start(m_audioCodec) == AMEDIA_OK) {
                m_audioFrames = 0;
                return true;
            }

            AMediaCodec_delete(m_audioCodec);
            m_audioCodec = nullptr;
            return false;
        }

        void startMuxer() {
            if (m_muxerStarted || !m_muxer) return;
            AMediaMuxer_start(m_muxer);
            m_muxerStarted = true;
            for (auto& sample : m_pending) {
                sample.info.offset = 0;
                AMediaMuxer_writeSampleData(m_muxer, static_cast<size_t>(sample.track), sample.data.data(), &sample.info);
            }
            m_pending.clear();
        }

        void startMuxerIfReady() {
            if (m_muxerStarted || m_videoTrack < 0) return;
            if (m_audioCodec && m_audioTrack < 0) return;
            startMuxer();
        }

        void emitSample(ssize_t track, const uint8_t* encoded, const AMediaCodecBufferInfo& info) {
            if (m_muxerStarted) {
                AMediaMuxer_writeSampleData(m_muxer, static_cast<size_t>(track), encoded, &info);
                return;
            }
            PendingSample sample;
            sample.track = track;
            sample.info = info;
            sample.data.assign(encoded + info.offset, encoded + info.offset + info.size);
            m_pending.push_back(std::move(sample));
        }

        void signalEndOfStream(AMediaCodec* codec, ssize_t& track, int64_t endUs) {
            for (int attempt = 0; attempt < 200; ++attempt) {
                const ssize_t slot = AMediaCodec_dequeueInputBuffer(codec, kDequeueTimeoutUs);
                if (slot >= 0) {
                    AMediaCodec_queueInputBuffer(codec, static_cast<size_t>(slot), 0, 0, static_cast<uint64_t>(endUs), AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
                    break;
                }
                drainEncoded(codec, track, false);
            }
            for (int attempt = 0; attempt < 500 && !drainEncoded(codec, track, true); ++attempt) {
            }
        }

        bool drainEncoded(AMediaCodec* codec, ssize_t& track, bool waiting) {
            if (!codec) return false;
            AMediaCodecBufferInfo info;
            for (;;) {
                const ssize_t slot = AMediaCodec_dequeueOutputBuffer(codec, &info, waiting ? kDequeueTimeoutUs : 0);
                if (slot == AMEDIACODEC_INFO_TRY_AGAIN_LATER) return false;
                if (slot == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
                    AMediaFormat* output = AMediaCodec_getOutputFormat(codec);
                    track = AMediaMuxer_addTrack(m_muxer, output);
                    AMediaFormat_delete(output);
                    startMuxerIfReady();
                    continue;
                }
                if (slot < 0) continue;

                if (info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) info.size = 0;
                if (info.size > 0 && track >= 0) {
                    size_t capacity = 0;
                    uint8_t* encoded = AMediaCodec_getOutputBuffer(codec, static_cast<size_t>(slot), &capacity);
                    if (encoded) emitSample(track, encoded, info);
                }
                const bool finished = (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0;
                AMediaCodec_releaseOutputBuffer(codec, static_cast<size_t>(slot), false);
                if (finished) return true;
            }
        }

        void convertRowPairs(const uint8_t* rgba, int firstPair, int lastPair) {
            uint8_t* lumaPlane = m_yuv.data();
            uint8_t* chromaPlane = m_yuv.data() + static_cast<size_t>(m_stride) * m_sliceHeight;
            const size_t rowBytes = static_cast<size_t>(m_width) * 4;

            for (int pair = firstPair; pair < lastPair; ++pair) {
                const int topRow = pair * 2;
                const uint8_t* upper = rgba + static_cast<size_t>(topRow) * rowBytes;
                const uint8_t* lower = upper + rowBytes;
                uint8_t* lumaUpper = lumaPlane + static_cast<size_t>(topRow) * m_stride;
                uint8_t* lumaLower = lumaUpper + m_stride;
                uint8_t* chromaRow = chromaPlane + static_cast<size_t>(pair) * m_stride;

                for (int column = 0; column < m_width; column += 2) {
                    const uint8_t* a = upper + column * 4;
                    const uint8_t* b = a + 4;
                    const uint8_t* c = lower + column * 4;
                    const uint8_t* d = c + 4;

                    lumaUpper[column] = lumaOf(a);
                    lumaUpper[column + 1] = lumaOf(b);
                    lumaLower[column] = lumaOf(c);
                    lumaLower[column + 1] = lumaOf(d);

                    const int red = (a[0] + b[0] + c[0] + d[0] + 2) >> 2;
                    const int green = (a[1] + b[1] + c[1] + d[1] + 2) >> 2;
                    const int blue = (a[2] + b[2] + c[2] + d[2] + 2) >> 2;
                    chromaRow[column] = clampByte(((m_matrix.cbr * red + m_matrix.cbg * green + m_matrix.cbb * blue + 32768) >> 16) + 128);
                    chromaRow[column + 1] = clampByte(((m_matrix.crr * red + m_matrix.crg * green + m_matrix.crb * blue + 32768) >> 16) + 128);
                }
            }
        }

        uint8_t lumaOf(const uint8_t* pixel) const {
            return clampByte(((m_matrix.yr * pixel[0] + m_matrix.yg * pixel[1] + m_matrix.yb * pixel[2] + 32768) >> 16) + m_matrix.yOffset);
        }

        void convertToSemiPlanar(const std::vector<uint8_t>& rgba) {
            const int pairs = m_height / 2;
            const int workers = static_cast<int>(std::clamp(std::thread::hardware_concurrency(), 1u, 4u));
            const int chunk = (pairs + workers - 1) / workers;

            std::vector<std::thread> helpers;
            for (int worker = 1; worker < workers; ++worker) {
                const int first = worker * chunk;
                const int last = std::min(pairs, first + chunk);
                if (first >= last) break;
                helpers.emplace_back([this, &rgba, first, last] { convertRowPairs(rgba.data(), first, last); });
            }
            convertRowPairs(rgba.data(), 0, std::min(pairs, chunk));
            for (auto& helper : helpers) helper.join();
        }

        struct PendingSample {
            ssize_t track = -1;
            AMediaCodecBufferInfo info {};
            std::vector<uint8_t> data;
        };

        ColorMatrix m_matrix;
        AMediaCodec* m_codec = nullptr;
        AMediaCodec* m_audioCodec = nullptr;
        AMediaMuxer* m_muxer = nullptr;
        int m_fileDescriptor = -1;
        ssize_t m_videoTrack = -1;
        ssize_t m_audioTrack = -1;
        bool m_muxerStarted = false;
        int m_audioRate = 0;
        int64_t m_audioFrames = 0;
        std::vector<PendingSample> m_pending;
        int m_width = 0;
        int m_height = 0;
        int m_fps = 60;
        int m_stride = 0;
        int m_sliceHeight = 0;
        int64_t m_lastPtsUs = -1;
        std::vector<uint8_t> m_yuv;
    };
}

namespace NXR::Render {
    std::unique_ptr<VideoSink> makeMediaCodecSink() {
        return std::make_unique<MediaCodecSink>();
    }
}
#else
namespace NXR::Render {
    std::unique_ptr<VideoSink> makeMediaCodecSink() {
        return nullptr;
    }
}
#endif
