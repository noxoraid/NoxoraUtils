# v1.3.2

- **Replay Info:** the info popup now shows Mode (Platformer or Normal), total jumps, left/right inputs, max and average CPS, longest and shortest hold, spam taps, and level stats: total objects, orbs, pads, all portals, gravity, gamemode, speed, size, mirror and dual portals, hazards, coins, triggers and start positions. Level stats are stored inside the macro file itself (new NXR9 format, older NXR8 and NXR7 macros still load) and are also written to the JSON export. Macros saved before this version and imports show N/A for them until they are saved again with the level open.
- **Layout Mode:** new Style presets (Custom, Blueprint, Mono, Neon, High Contrast), plus options to hide coins, hazards, ground and background, recolor solids, hazards and orbs/pads/portals, and set solid and decoration opacity.

# v1.3.1

- **Click Indicator:** only two effects remain, Zone Follow and Line. Pulse, Tick, Dots, Hold Tail and Burst were removed, and saved settings that used them fall back to Zone Follow.
- **Theme:** new Theme choice in Settings: Basic, Normal, Medium and Pro. It recolors the menu panels and buttons and applies after the menu is reopened.
- **Player:** added **Easy Straight** for Ship and Wave. Quick taps lock a real straight line at that height, holding longer moves normally again. Timing uses game time instead of FPS. It stays off while Ship Straight or Wave Straight is on.
- **Replay Info:** the info popup in the replay picker now shows level name, level ID, version, actions, clicks (P1 / P2), releases, total and recorded frames, super frames, last frame, real FPS / TPS, duration and file size. It scrolls, and FPS no longer shows a fixed 60.
- **UI:** the arrow beside every mode picker is bigger and easier to tap.

# v1.3.0

- **Platforms:** NXR now targets only Android 64-bit and iOS. Windows and macOS support, the desktop ImGui menu and all desktop-only code were removed, and CI builds Android64 and iOS.
- **UI:** the whole mod menu is now the touch interface only, every hack keeps its settings panel in it.
- **Theme:** every image in `res` was recolored to a purple glass theme.
- **Cleanup:** removed the bundled ImGui library, the `gd-imgui-cocos` dependency and the unused Legacy Render setting.

# v1.2.2

- **Bot:** added the **Playback Death** option (on by default). Hitting an obstacle during playback now kills the player and restarts the attempt, so desyncs and macros recorded with noclip are no longer hidden. Turn it off for the old "never die during playback" behaviour.
- **Bot:** fixed a Windows build error caused by hooking an inline function (`GJBaseGameLayer::queueButton`). Input blocking during playback still works through the button queue.
- Synced the version number across `mod.json`, `CMakeLists.txt` and `about.md`.
