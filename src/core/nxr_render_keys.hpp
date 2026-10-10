#pragma once

namespace NXR::Render::Keys {
    inline constexpr const char* window = "Recorder";
    inline constexpr const char* frameRate = "nxr.recorder.fps";
    inline constexpr const char* bitrate = "nxr.recorder.bitrate_mbps";
    inline constexpr const char* bitrateMode = "nxr.recorder.bitrate_mode";
    inline constexpr const char* profile = "nxr.recorder.profile";
    inline constexpr const char* colorMatrix = "nxr.recorder.color_matrix";
    inline constexpr const char* encoder = "nxr.recorder.encoder";
    inline constexpr const char* colorRange = "nxr.recorder.color_range";
    inline constexpr const char* tailSeconds = "nxr.recorder.auto_stop_on_complete::tail";
    inline constexpr const char* autoStopHack = "Auto Stop On Complete";
    inline constexpr const char* hideButtonHack = "Hide Button While Recording";
    inline constexpr const char* audioOffset = "nxr.recorder.audio_offset_ms";
    inline constexpr const char* audioEnabled = "nxr.recorder.audio";
    inline constexpr const char* concealId = "nxr.recording.hidden";
    inline constexpr const char* toggleAction = "nxr.recorder::toggle";
}
