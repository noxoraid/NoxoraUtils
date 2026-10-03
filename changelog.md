# v1.4.4

- **Table:** new setting **Hack Settings Popup** with two modes. **Popup** is the floating window with title bar, arrow and X like before. **Clean** shows only the options (no title bar, no arrow, no X) and closes when you tap any empty space outside the menu. Tapping inside the popup or on the menu windows does not close it. Table only, Panel is unchanged.
- **Table:** a window now opens and collapses from the arrow, from the title text, or from anywhere on the title bar. A tap toggles it, a drag still moves the window.
- **Keybind:** new **Keybind** tab in Table and Panel. Every hack from every tab is listed automatically (new hacks appear by themselves). Tap the key button, press a key, and that key toggles the hack without opening the menu. Esc or Clear removes it. Saved in keybinds.json.
- **Fix:** a keybind no longer toggles a hack that is disabled.

# v1.4.3

- **UI:** every on/off control in Panel and Table now uses a real check box (rounded square with a check mark) instead of a slider toggle. res/NXR_tableCheckOn.png and res/NXR_tableCheckOff.png were redrawn, and the radio rows use the same boxes. The Toggle Style setting was removed because the check box is the only style now.
- **Table:** no more modal popups. Hack settings (Noclip, FPS Limiter, ...), the color picker, the hold-to-read description, and every Bot popup (select, load, delete, export, browse, new, info, merge) now open as floating windows that can be dragged, collapsed and closed with the X. They close by themselves when you confirm or cancel inside them.
- **Table:** option selectors inside hack settings cycle with one tap instead of opening a popup. The Merge mode question is a floating window with two rows.
- **Panel:** unchanged, it keeps its popups.
- **Fix:** Windows, macOS and Android64 build errors. gd-imgui-cocos is now pulled from its `geode` branch (the `main` branch had `keyDown`, `keyUp` and `insertText` signatures that do not match Geode 5), and the ImGui settings form is held as `NXR::Form&` so the default arguments of `addSeparator`, `addConfigFloatInput` and `addConfigToggle` work again.

# v1.4.2

- **Table:** windows redrawn in the GDH style (dark windows, centered title, collapse arrow on the right). Every hack has an image check box on the left (res/NXR_tableCheckOn.png / NXR_tableCheckOff.png) and a triangle button on the right when it has settings.
- **Table:** Bot now opens as a floating window with Record, Playback and the rest of the controls directly, no more "Bot Panel..." popup. Hack settings (Noclip, FPS Limiter, ...) and UI Settings also open as floating windows next to the row instead of the mobile popup. Tap the triangle again or the X to close them.
- **Table:** windows can now be tapped and dragged while the game is paused. Touches are ignored while a popup is open.
- **Toggle Style:** the Checkbox style now uses the image check boxes from res.
- **Fix:** Windows build error, Zoom Bypass hooks EditorUI::zoomIn / zoomOut instead of the inlined zoomGameLayer.

# v1.4.1

- **Settings:** new **Open Menu Key**. Tap the key button, press a key, and that key opens and closes the menu on Windows and macOS (Tab by default). Esc or Clear removes it. It is saved and does not come back after a restart.
- **Settings:** new **Toggle Style** with two modes: **Switch** (the slider toggle that was already there) and **Checkbox** (a check mark box, tap once = on, tap again = off). It applies to every toggle in the menu, in both Panel and Table layouts. The menu reopens by itself when you change it.
- **Table:** hacks show a check mark box when Toggle Style is Checkbox. The Table Settings window has rows for Layout, Theme, UI Scale, Toggle Style and Reset Window Positions.

# v1.4.0

- **Platforms:** NXR now builds for **Windows, macOS and Android 64-bit**. iOS support was removed, CI builds the three platforms.
- **UI:** new **Table** menu layout, draggable and collapsible windows, one per tab, drawn with cocos2d only so it works the same with mouse and touch. Switch between **Panel** and **Table** in Settings > Menu Layout (Table is the default on desktop, Panel on Android). Table scale, window positions and collapsed state are saved. Tabs with their own controls (Bot, Settings) open them from a row at the top of their window.
- **Keybinds:** new Open Menu action (Tab by default).

# v1.3.3

- **Bot:** new **Desync Rescue** toggle (on by default). If playback would die on a frame the recording survived, that death is a desync by definition, so the player is not killed and is snapped back to the recorded state of that frame. A "Rescue N" counter shows in the Bot tab and a notification at level end lists how many frames were fixed and the first one.
- **Bot:** playback correction tolerance lowered from 0.05 to 0.002 units and Y velocity is now checked too, so drift is fixed before it reaches a tight gap or spike.
- **Bot:** full-state super frames are now stored every 30 frames (was 60) for a more complete resync. Macro files are a bit bigger.

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
