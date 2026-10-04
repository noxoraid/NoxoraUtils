# v1.4.18

- **Fix:** much stricter level check for playback. Macros now store a fingerprint of every non-decoration object (id and start position) and the macro file format moves to v11. Desync Rescue only repairs drift and swallows deaths while the level fingerprint matches the macro, so an unchanged level replays exactly as recorded, and any added, removed or moved block makes playback follow the game's physics and die. Older macros fall back to the object, solid and hazard counts.
- **Fix:** playback no longer passes through blocks. Playback used to overwrite the player with the recorded position whenever it drifted and swallowed the death, so new blocks or a macro recorded with noclip went straight through even with every noclip off. Desync Rescue now only repairs drift or swallows a death when the level still matches the macro (same object, solid and hazard counts) and the macro was not recorded with noclip. If you change the level, or play a noclip macro with noclip off, playback follows the game's physics and dies on blocks. On an unchanged level Desync Rescue stays on by default, so float desync no longer kills the run.
- **Change:** Frame Window Counter now follows the Frame Window Counter mod's logic, but works on your own NXR macro and runs instead of an imported macro. Every click gets a frame window: how many physics frames earlier or later the same click still survives with the game's own physics. A marker (circle with the window size and the lowest FPS that can hit it) appears on the player at the click, a click sound plays with a pitch that matches the label, and the label list in the top left counts clicks by needed FPS (20 white, 30, 45, 60, 90, 120, 240, 240+ red) with the count animation. While a macro plays, the window is measured from the macro inputs; in record and manual play it is measured after a short delay. Results are kept per level, so the next playback shows markers and counts instantly, and counts follow respawns and checkpoints. Show Counter and Show Markers can be hidden separately, with sound volume, check length, max window, marker size, counter size and height.
- **Fix:** build errors in the new scroll grip and panel scrollbar (const-correctness) on Windows, macOS and Android.
- **Change:** playback now runs on any level, even when the replay Level ID is different. The "Validate Level ID" option is removed and a running replay is no longer stopped when the level changes.
- **Change:** the different-level notice is short and split into two lines (Replay ID and Level ID) instead of one long message.

# v1.4.17

- **Fix:** a replay recorded on one level no longer plays on another. Playback is refused when the Level ID differs (new "Validate Level ID" option in the bot settings, on by default) and a running replay stops with a message if the level changes.
- **Fix:** replays recorded with Noclip no longer keep passing through obstacles after Noclip is turned off. Desync Rescue is skipped for those replays, so deaths are real unless Noclip is on right now.
- **Change:** Noclip now follows the current global setting in every bot mode, including recording and playback. Replays remember if they were recorded with Noclip (shown in the replay info). Playback Death only hides deaths while Noclip is on. Replays saved before this version have no Noclip flag.
- **New:** UI Size goes down to 0.5x (up to 1.5x), with more presets and a value field with a pencil icon. Tap the field to type an exact number.
- **New:** Colors section in Settings: accent color, panel color, gradient on/off, gradient color, gradient direction and Reset Colors. The gradient is used by the panel background, selected buttons, chips and slider fills.
- **Change:** all pop-ups (Select Replay, Replay Browser, replay info, file name, search, confirmations, Gamemode Swapper) use a new dark rounded style that follows the panel colors, with the same buttons and rows as the panel.
- **Fix:** the N logo button no longer sits on top of the open panel. It hides while the panel is open and comes back when it closes.
- **Change:** res cleanup. Removed unused images (closeBtn, infoIcon, settingsBtn, tableArrow, tableClose), centered the UI icons and the NXR logo, and added NXR_uiEdit and NXR_uiInfo.

# v1.4.16

- **New:** the mobile panel is rebuilt as one landscape layout with a tab rail, search bar, status chips, favorites (star on every hack), and side sheets for hack settings and info instead of popups.
- **New:** touch-first controls: segmented choice, log slider with presets and a number pad for manual input, and a color picker (saturation/value, hue, alpha, HEX/RGB, recent, presets, Apply/Cancel).
- **New:** Settings tab with UI size, panel opacity and position, logo size and position, hide logo in levels/editor, keybinds, theme, config reset, import and export. About tab shows version, links, build info and the changelog.
- **Change:** Speedhack uses a log slider with presets, Click Between Frames uses a Low/High/Extreme choice, Super Fast Practice Click uses a delay slider. Config keys are unchanged.
- **Change:** all hack colors use the new color picker. The desktop ImGui UI is not changed and has fallbacks for the new form controls.

# v1.4.15

- **Fix:** Speedhack and Auto Sync Music audio sounding broken and choppy. The pitch is now set only on the background music channel (same way the bot slow mode does it) and only when it actually changes. The global channel is no longer touched.
- **Change:** Auto Sync Music no longer re-seeks the music. It only adjusts the pitch gently from the measured speed and the music drift, with a small dead zone, so it does not warble.

# v1.4.14

- **Fix:** overlay button not opening the panel. Touch handling is back to exactly how it worked before v1.4.12. The only change kept is that the hidden state is no longer saved to the config.

# v1.4.13

- **Fix:** overlay button not opening the panel. Its touch handler is no longer added and removed manually when it is shown or hidden, it only uses the normal enter/exit registration. Hidden state still is not saved.

# v1.4.12

- **Fix:** the overlay button no longer disappears after restarting the game. Its hidden state was being saved to the config, now it is never saved and it always comes back on the menu.
- **Fix:** Speedhack audio now sets the pitch on the music and global channels every frame.
- **Fix:** Super Fast Practice Click was hooked on a function that does not exist in PlayLayer, so it never ran. It now runs from postUpdate.
- **Change:** Click Between Frames now has 3 modes: Low, High, Extreme (frames and input merged into one hack).
- **New:** Safe Mode shows "It's safe mode" when you finish a level.
- **Fix:** New and Save in the bot no longer wipe an unsaved recording. Save without a file name asks for a name (pre-filled with the level name) and keeps the recording.
- **Change:** Auto Sync Music now also corrects drift against the real music time and re-seeks the music if it drifts more than 250 ms.

# v1.4.11

- **Removed:** the nxr_log_macro folder is no longer created.
- **New:** "Global" tab with Speedhack (0.01 - 1000, gameplay and audio, mod menu unaffected), Safe Mode (wins and progress are not counted), Click Between Frames (built in, no other mod needed), Auto Sync Music (music follows the gameplay when the game lags or slows down) and Super Fast Practice Click (Practice mode, press 1 to spam checkpoints, delay 0 - 10000 ms).
- **New:** Bot "New" now fills the file name with the level name (adds _2, _3 if it already exists). Press OK or edit the name first.
- **New:** Bot "Rename" button to rename any saved replay.
- **Fix:** float inputs now show small values like 0.01 correctly.

# v1.4.10

- **Fix:** Clean popup mode no longer closes when you touch or drag the popup itself on mobile. It now only closes when you tap empty space outside every window.

# v1.4.9

- **Fix:** Wave Trail Fix, the last thin line is now removed completely. Before every trail redraw it drops points closer than 3 units, points that make a sharp U-turn spike, and points that go backwards in X (not in platformer levels), which are what drew the stray line.

# v1.4.8

- **Fix:** Windows, macOS and Android64 build error in the Wave Trail Fix (ambiguous CCPoint assignment).

# v1.4.7

- **Fix:** Wave Trail Fix, thin line that still appeared while sliding on blocks. The trail now merges points closer than 1.5 units and is cleaned before every redraw, and the head segment can no longer have zero length. A corner is only added by the fix when the game has not already placed one right there.

# v1.4.6

- **Fix:** Wave Trail Fix rewritten. It no longer adds a trail point every tick (that made zero-length segments and the thin lines fanning out behind the wave). Now a point is added only at a real corner, when the wave changes direction, at the exact corner position, so the trail stays a clean zigzag in record, playback and normal play. Duplicate, non-finite and stale points are still dropped, and the tracking resets on respawn and quit.

# v1.4.5

- **Fix:** tapping the Table menu (window titles, check boxes, buttons, the open/collapse arrow) no longer sends the tap to the game. Before, the first touch on a menu window could reach the level as a jump, and the release was swallowed by the menu, so the player kept jumping like the screen was held. Now any touch that starts on a Table window, popup or color picker is blocked from the level (and from the editor) from the start to the end. Touches outside the menu still play normally. Applies to Windows, macOS and Android64.
- **Fix:** the **Clean** hack settings popup no longer moves when you drag or scroll inside it. It stays still and only the options scroll. Tapping empty space outside still closes it.
- **Table:** Bot macro pickers are now ImGui windows instead of the Panel popup: **Select Replay**, **Load**, **Delete**, **Export JSON** and both steps of **Merge Replays**. They use the same style as the hack settings window and follow the Hack Settings Popup setting (Popup = title bar with X, Clean = only the list, tap outside to cancel). Tap a replay to select it (tap again to deselect on Select Replay), **i** shows its actions, frames, super frames and TPS, then press the action button or Cancel. Panel is unchanged and still uses its popups.
- **Table:** while a macro picker is open, the menu windows behind it stay visible and usable instead of being hidden.
- **Click Indicator (Line):** player 2 now has its own color. In dual mode the P1 path stays green and the P2 path and its click markers are purple (set in the P2 color option). Hold sections use the same color per player.
- **Click Indicator:** in dual mode, when one tap presses both players, only one click marker is drawn (on P1). When the players click separately (split input, left and right), each player gets its own marker. The same rule applies to the Fadeout boxes, the Perfect labels and the click sound.
- **Click Indicator:** fixed the long stray line under the path. It came from the hidden player 2 in single mode, whose recorded position is stale, being drawn as a real path. Replays now store whether dual mode was active on each frame, so P2 is only drawn while it is really in play. Replays recorded before this version fall back to comparing the X of both players. Path segments between points more than 160 units apart (portal jumps, respawns) are no longer connected.
- **Wave Trail:** fixed the thin line that followed the wave after a record and playback. Two identical trail points in a row made a zero-length segment that the trail drew as a long thin line, and the Wave Trail Fix could add the same point the game had just added. Duplicate and non-finite points are now dropped (controlled by the Wave Trail Fix toggle).
- **Internal:** Wave Straight and Ship Straight now share one implementation (`NXR::Straight::apply`) instead of two copies. Settings and behavior are unchanged. Added explanatory comments to the bot engine and the Table menu, and named the hook priority constants.

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
