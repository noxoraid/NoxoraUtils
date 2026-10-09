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
        settings.bitrateMbps = std::clamp(config.get<int>(NXR::Render::Keys::bitrate, 16), 2, 80);
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
            "Bitrate", NXR::Render::Keys::bitrate, 2.f, 80.f, 16.f, 1.f, NXR::SliderScale::Linear,
            {{"8", 8.f}, {"16", 16.f}, {"32", 32.f}, {"60", 60.f}},
            nullptr, true, " Mbps"
        );
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
