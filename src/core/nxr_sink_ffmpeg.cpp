#include "nxr_video_sink.hpp"

#ifndef GEODE_IS_ANDROID
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#ifdef GEODE_IS_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace {
    namespace fs = std::filesystem;

    // A child process with an optional stdin pipe. Output of the child goes to a log file.
    class Process {
    public:
        Process() = default;
        Process(const Process&) = delete;
        Process& operator=(const Process&) = delete;
        ~Process() { closeInput(); wait(); }

        bool start(const std::string& exe, const std::vector<std::string>& args, bool withStdin, const fs::path& logPath) {
#ifdef GEODE_IS_WINDOWS
            SECURITY_ATTRIBUTES inherit { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };

            HANDLE stdinRead = nullptr;
            if (withStdin) {
                if (!CreatePipe(&stdinRead, &m_stdinWrite, &inherit, 1 << 20)) return false;
                SetHandleInformation(m_stdinWrite, HANDLE_FLAG_INHERIT, 0);
            } else {
                stdinRead = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, 0, nullptr);
            }
            HANDLE logHandle = CreateFileW(logPath.wstring().c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_ALWAYS, 0, nullptr);

            STARTUPINFOW startup {};
            startup.cb = sizeof(startup);
            startup.dwFlags = STARTF_USESTDHANDLES;
            startup.hStdInput = stdinRead;
            startup.hStdOutput = logHandle;
            startup.hStdError = logHandle;

            std::wstring commandLine = quote(exe);
            for (const auto& arg : args) commandLine += L" " + quote(arg);

            PROCESS_INFORMATION info {};
            const BOOL created = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &info);

            if (stdinRead) CloseHandle(stdinRead);
            if (logHandle != INVALID_HANDLE_VALUE) CloseHandle(logHandle);
            if (!created) {
                if (m_stdinWrite) CloseHandle(m_stdinWrite);
                m_stdinWrite = nullptr;
                return false;
            }
            CloseHandle(info.hThread);
            m_process = info.hProcess;
            return true;
#else
            int pipeEnds[2] = {-1, -1};
            if (withStdin) {
                if (pipe(pipeEnds) != 0) return false;
                fcntl(pipeEnds[1], F_SETFD, FD_CLOEXEC);
#ifdef __APPLE__
                fcntl(pipeEnds[1], F_SETNOSIGPIPE, 1);
#else
                signal(SIGPIPE, SIG_IGN);
#endif
            }

            posix_spawn_file_actions_t actions;
            posix_spawn_file_actions_init(&actions);
            if (withStdin) posix_spawn_file_actions_adddup2(&actions, pipeEnds[0], 0);
            else posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
            const std::string log = logPath.string();
            posix_spawn_file_actions_addopen(&actions, 1, log.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
            posix_spawn_file_actions_adddup2(&actions, 1, 2);

            std::vector<char*> argv;
            argv.push_back(const_cast<char*>(exe.c_str()));
            for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
            argv.push_back(nullptr);

            pid_t pid = 0;
            const int result = posix_spawnp(&pid, exe.c_str(), &actions, nullptr, argv.data(), environ);
            posix_spawn_file_actions_destroy(&actions);

            if (withStdin) {
                close(pipeEnds[0]);
                m_stdinFd = pipeEnds[1];
            }
            if (result != 0) {
                closeInput();
                return false;
            }
            m_pid = pid;
            return true;
#endif
        }

        bool writeInput(const uint8_t* data, size_t size) {
#ifdef GEODE_IS_WINDOWS
            while (size > 0) {
                DWORD written = 0;
                const DWORD chunk = static_cast<DWORD>(std::min<size_t>(size, 1 << 20));
                if (!WriteFile(m_stdinWrite, data, chunk, &written, nullptr) || written == 0) return false;
                data += written;
                size -= written;
            }
            return true;
#else
            while (size > 0) {
                const ssize_t written = ::write(m_stdinFd, data, size);
                if (written < 0) {
                    if (errno == EINTR) continue;
                    return false;
                }
                data += written;
                size -= static_cast<size_t>(written);
            }
            return true;
#endif
        }

        void closeInput() {
#ifdef GEODE_IS_WINDOWS
            if (m_stdinWrite) CloseHandle(m_stdinWrite);
            m_stdinWrite = nullptr;
#else
            if (m_stdinFd >= 0) ::close(m_stdinFd);
            m_stdinFd = -1;
#endif
        }

        // Returns the exit code, or -1 if the process was never started.
        int wait() {
#ifdef GEODE_IS_WINDOWS
            if (!m_process) return m_exitCode;
            WaitForSingleObject(m_process, INFINITE);
            DWORD code = 1;
            GetExitCodeProcess(m_process, &code);
            CloseHandle(m_process);
            m_process = nullptr;
            m_exitCode = static_cast<int>(code);
            return m_exitCode;
#else
            if (m_pid <= 0) return m_exitCode;
            int status = 0;
            while (waitpid(m_pid, &status, 0) < 0 && errno == EINTR) {
            }
            m_pid = -1;
            m_exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
            return m_exitCode;
#endif
        }

    private:
#ifdef GEODE_IS_WINDOWS
        static std::wstring quote(const std::string& text) {
            const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
            std::wstring wide(length > 0 ? static_cast<size_t>(length) - 1 : 0, L'\0');
            if (length > 1) MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), length);

            std::wstring quoted = L"\"";
            size_t backslashes = 0;
            for (wchar_t c : wide) {
                if (c == L'\\') {
                    ++backslashes;
                } else if (c == L'"') {
                    quoted.append(backslashes * 2 + 1, L'\\');
                    quoted += L'"';
                    backslashes = 0;
                } else {
                    quoted.append(backslashes, L'\\');
                    quoted += c;
                    backslashes = 0;
                }
            }
            quoted.append(backslashes * 2, L'\\');
            quoted += L'"';
            return quoted;
        }

        HANDLE m_process = nullptr;
        HANDLE m_stdinWrite = nullptr;
#else
        pid_t m_pid = -1;
        int m_stdinFd = -1;
#endif
        int m_exitCode = -1;
    };

    fs::path logFile() {
        return geode::Mod::get()->getSaveDir() / "ffmpeg_last.log";
    }

    // Runs a short ffmpeg command and returns true when it exits with code 0.
    bool runOk(const std::string& exe, const std::vector<std::string>& args) {
        Process process;
        if (!process.start(exe, args, false, logFile())) return false;
        return process.wait() == 0;
    }

    // Finds a working ffmpeg: the user's own path, the one bundled in the mod, then PATH.
    std::string resolveFfmpeg() {
        static std::mutex lock;
        static std::string cached;
        std::lock_guard guard(lock);
        if (!cached.empty()) return cached;

        std::vector<std::string> candidates;
        const auto custom = geode::Mod::get()->getSettingValue<std::string>("ffmpeg-path");
        if (!custom.empty()) candidates.push_back(custom);

        const auto resources = geode::Mod::get()->getResourcesDir();
#ifdef GEODE_IS_WINDOWS
        candidates.push_back((resources / "ffmpeg.exe").string());
        candidates.push_back("ffmpeg.exe");
#else
        const auto bundled = resources / "ffmpeg_mac";
        std::error_code ignored;
        if (fs::exists(bundled, ignored)) {
            // Archives do not always keep the executable bit.
            fs::permissions(bundled, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add, ignored);
            candidates.push_back(bundled.string());
        }
        candidates.push_back("ffmpeg");
#endif
        for (const auto& candidate : candidates) {
            if (runOk(candidate, {"-hide_banner", "-loglevel", "error", "-version"})) {
                cached = candidate;
                return cached;
            }
        }
        return {};
    }

    // 0 = auto, 1 = CPU, 2 = NVIDIA, 3 = AMD, 4 = Intel, 5 = Apple
    const char* encoderName(int choice) {
        switch (choice) {
            case 1: return "libx264";
            case 2: return "h264_nvenc";
            case 3: return "h264_amf";
            case 4: return "h264_qsv";
            case 5: return "h264_videotoolbox";
            default: return "libx264";
        }
    }

    bool encoderWorks(const std::string& exe, const std::string& name) {
        static std::mutex lock;
        static std::vector<std::pair<std::string, bool>> cache;
        std::lock_guard guard(lock);
        for (const auto& [key, ok] : cache) if (key == name) return ok;

        const bool ok = runOk(exe, {
            "-hide_banner", "-loglevel", "error",
            "-f", "lavfi", "-i", "color=c=black:s=256x256:r=30:d=0.2",
            "-frames:v", "1", "-pix_fmt", "yuv420p", "-c:v", name, "-f", "null", "-"
        });
        cache.emplace_back(name, ok);
        return ok;
    }

    std::string pickEncoder(const std::string& exe, int choice) {
        if (choice != 0) {
            const std::string name = encoderName(choice);
            return encoderWorks(exe, name) ? name : "libx264";
        }
#ifdef GEODE_IS_WINDOWS
        for (const char* name : {"h264_nvenc", "h264_amf", "h264_qsv"}) {
            if (encoderWorks(exe, name)) return name;
        }
#else
        if (encoderWorks(exe, "h264_videotoolbox")) return "h264_videotoolbox";
#endif
        return "libx264";
    }

    void addRateArgs(std::vector<std::string>& args, const std::string& encoder, const NXR::Render::SinkConfig& config) {
        const std::string rate = std::to_string(config.bitrateMbps) + "M";
        const std::string buffer = std::to_string(config.bitrateMbps * 2) + "M";
        const bool constant = config.bitrateMode == 2;

        auto push = [&](std::initializer_list<const char*> items) { for (auto* item : items) args.emplace_back(item); };

        if (encoder == "h264_nvenc") {
            push({"-preset", "p5", "-rc", constant ? "cbr" : "vbr"});
        } else if (encoder == "h264_amf") {
            push({"-quality", "quality", "-rc", constant ? "cbr" : "vbr_peak"});
        } else if (encoder == "h264_qsv") {
            push({"-preset", "medium"});
        } else if (encoder == "libx264") {
            push({"-preset", "fast"});
        }

        args.insert(args.end(), {"-b:v", rate});
        if (constant) args.insert(args.end(), {"-minrate", rate, "-maxrate", rate, "-bufsize", buffer});
        else if (encoder != "h264_videotoolbox") args.insert(args.end(), {"-maxrate", std::to_string(config.bitrateMbps * 3 / 2) + "M", "-bufsize", buffer});

        if (config.profileHigh) args.insert(args.end(), {"-profile:v", "high"});
    }

    // Raw RGBA in, H.264 out. The matrix, range and tags are set from the same settings so
    // the conversion and the flags written into the file always agree.
    std::vector<std::string> buildVideoArgs(const std::string& encoder, const NXR::Render::SinkConfig& config, const fs::path& output) {
        const char* matrix = config.bt709 ? "bt709" : "smpte170m";
        const char* range = config.fullRange ? "pc" : "tv";
        const std::string filter = std::string("scale=trunc(iw/2)*2:trunc(ih/2)*2:flags=accurate_rnd+full_chroma_int:out_color_matrix=")
            + (config.bt709 ? "bt709" : "bt601") + ":out_range=" + (config.fullRange ? "full" : "limited") + ",format=yuv420p";

        std::vector<std::string> args = {
            "-hide_banner", "-loglevel", "error", "-y",
            "-f", "rawvideo", "-pix_fmt", "rgba",
            "-s", std::to_string(config.width) + "x" + std::to_string(config.height),
            "-framerate", std::to_string(config.fps),
            "-i", "-",
            "-vf", filter,
            "-c:v", encoder
        };
        addRateArgs(args, encoder, config);
        args.insert(args.end(), {
            "-g", std::to_string(config.fps),
            "-r", std::to_string(config.fps),
            "-colorspace", matrix, "-color_primaries", matrix, "-color_trc", config.bt709 ? "bt709" : "smpte170m",
            "-color_range", range,
            "-an"
        });
        args.push_back(output.string());
        return args;
    }

    class FfmpegSink final : public NXR::Render::VideoSink {
    public:
        ~FfmpegSink() override { close(); }

        geode::Result<> open(const NXR::Render::SinkConfig& config) override {
            m_config = config;
            m_exe = resolveFfmpeg();
            if (m_exe.empty()) {
                return geode::Err("FFmpeg was not found. Put ffmpeg.exe (Windows) or ffmpeg_mac (macOS) in the mod's res folder, or set the FFmpeg Path setting");
            }

            m_final = config.outputPath;
            m_hasAudio = config.audioSampleRate > 0;
            m_videoOut = m_hasAudio ? fs::path(m_final).replace_extension(".video.tmp.mkv") : m_final;
            m_audioTmp = fs::path(m_final).replace_extension(".audio.tmp.f32");

            std::error_code ignored;
            fs::remove(logFile(), ignored);

            const std::string encoder = pickEncoder(m_exe, config.encoder);
            if (!m_video.start(m_exe, buildVideoArgs(encoder, config, m_videoOut), true, logFile())) {
                return geode::Err("Could not start FFmpeg");
            }
            if (m_hasAudio) {
                m_audio.open(m_audioTmp, std::ios::binary | std::ios::trunc);
                if (!m_audio) m_hasAudio = false;
            }
            m_open = true;
            return geode::Ok();
        }

        void write(const std::vector<uint8_t>& topDownRgba, int64_t) override {
            if (!m_open || m_failed) return;
            if (!m_video.writeInput(topDownRgba.data(), topDownRgba.size())) m_failed = true;
        }

        void writeAudio(const float* interleavedStereo, size_t frames) override {
            if (!m_open || !m_hasAudio || frames == 0) return;
            m_audio.write(reinterpret_cast<const char*>(interleavedStereo), static_cast<std::streamsize>(frames * 2 * sizeof(float)));
        }

        void close() override {
            if (!m_open) return;
            m_open = false;

            m_video.closeInput();
            const int videoExit = m_video.wait();
            if (m_audio.is_open()) m_audio.close();

            std::error_code ignored;
            if (m_hasAudio) {
                if (videoExit == 0) {
                    const bool muxed = runOk(m_exe, {
                        "-hide_banner", "-loglevel", "error", "-y",
                        "-i", m_videoOut.string(),
                        "-f", "f32le", "-ar", std::to_string(m_config.audioSampleRate), "-ac", "2", "-i", m_audioTmp.string(),
                        "-map", "0:v:0", "-map", "1:a:0",
                        "-c:v", "copy", "-c:a", "aac", "-b:a", "192k",
                        "-movflags", "+faststart",
                        m_final.string()
                    });
                    // Keep the audio-less video if muxing failed, so the recording is never lost.
                    if (!muxed) fs::rename(m_videoOut, fs::path(m_final).replace_extension(".silent.mkv"), ignored);
                } else {
                    fs::remove(m_final, ignored);
                }
                fs::remove(m_videoOut, ignored);
                fs::remove(m_audioTmp, ignored);
            }
        }

    private:
        NXR::Render::SinkConfig m_config;
        std::string m_exe;
        fs::path m_final;
        fs::path m_videoOut;
        fs::path m_audioTmp;
        Process m_video;
        std::ofstream m_audio;
        bool m_hasAudio = false;
        bool m_open = false;
        bool m_failed = false;
    };
}

namespace NXR::Render {
    std::unique_ptr<VideoSink> makeFfmpegSink() {
        return std::make_unique<FfmpegSink>();
    }
}
#else
namespace NXR::Render {
    std::unique_ptr<VideoSink> makeFfmpegSink() {
        return nullptr;
    }
}
#endif

namespace NXR::Render {
    std::unique_ptr<VideoSink> makeVideoSink() {
#ifdef GEODE_IS_ANDROID
        return makeMediaCodecSink();
#else
        return makeFfmpegSink();
#endif
    }
}
