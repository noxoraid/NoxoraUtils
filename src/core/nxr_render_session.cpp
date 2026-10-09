#include "nxr_render_session.hpp"
#include <chrono>
#include <cctype>
#include <ctime>
#include <string>
#include "nxr_config.hpp"
#include "nxr_render_keys.hpp"

namespace {
    constexpr size_t kMaxPendingFrames = 4;
    constexpr size_t kMaxSpareFrames = 6;

    std::string safeLevelName() {
        auto* layer = PlayLayer::get();
        if (!layer || !layer->m_level) return "gameplay";

        std::string name = layer->m_level->m_levelName;
        for (char& letter : name) {
            if (!std::isalnum(static_cast<unsigned char>(letter))) letter = '_';
        }
        return name.empty() ? std::string("gameplay") : name;
    }

    std::string timestampLabel() {
        const std::time_t now = std::time(nullptr);
        std::tm local {};
#ifdef _WIN32
        localtime_s(&local, &now);
#else
        localtime_r(&now, &local);
#endif
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &local);
        return buffer;
    }
}

namespace NXR::Render {
    GameplayVideoSession::GameplayVideoSession() = default;

    GameplayVideoSession::~GameplayVideoSession() {
        if (m_finalizeThread.joinable()) m_finalizeThread.join();
        if (m_encodeThread.joinable()) m_encodeThread.join();
    }

    GameplayVideoSession& GameplayVideoSession::get() {
        static GameplayVideoSession instance;
        return instance;
    }

    bool GameplayVideoSession::isAdvancing() const {
        if (!m_active.load(std::memory_order_relaxed)) return false;
        auto* layer = PlayLayer::get();
        return layer && !layer->m_isPaused;
    }

    void GameplayVideoSession::joinFinalizer() {
        if (m_finalizeThread.joinable()) m_finalizeThread.join();
    }

    geode::Result<std::filesystem::path> GameplayVideoSession::begin(const VideoSettings& settings) {
        if (m_active.load()) return geode::Err("Already recording");
        if (m_saving.load()) return geode::Err("Still saving the previous video");
        auto* director = cocos2d::CCDirector::sharedDirector();
        auto* view = director ? director->getOpenGLView() : nullptr;
        if (!view) return geode::Err("No render view available");

        const auto frame = view->getFrameSize();
        const int width = static_cast<int>(frame.width) & ~1;
        const int height = static_cast<int>(frame.height) & ~1;
        if (width < 16 || height < 16) return geode::Err("Render size is too small");

        joinFinalizer();

        const auto folder = getFolderRenderPath();
        std::error_code folderError;
        std::filesystem::create_directories(folder, folderError);
        if (folderError) return geode::Err(fmt::format("Cannot create {}", folder.string()));

        const auto outputPath = folder / fmt::format("{}_{}.mp4", safeLevelName(), timestampLabel());

        SinkConfig sinkConfig;
        sinkConfig.outputPath = outputPath;
        sinkConfig.width = width;
        sinkConfig.height = height;
        sinkConfig.fps = settings.fps;
        sinkConfig.bitrateMbps = settings.bitrateMbps;
        sinkConfig.audioSampleRate = AudioTap::get().probeSampleRate();

        auto started = makeMediaCodecSink();
        if (!started) return geode::Err("Video recording is only available on Android");

        auto opened = started->open(sinkConfig);
        if (opened.isErr()) return geode::Err(opened.unwrapErr());

        m_encoder = std::move(started);
        m_settings = settings;
        m_width = width;
        m_height = height;
        m_outputPath = outputPath;
        m_framesQueued = 0;
        m_tailFrames = 0;
        m_tailArmed = false;
        m_pending.clear();
        m_closing = false;
        m_readback.prepare(width, height);
        m_audioActive = sinkConfig.audioSampleRate > 0 && AudioTap::get().attach();
        m_encodeThread = std::thread([this] { encodeLoop(); });
        m_active.store(true);
        return geode::Ok(outputPath);
    }

    void GameplayVideoSession::armTail(int frames) {
        m_tailFrames = std::max(frames, 1);
        m_tailArmed = true;
    }

    void GameplayVideoSession::captureFromBackBuffer() {
        if (!isAdvancing()) return;

        auto pixels = takeSpareBuffer();
        if (!m_readback.capture(pixels)) {
            recycle(std::move(pixels));
            return;
        }

        enqueue(std::move(pixels));
        ++m_framesQueued;
        if (m_audioActive) AudioTap::get().onVideoFrame(m_framesQueued, m_settings.fps);

        if (!m_tailArmed) return;
        if (--m_tailFrames > 0) return;
        finish();
    }

    void GameplayVideoSession::tickAudioGate() {
        if (!m_audioActive) return;
        AudioTap::get().setGate(isAdvancing());
    }

    void GameplayVideoSession::finish() {
        if (!m_active.exchange(false)) return;
        if (m_audioActive) AudioTap::get().detach();

        auto lastFrame = takeSpareBuffer();
        if (m_readback.drain(lastFrame)) {
            enqueue(std::move(lastFrame));
            ++m_framesQueued;
        } else {
            recycle(std::move(lastFrame));
        }
        m_readback.release();

        m_saving.store(true);
        const auto savedPath = m_outputPath;
        const auto savedFrames = m_framesQueued;

        m_finalizeThread = std::thread([this, savedPath, savedFrames] {
            {
                std::lock_guard lock(m_queueMutex);
                m_closing = true;
            }
            m_frameReady.notify_all();
            m_spaceFreed.notify_all();

            if (m_encodeThread.joinable()) m_encodeThread.join();
            m_encoder->close();
            m_encoder.reset();
            m_audioActive = false;

            m_saving.store(false);

            geode::queueInMainThread([savedPath, savedFrames] {
                if (savedFrames == 0) {
                    geode::Notification::create("Nothing was recorded", geode::NotificationIcon::Warning)->show();
                    return;
                }
                geode::Notification::create(fmt::format("Saved {}", savedPath.filename().string()), geode::NotificationIcon::Success)->show();
            });
        });
    }

    void GameplayVideoSession::encodeLoop() {
        uint64_t frameIndex = 0;
        for (;;) {
            std::vector<uint8_t> frame;
            {
                std::unique_lock lock(m_queueMutex);
                m_frameReady.wait(lock, [this] { return !m_pending.empty() || m_closing; });
                if (m_pending.empty()) return;
                frame = std::move(m_pending.front());
                m_pending.pop_front();
            }
            m_spaceFreed.notify_one();

            flipRows(frame);
            m_encoder->write(frame);
            recycle(std::move(frame));

            if (m_audioActive) {
                m_audioScratch.clear();
                AudioTap::get().collect(frameIndex, m_settings.fps, m_audioScratch);
                if (!m_audioScratch.empty()) m_encoder->writeAudio(m_audioScratch.data(), m_audioScratch.size() / 2);
            }
            ++frameIndex;
        }
    }

    void GameplayVideoSession::flipRows(std::vector<uint8_t>& pixels) const {
        const size_t stride = static_cast<size_t>(m_width) * 4;
        for (int top = 0, bottom = m_height - 1; top < bottom; ++top, --bottom) {
            uint8_t* upper = pixels.data() + static_cast<size_t>(top) * stride;
            uint8_t* lower = pixels.data() + static_cast<size_t>(bottom) * stride;
            std::swap_ranges(upper, upper + stride, lower);
        }
    }

    void GameplayVideoSession::enqueue(std::vector<uint8_t>&& pixels) {
        std::unique_lock lock(m_queueMutex);
        m_spaceFreed.wait(lock, [this] { return m_pending.size() < kMaxPendingFrames || m_closing; });
        m_pending.push_back(std::move(pixels));
        lock.unlock();
        m_frameReady.notify_one();
    }

    void GameplayVideoSession::recycle(std::vector<uint8_t>&& pixels) {
        std::lock_guard lock(m_spareMutex);
        if (m_spare.size() < kMaxSpareFrames) m_spare.push_back(std::move(pixels));
    }

    std::vector<uint8_t> GameplayVideoSession::takeSpareBuffer() {
        std::vector<uint8_t> buffer;
        {
            std::lock_guard lock(m_spareMutex);
            if (!m_spare.empty()) {
                buffer = std::move(m_spare.back());
                m_spare.pop_back();
            }
        }
        buffer.resize(static_cast<size_t>(m_width) * static_cast<size_t>(m_height) * 4);
        return buffer;
    }
}
