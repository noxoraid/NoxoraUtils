# NXR
NXR is a glass themed mod menu with four selectable themes for Geometry Dash on Windows, macOS and Android 64-bit.

## Features

### Bot
- Record and Playback rebuilt: every tick stores frame, p1, p2, down, hold, and a full-state super frame is stored on a fixed interval and at practice checkpoints. Playback forces the recorded state on every tick, so it stays exact at any FPS or frame
- Click Indicator: shows every click of the replay during playback, with Zone Follow and Line effects, a Mirror HUD (hideable), custom colors, fade time and slide distance
- Autosave: saves in the background every few seconds and when the level is completed, keeps timestamped backups
- Strict record: noclip is ignored while recording and dead attempts are cut back to the last checkpoint, so only the surviving run is saved
- Ignore Inputs (on by default) and replay speed
- Practice Fixes: full player state is saved and restored on every practice checkpoint
- Fix Random: random triggers are seeded per frame so record and playback match
- Replay tools: New, Save, Load, Delete, Restore Last Autosave, Open Replays Folder

### Player
- Noclip (tint on death, per-player toggle, accuracy and death limits)
- Easy Straight: tap quickly to lock Ship or Wave to a real straight line, hold to move normally, independent of FPS
- Unlock All Icon & Color
- FPS Limiter

### Utils
- Auto Clicker (per-player Hold/Release)
- Ship Straight, Wave Straight, UFO Straight

### Level
- Auto Pick Up Coin (collects every coin, wherever it is)
- Gamemode Swapper (any gamemode, speed and platformer mode)
- Show Coin (long lines to every coin, custom color)
- Startpos Switcher (< N > indicator)
- Layout Mode

### Menu
- Two layouts, switch in Settings > Menu Layout: **Panel** (tabs in one popup, default on Android) and **Table** (one draggable, collapsible, scrollable window per tab, default on Windows and macOS). Table works with mouse and touch, tap a row to toggle, tap the corner triangle for its settings, hold a row for its description, mouse wheel scrolls
- Open Menu keybind (Tab by default) on top of the floating NXR button, you can change it in Settings > Open Menu Key
- Toggle Style in Settings: **Switch** (slider toggle, the original) or **Checkbox** (check mark, tap once = on, tap again = off). Works in both layouts
- Table Scale, positions and collapsed state are remembered

### Settings
- New Settings tab in the mobile menu
- Set a keybind for every feature in every tab
- Set a keybind for every bot action: Disabled, Record, Playback, Previous Replay, Next Replay, Save Replay
- Startpos Previous and Next and Toggle UI are listed too
- Tap a key button, then press a key on your keyboard
- Press Esc or tap Clear to remove a key
- Cleared keys are now saved and no longer come back after a restart

### Keybinds and Theme Editor
- Keybinds Mode to bind any hack to a keyboard key
- Theme Editor and layout refresh

## Update Notes

### UI
- Fixed the Replays selector going outside the box in the Bot tab
- Scrolling on mobile no longer presses buttons: drag from anywhere, including on top of a button, and the press is cancelled once your finger moves
- Applies to the tabs, the sidebar and the popups

### Bot
- Hold state of the jump button is saved and restored with every checkpoint
- After loading a checkpoint the macro is synced with the button you are really holding, so ship and wave gaps no longer desync
- Release events are no longer written one frame early after a checkpoint
- Playback now uses the replay in memory when available and waits for pending saves, so an old file is not played by mistake
- Starting Record keeps the selected replay name
- Unknown checkpoints fall back to a full restart instead of a wrong frame
- Checkpoint data is pruned so it does not grow forever
- Player 2 control flip is ignored in platformer levels
- Random triggers are seeded per frame
- New toggles: Practice Fixes and Fix Random (both on by default, keep them the same for record and playback)

### Keybinds
- Actions now have labels and are listed in one place
- Empty keybinds are saved correctly

## Controls
- Mobile: tap the floating NXR button
- Mobile: swipe anywhere in a tab to scroll
-
- Utils: Easy Black Orb and Easy Dash Orb (pink and green) auto-hit the orb, the click is stored in the macro so playback hits it too
- Utils: Precise Fly limits Ship and UFO vertical speed for tight gaps
- Replay browser import now overwrites a same-name replay (old one is kept as a backup) instead of creating "_imported"
- Less lag with auto clicker: click sound, Perfect labels and indicator drawing are throttled
