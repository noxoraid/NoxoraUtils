#include "nxr_bot.hpp"
#include "nxr_config.hpp"
#include <algorithm>
#include <cstring>
#include <chrono>
#include <ctime>
#include <deque>
#include <limits>

using namespace NXR::Bot;

namespace {
    constexpr char kMagic[4] = {'N', 'X', 'R', '6'};
    constexpr char kV5Magic[4] = {'N', 'X', 'R', '5'};
    constexpr char kV4Magic[4] = {'N', 'X', 'R', '4'};
    constexpr char kV3Magic[4] = {'N', 'X', 'R', '3'};
    constexpr char kV2Magic[4] = {'N', 'X', 'R', '2'};
    constexpr char kLegacyMagic[4] = {'N', 'X', 'R', '1'};

    template <typename T>
    void writeRaw(std::ofstream& out, const T& value) {
        out.write(reinterpret_cast<const char*>(&value), sizeof(T));
    }

    template <typename T>
    bool readRaw(std::ifstream& in, T& value) {
        in.read(reinterpret_cast<char*>(&value), sizeof(T));
        return static_cast<bool>(in);
    }
}

float NXR::Bot::effectiveTps() {
    auto& config = NXRConfig::get();
    if (!config.get<bool>("nxr.player.fps_limiter", false)) return 240.f;
    return static_cast<float>(std::clamp(config.get<int>("nxr.player.fps_limiter::tps", 240), 1, 5000000));
}

std::filesystem::path NXR::Bot::macroPathFor(const std::string& name) {
    return getFolderMacroPath() / (name + ".nxr");
}

std::filesystem::path NXR::Bot::macroJsonPathFor(const std::string& name) {
    return getFolderMacroPath() / (name + ".gdr.json");
}

bool NXR::Bot::loadMacroLegacy(Macro& macro, const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;

    char magic[4];
    in.read(magic, 4);
    if (!in) return false;

    const bool legacy = std::memcmp(magic, kLegacyMagic, 4) == 0;
    const bool v2 = std::memcmp(magic, kV2Magic, 4) == 0;
    const bool v3only = std::memcmp(magic, kV3Magic, 4) == 0;
    const bool v6 = std::memcmp(magic, kMagic, 4) == 0;
    const bool v5only = std::memcmp(magic, kV5Magic, 4) == 0;
    const bool v5 = v5only || v6;
    const bool v4only = std::memcmp(magic, kV4Magic, 4) == 0;
    const bool v4 = v4only || v5;
    const bool v3 = v3only || v4;
    if (!legacy && !v2 && !v3) return false;

    uint32_t nameLen = 0;
    if (!readRaw(in, nameLen)) return false;
    if (nameLen > 4096) return false;

    std::string name(nameLen, '\0');
    in.read(name.data(), nameLen);
    if (!in) return false;

    uint64_t totalFrames = 0;
    if (!readRaw(in, totalFrames)) return false;

    float tps = 240.f;
    if (v3 && !readRaw(in, tps)) return false;

    uint64_t count = 0;
    if (!readRaw(in, count)) return false;
    if (count > 50000000ULL) return false;

    Macro result;
    result.name = std::move(name);
    result.totalFrames = totalFrames;
    result.tps = tps;
    result.events.reserve(static_cast<size_t>(count));

    for (uint64_t i = 0; i < count; i++) {
        InputEvent ev;
        if (legacy) {
            uint64_t frame = 0;
            uint8_t player = 1;
            uint8_t button = 0;
            uint8_t down = 0;
            if (!readRaw(in, frame)) return false;
            if (!readRaw(in, player)) return false;
            if (!readRaw(in, button)) return false;
            if (!readRaw(in, down)) return false;
            if (frame > InputEvent::kMaxFrame) return false;
            ev = InputEvent::make(frame, player, button, down != 0);
        } else {
            if (!readRaw(in, ev.packed)) return false;
        }
        result.events.push_back(ev);
    }

    if (v3) {
        uint64_t fixCount = 0;
        if (!readRaw(in, fixCount)) return false;
        if (fixCount > 50000000ULL) return false;

        result.frames.reserve(static_cast<size_t>(fixCount));
        for (uint64_t i = 0; i < fixCount; i++) {
            MacroFrame row;
            uint32_t frame = 0;
            float p1x = 0.f, p1y = 0.f, p1r = 0.f, p2x = 0.f, p2y = 0.f, p2r = 0.f;
            float p1v = 0.f, p2v = 0.f, p1xv = 0.f, p2xv = 0.f;
            uint32_t p1f = 0, p2f = 0;

            if (!readRaw(in, frame)) return false;
            if (!readRaw(in, p1x)) return false;
            if (!readRaw(in, p1y)) return false;
            if (!readRaw(in, p1r)) return false;
            if (!readRaw(in, p2x)) return false;
            if (!readRaw(in, p2y)) return false;
            if (!readRaw(in, p2r)) return false;
            if (v4) {
                if (!readRaw(in, p1v)) return false;
                if (!readRaw(in, p2v)) return false;
            }
            if (v5) {
                if (!readRaw(in, p1f)) return false;
                if (!readRaw(in, p2f)) return false;
            }
            if (v6) {
                if (!readRaw(in, p1xv)) return false;
                if (!readRaw(in, p2xv)) return false;
            }

            row.frame = frame;
            const float unknown = std::numeric_limits<float>::quiet_NaN();
            row.p1 = {p1x, p1y, p1r, p1v, v6 ? p1xv : unknown, p1f};
            row.p2 = {p2x, p2y, p2r, p2v, v6 ? p2xv : unknown, p2f};
            row.full = v4;
            result.frames.push_back(row);
        }
    }

    char trailing = 0;
    if (in.read(&trailing, 1)) return false;

    result.finalize(true);
    macro = std::move(result);
    return true;
}

std::vector<std::string> NXR::Bot::listMacros() {
    std::vector<std::string> names;
    std::error_code ec;
    auto dir = getFolderMacroPath();
    if (!std::filesystem::is_directory(dir, ec)) return names;

    for (auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        auto path = entry.path();
        if (path.extension() != ".nxr") continue;
        const auto stem = path.stem().string();
        if (stem == "_autosave" || stem.rfind("_backup_", 0) == 0) continue;
        names.push_back(stem);
    }

    std::sort(names.begin(), names.end());
    return names;
}

bool NXR::Bot::deleteMacro(const std::string& name) {
    std::error_code ec;
    return std::filesystem::remove(macroPathFor(name), ec);
}

namespace {
    struct SaveJob {
        NXR::Bot::Macro macro;
        std::filesystem::path path;
    };

    class AsyncWriter {
    public:
        static AsyncWriter& get() {
            static AsyncWriter instance;
            return instance;
        }

        void push(SaveJob job) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                for (auto it = m_queue.begin(); it != m_queue.end();) {
                    if (it->path == job.path) it = m_queue.erase(it);
                    else ++it;
                }
                m_queue.push_back(std::move(job));
                m_pending++;
            }
            ensureThread();
            m_cv.notify_one();
        }

        void flush() {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_idle.wait(lock, [this] { return m_queue.empty() && !m_busy; });
        }

        void shutdown() {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_stop = true;
            }
            m_cv.notify_all();
            if (m_thread.joinable()) m_thread.join();
        }

    private:
        AsyncWriter() = default;

        ~AsyncWriter() {
            shutdown();
        }

        void ensureThread() {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_started) return;
            m_started = true;
            m_thread = std::thread([this] { run(); });
        }

        void run() {
            for (;;) {
                SaveJob job;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_cv.wait(lock, [this] { return m_stop || !m_queue.empty(); });
                    if (m_queue.empty()) {
                        if (m_stop) return;
                        continue;
                    }
                    job = std::move(m_queue.front());
                    m_queue.pop_front();
                    m_busy = true;
                }

                NXR::Bot::saveMacro(job.macro, job.path);

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_busy = false;
                    if (m_pending > 0) m_pending--;
                }
                m_idle.notify_all();
            }
        }

        std::mutex m_mutex;
        std::condition_variable m_cv;
        std::condition_variable m_idle;
        std::deque<SaveJob> m_queue;
        std::thread m_thread;
        size_t m_pending = 0;
        bool m_started = false;
        bool m_busy = false;
        bool m_stop = false;
    };
}

void NXR::Bot::saveMacroAsync(Macro macro, std::filesystem::path path) {
    AsyncWriter::get().push(SaveJob{std::move(macro), std::move(path)});
}

void NXR::Bot::flushAsyncSaves() {
    AsyncWriter::get().flush();
}

bool NXR::Bot::hasBackup(const std::string& name) {
    std::error_code ec;
    return std::filesystem::exists(macroPathFor(name), ec);
}

bool NXR::Bot::restoreBackup(const std::string& name) {
    Macro macro;
    if (!loadMacro(macro, macroPathFor(name))) return false;
    auto& st = State::get();
    st.stop();
    st.current = std::move(macro);
    st.resetRun();
    return true;
}

std::string NXR::Bot::backupNameNow(const std::string& reason) {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
    return std::string("_backup_") + buf + "_" + reason;
}

void NXR::Bot::pruneBackups(size_t keep) {
    std::error_code ec;
    auto dir = getFolderMacroPath();
    if (!std::filesystem::is_directory(dir, ec)) return;

    std::vector<std::filesystem::path> backups;
    for (auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        const auto& p = entry.path();
        if (p.extension() != ".nxr") continue;
        if (p.stem().string().rfind("_backup_", 0) != 0) continue;
        backups.push_back(p);
    }

    std::sort(backups.begin(), backups.end());
    if (backups.size() <= keep) return;

    for (size_t i = 0; i + keep < backups.size(); i++) {
        std::filesystem::remove(backups[i], ec);
    }
}

void NXR::Bot::cleanupOrphanTemps() {
    std::error_code ec;
    auto dir = getFolderMacroPath();
    if (!std::filesystem::is_directory(dir, ec)) return;

    for (auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".tmp") continue;
        std::filesystem::remove(entry.path(), ec);
        ec.clear();
    }
}
