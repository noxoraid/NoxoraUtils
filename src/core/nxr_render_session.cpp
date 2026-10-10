#include "nxr_render_session.hpp"
#include <chrono>
#include <thread>
#include <cctype>
#include <ctime>
#include <string>
#include "nxr_config.hpp"
#include "nxr_render_keys.hpp"

namespace {
    constexpr size_t kMaxPendingFrames = 8;
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
        sinkConfig.bitrateMode = settings.bitrateMode;
        sinkConfig.profileHigh = settings.profileHigh;
        sinkConfig.bt709 = settings.bt709;
        sinkConfig.fullRange = settings.fullRange;
        sinkConfig.encoder = settings.encoder;
        sinkConfig.audioSampleRate = settings.audio ? AudioTap::get().probeSampleRate() : 0;

        auto started = makeVideoSink();
        if (!started) return geode::Err("Video recording is not available on this platform");

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
        m_steps = 0;
        m_costEma = 1.0 / std::max(settings.fps, 1);
        m_paceValid = false;
        m_audioActive = sinkConfig.audioSampleRate > 0 && AudioTap::get().attach(settings.audioOffsetMs);
        m_encodeThread = std::thread([this] { encodeLoop(); });
        m_active.store(true);
        return geode::Ok(outputPath);
    }

    void GameplayVideoSession::armTail(int frames) {
        m_tailFrames = std::max(frames, 1);
        m_tailArmed = true;
    }

    void GameplayVideoSession::syncRecordingClock() {
        const bool advancing = isAdvancing();
        if (!advancing) m_paceValid = false;
        if (!m_audioActive) return;
        AudioTap::get().setGate(advancing);
    }

    void GameplayVideoSession::paceFrame() {
        using Clock = std::chrono::steady_clock;
        const auto now = Clock::now();
        if (m_paceValid) {
            const double work = std::min(std::chrono::duration<double>(now - m_paceMark).count(), 0.25);
            m_costEma = m_costEma * 0.92 + work * 0.08;
            // Never faster than real time, never faster than the recent average cost, so
            // a slow phone shows an even slow motion instead of fast and slow bursts.
            const double target = std::max(m_costEma * 1.1, 1.0 / frameRate());
            if (work < target) std::this_thread::sleep_for(std::chrono::duration<double>(std::min(target - work, 0.05)));
        }
        m_paceMark = Clock::now();
        m_paceValid = true;
    }

    void GameplayVideoSession::captureFromBackBuffer() {
        if (!isAdvancing()) return;

        // Always one video frame per game step, however slow the phone is. The audio mixer
        // is only allowed to produce this step's share of samples, which keeps both in sync.
        const uint64_t step = m_steps++;
        if (m_audioActive) AudioTap::get().grantFrame(step, frameRate());

        auto pixels = takeSpareBuffer();
        if (!m_readback.capture(pixels)) {
            recycle(std::move(pixels));
            return;
        }

        enqueue(std::move(pixels), static_cast<int64_t>(m_framesQueued * 1000000ULL / static_cast<uint64_t>(frameRate())));
        ++m_framesQueued;

        if (m_tailArmed && --m_tailFrames <= 0) {
            finish();
            return;
        }
        paceFrame();
    }

    void GameplayVideoSession::finish() {
        if (!m_active.exchange(false)) return;
        if (m_audioActive) AudioTap::get().detach();

        auto lastFrame = takeSpareBuffer();
        if (m_readback.drain(lastFrame)) {
            const int64_t lastPts = static_cast<int64_t>(m_framesQueued * 1000000ULL / static_cast<uint64_t>(frameRate()));
            enqueue(std::move(lastFrame), lastPts);
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
        uint64_t framesWritten = 0;
        for (;;) {
            QueuedFrame queued;
            {
                std::unique_lock lock(m_queueMutex);
                m_frameReady.wait(lock, [this] { return !m_pending.empty() || m_closing; });
                if (m_pending.empty()) return;
                queued = std::move(m_pending.front());
                m_pending.pop_front();
            }
            m_spaceFreed.notify_one();

            flipRows(queued.pixels);
            m_encoder->write(queued.pixels, queued.ptsUs);
            recycle(std::move(queued.pixels));
            ++framesWritten;

            if (m_audioActive) {
                m_audioScratch.clear();
                AudioTap::get().collect(static_cast<int64_t>((framesWritten + 1) * 1000000ULL / static_cast<uint64_t>(frameRate())), m_audioScratch);
                if (!m_audioScratch.empty()) m_encoder->writeAudio(m_audioScratch.data(), m_audioScratch.size() / 2);
            }
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

    void GameplayVideoSession::enqueue(std::vector<uint8_t>&& pixels, int64_t ptsUs) {
        std::unique_lock lock(m_queueMutex);
        m_spaceFreed.wait(lock, [this] { return m_pending.size() < kMaxPendingFrames || m_closing; });
        m_pending.push_back(QueuedFrame { std::move(pixels), ptsUs });
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
