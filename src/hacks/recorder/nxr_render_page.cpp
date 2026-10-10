#include <Geode/Geode.hpp>
#include <imgui.h>
#include <algorithm>
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_keybinds.hpp"
#include "../../core/nxr_render_keys.hpp"
#include "../../core/nxr_render_session.hpp"
#include "../../interface/cocos/nxr_ui_page.hpp"

namespace {
    constexpr int kFrameRates[] = {30, 60, 90, 120};

    void announce(const std::string& text, geode::NotificationIcon icon) {
        geode::Notification::create(text, icon)->show();
    }

    NXR::Render::VideoSettings readVideoSettings() {
        auto& config = NXRConfig::get();
        NXR::Render::VideoSettings settings;
        settings.fps = kFrameRates[std::clamp(config.get<int>(NXR::Render::Keys::frameRate, 1), 0, 3)];
        settings.bitrateMbps = std::clamp(config.get<int>(NXR::Render::Keys::bitrate, 16), 2, 120);
        settings.bitrateMode = std::clamp(config.get<int>(NXR::Render::Keys::bitrateMode, 0), 0, 1) == 1 ? 2 : 1;
        settings.profileHigh = config.get<int>(NXR::Render::Keys::profile, 1) == 1;
        settings.bt709 = config.get<int>(NXR::Render::Keys::colorMatrix, 0) == 0;
        settings.fullRange = config.get<int>(NXR::Render::Keys::colorRange, 0) == 1;
        settings.encoder = std::clamp(config.get<int>(NXR::Render::Keys::encoder, 0), 0, 5);
        settings.audio = config.get<bool>(NXR::Render::Keys::audioEnabled, true);
        settings.audioOffsetMs = std::clamp(config.get<int>(NXR::Render::Keys::audioOffset, 0), -300, 300);
        return settings;
    }

    void startRecording() {
        if (!PlayLayer::get()) {
            announce("Open a level first", geode::NotificationIcon::Warning);
            return;
        }

        auto result = NXR::Render::GameplayVideoSession::get().begin(readVideoSettings());
        if (result.isErr()) {
            announce(result.unwrapErr(), geode::NotificationIcon::Error);
            return;
        }
        announce("Recording started", geode::NotificationIcon::Success);
    }

    void stopRecording() {
        auto& session = NXR::Render::GameplayVideoSession::get();
        if (!session.isActive()) {
            announce("Not recording", geode::NotificationIcon::Info);
            return;
        }
        session.finish();
        announce("Saving video...", geode::NotificationIcon::Info);
    }

    void toggleRecording() {
        if (NXR::Render::GameplayVideoSession::get().isActive()) stopRecording();
        else startRecording();
    }
}

$execute {
    auto& window = NXR::Gui::get().getWindow(NXR::Render::Keys::window);

    auto& autoStop = window.createHack(
        NXR::Render::Keys::autoStopHack,
        "Stops and saves the video a few seconds after the level is completed",
        false
    );
    autoStop.setForm([](NXR::Form& form) {
        form.addConfigIntInput("Seconds After Complete", NXR::Render::Keys::tailSeconds, 0, 30, 3);
    });

    window.createHack(
        NXR::Render::Keys::hideButtonHack,
        "Hides the NXR round button while the video is being recorded so it does not show up in the video",
        false
    );

    window.setCustomWindowCocos([](NXR::Kit::PageBuilder& page) {
        page.addPadding(2.f);
        page.addSection("Recording");
        page.addButtons({
            {"Start", [] { startRecording(); }},
            {"Stop & Save", [] { stopRecording(); }}
        });
        page.addText("Open a level, press Start, play, then Stop & Save. The game is rendered on a fixed step, so the video stays smooth even when the phone lags while recording.");
        page.addSection("Quality");
        page.addConfigChoice("Frame Rate", NXR::Render::Keys::frameRate, {"30 FPS", "60 FPS", "90 FPS", "120 FPS"}, 1);
        page.addConfigSlider(
            "Bitrate", NXR::Render::Keys::bitrate, 2.f, 120.f, 16.f, 1.f, NXR::SliderScale::Linear,
            {{"8", 8.f}, {"16", 16.f}, {"32", 32.f}, {"60", 60.f}, {"100", 100.f}},
            nullptr, true, " Mbps"
        );
        page.addConfigChoice("Bitrate Mode", NXR::Render::Keys::bitrateMode, {"Variable (VBR)", "Constant (CBR)"}, 0);
#ifndef GEODE_IS_ANDROID
        page.addConfigChoice("Encoder", NXR::Render::Keys::encoder, {"Auto", "CPU (x264)", "NVIDIA", "AMD", "Intel", "Apple"}, 0);
#endif
        page.addConfigChoice("H.264 Profile", NXR::Render::Keys::profile, {"Device default", "High"}, 1);
        page.addConfigChoice("Color Matrix", NXR::Render::Keys::colorMatrix, {"BT.709 (HD)", "BT.601 (SD)"}, 0);
        page.addConfigChoice("Color Range", NXR::Render::Keys::colorRange, {"Limited (TV)", "Full (PC)"}, 0);
        page.addText("Defaults (BT.709, Limited, High) match what players expect for HD video, so colors stay the same as in the game. If you change Color Matrix or Range, the video is tagged to match. A bitrate above what your phone's encoder supports makes recording fail to start; lower it if that happens.");
        page.addSection("Audio");
        page.addConfigToggle("Record Audio", NXR::Render::Keys::audioEnabled, true);
        page.addText("The game is rendered one step per video frame, so the saved video stays smooth even when the phone lags. The sound is recorded in lockstep with it (music and clicks stay in sync), and the phone speaker is muted while recording.");
        page.addConfigSlider(
            "Audio Offset", NXR::Render::Keys::audioOffset, -300.f, 300.f, 0.f, 1.f, NXR::SliderScale::Linear,
            {{"-100", -100.f}, {"0", 0.f}, {"100", 100.f}},
            nullptr, true, " ms"
        );
        page.addText("Negative moves the sound earlier, positive moves it later.");
        page.addText(fmt::format("Videos are saved to {}", getFolderRenderPath().string()));
    });

    window.setImguiPanel([] {
        const bool recording = NXR::Render::GameplayVideoSession::get().isActive();
        if (ImGui::Button(recording ? "Stop & Save" : "Start Recording")) {
            geode::queueInMainThread([] { toggleRecording(); });
        }
    });

    NXR::Keybinds::get().registerAction(
        NXR::Render::Keys::toggleAction,
        "Recorder: Start / Stop",
        geode::Keybind(),
        [](bool repeat) {
            if (!repeat) toggleRecording();
        }
    );
}
