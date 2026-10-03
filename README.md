# NoxoraUtils (NXR)

A mod menu for **Geometry Dash 2.2081** built on the [Geode](https://geode-sdk.org) SDK. It runs on **Windows, macOS and Android 64-bit** from the same codebase. The main feature is a frame-based **macro bot** (record / playback). Around it there is a set of player, utility, level and editor hacks, and a menu with two layouts.

| | |
|---|---|
| Mod ID | `noxorautils.nxr` |
| Geode | 5.10.1 |
| GD version | 2.2081 (win / mac / android) |
| Language | C++20 |
| UI | cocos2d popups (Panel) and Dear ImGui through `gd-imgui-cocos` (Table) |
| Source | https://github.com/noxoraid/NoxoraUtils |

> Many features are cheats (noclip, auto coin, bot, unlock all). Use them offline, in private or in the editor. Don't use them to submit fake records.

---

## Table of contents

1. [How a Geode mod works (short version)](#1-how-a-geode-mod-works-short-version)
2. [Architecture](#2-architecture)
3. [The hack system](#3-the-hack-system)
4. [The bot, in depth](#4-the-bot-in-depth)
5. [The menu](#5-the-menu)
6. [Other hacks, how they work](#6-other-hacks-how-they-work)
7. [Config, keybinds and files on disk](#7-config-keybinds-and-files-on-disk)
8. [Building](#8-building)
9. [Adding a new hack](#9-adding-a-new-hack)
10. [Known limits and troubleshooting](#10-known-limits-and-troubleshooting)
11. [Project layout](#11-project-layout)
12. [Credits and license](#12-credits-and-license)

---

## 1. How a Geode mod works (short version)

Geode gives the mod headers that describe the game's classes (`PlayLayer`, `GJBaseGameLayer`, `PlayerObject`, ...). A mod changes behavior by **hooking** a game function with `$modify`:

```cpp
class $modify(MyPlayLayer, PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        // our code runs first
        PlayLayer::destroyPlayer(player, obj); // call the original (or skip it)
    }
};
```

Everything in NXR is built from this one idea: **hook a function, change its inputs or outputs, optionally call the original.** Noclip skips `destroyPlayer`. The bot hooks the button and tick functions. The menu hooks touch input.

Two Geode details matter in this project:

- **Hook priority.** When several mods hook the same function, priority decides who runs first. NXR sets priorities explicitly where order matters (`GJBaseGameLayer::processCommands` at -30, `PlayerObject::pushButton/releaseButton` at -1000, `PlayLayer::destroyPlayer` at -30).
- **`$execute`** blocks run once at load. NXR uses them to register hacks into the menu.

---

## 2. Architecture

```
            ┌─────────────────────────── src/core ────────────────────────────┐
            │ Hack registry   Config (config3.json)   Keybinds   Bot data/IO   │
            └───────▲───────────────▲───────────────────▲──────────────▲──────┘
                    │               │                   │              │
        ┌───────────┴────┐  ┌───────┴────────┐  ┌───────┴───────┐  ┌───┴──────────┐
        │  src/hacks/*   │  │ src/interface/ │  │ src/interface/│  │ hacks/bot/*  │
        │ player, utils, │  │ cocos (Panel)  │  │ imgui (Table) │  │ engine, gui, │
        │ level, creator │  │ popups, tabs   │  │ windows       │  │ overlay      │
        └────────────────┘  └────────────────┘  └───────────────┘  └──────────────┘
```

- **`src/core`** has no gameplay hooks of its own (apart from a few). It holds the hack registry, the config store, keybinds, the macro data model and file formats, the player-state capture code and shared helpers.
- **`src/hacks/<group>`** has one file per feature. Each file does three things: registers a hack, hooks the game functions it needs, and describes its settings form.
- **`src/interface`** draws the menu. Both layouts read the same registry, so a hack shows up in both without extra work.

All hacks live in one binary. The CMake file globs `src/*.cpp`, so a new `.cpp` file is picked up automatically.

---

## 3. The hack system

### Registry

Hacks are grouped in **windows** (`Bot`, `Player`, `Utils`, `Level`, `Creator`, ...). `NXR::Gui` owns the windows, a `Window` owns its `Hack` objects, and a `Hack` knows:

- an ID, name and description,
- whether it counts as a **cheat** (`cheating` flag, used to track which cheats are active),
- a handler called when it is turned on or off,
- the **hooks** that belong to it,
- an optional **settings form** and a **keybind**.

A feature registers itself at load time:

```cpp
NXR_HACK_CREATE("Player", "Noclip", "The player will be invincible to obstacles", true);
```

### Config keys

State is stored in one flat key/value store (`NXRConfig`, values are `bool`, `int`, `float` or `string`). Keys are derived from the window and hack name:

```
nxr.player.noclip                 → hack on/off
nxr.player.noclip::tint_color     → a setting of that hack (id + "::" + name)
nxr.bot.bot_engine::ignore_input  → bot setting
```

### Hooks follow the toggle

This is the trick that keeps disabled hacks free:

1. A hack registers its hook with `hack.addHookPtr(...)`.
2. That call sets the hook to start **disabled** unless the hack is already on in the config.
3. `Hack::setEnabled(state)` writes the config, flips every registered hook with `hook->toggle(state)`, runs the handler, and updates the active-cheats set.

So a turned-off hack costs nothing per frame, because its hook is not installed. `tryAddHook` / `trySetPriority` (in `nxr_safe_hook.hpp`) wrap this and log a warning instead of crashing when a hook is not available on one platform.

### Settings forms

A hack describes its settings with a small builder:

```cpp
hack.setForm([=](NXR::Form& form) {
    form.addConfigToggle("Tint On Death", tintKey, false);
    form.addConfigIntInput("Opacity", opacityKey, 0, 255, 100);
    form.addConfigColor3Hex("Tint Color", colorKey, "FF0000");
});
```

The same lambda is rendered by the cocos popup (Panel layout) and by an ImGui window (Table layout). The hack author writes it once.

---

## 4. The bot, in depth

The bot is a **frame-based replay system** (not a click-only system). It records the inputs and also the player state, so playback can detect and repair desyncs.

Main files: `core/nxr_bot.hpp` (data model), `core/nxr_macro_io.cpp` (file format), `hacks/bot/nxr_bot_engine.cpp` (record/playback), `core/nxr_player_state.hpp` and `core/nxr_practice_fix.hpp` (state capture).

### 4.1 Frame counter

The engine needs a stable frame number, the same on every run and at any FPS. It hooks `GJBaseGameLayer::processCommands` (once per physics tick, half-ticks are ignored) and derives the frame from the game's own tick counter (`m_gameState.m_currentProgress`), plus a bias.

Because it is not obvious whether the counter is already incremented when `processCommands` runs, the engine **probes it at runtime**: it watches whether the counter moves inside the call, and after a few consistent observations it fixes an offset. The bias is also reset on every checkpoint load, so a restart from a practice checkpoint lands on the right frame.

### 4.2 What gets recorded

A `Macro` holds three lists:

| List | Content | When |
|---|---|---|
| `events` | button down/up: frame, player (1/2), button (1-3) | every input |
| `frames` | one row per tick: position x/y, rotation, y velocity, x velocity (platformer), flags, and a bitmask of held buttons | every tick |
| `supers` | a **full blob** of the player object's fields | frame 1, every 30 frames, and at practice checkpoints |

An input event is packed into 32 bits: 28 bits of frame, 1 bit for player, 2 bits for button, 1 bit for down/up. The max frame is `2^28 - 1`.

A **player-state row** stores position, rotation and velocity plus flags (on ground, upside down, dashing, sliding, on slope, touched pad/ring, jump buffered) and the **game mode bits** (cube, ship, ball, ufo, wave, robot, spider, swing), mini, and speed index.

A **super frame** is a byte blob of every plain-data field in `PlayerObject` (listed by the `NXR_PLAYER_FIELDS` macro). A hash of the field names and sizes (`blobLayout`) is stored in the macro. If you load a macro on a game build where the layout hash differs, super frames are ignored instead of writing garbage into memory.

Inputs are captured in two places: from the **queued buttons** (`processQueuedButtons`) and directly from `PlayerObject::pushButton / releaseButton`. The second one catches clicks that bypass the queue (orb auto-clicks, other mods).

### 4.3 Recording rules

- **Strict recording.** Noclip is ignored while recording. After a death or a reset, `onReset(frame)` **cuts** every event, row and super frame after the checkpoint frame, so only the run that survived is kept.
- A row is not written while a player is dead.
- **Checkpoints.** On `storeCheckpoint` the engine remembers which macro frame the checkpoint belongs to (`checkpointFrames`) and, with **Practice Fixes** on, captures the full player pair. On `loadFromCheckpoint` it restores that state. This is what makes record + practice mode exact. A checkpoint the engine has never seen triggers a full restart instead of a wrong frame.
- **Held buttons.** The hold state is saved with each checkpoint and re-synced with the button you are physically holding after loading it, so ship and wave gaps don't desync.
- **Fix Random.** Random triggers are seeded per frame (`fast_srand(seed(frame))`), in both record and playback. Keep this setting the same for both.

### 4.4 Playback

Each tick, in order:

1. **Inject inputs.** `injectReplayInputs` replays every event whose frame is `<=` the current frame through the game's own `queueButton`, using a flag so the input blocker lets it through.
2. **Block real input** (option *Ignore Inputs*, on by default). The `handleButton` hook drops your real touches while a replay plays.
3. **Apply state.** Before and after `processCommands`, `applyPlayback` looks up the recorded row for that frame:
   - it forces the **game mode, mini size and speed** to match (calling the game's own `toggleFlyMode`, `toggleRollMode`, ... when they differ),
   - it syncs gravity,
   - it compares the real position and y velocity with the row. If the drift is larger than `0.002`, it restores the nearest super frame and writes the recorded state. Otherwise it only corrects rotation if it is off by more than 1 degree,
   - it sets the held-button flags from the row.
4. **Desync rescue.** If the player is about to die while a recorded row exists for that frame, `destroyPlayer` is suppressed and the frame is counted as a rescue. At the end of the level a notification tells you how many frames were fixed and where the first one was. If you want a hard fail instead, turn the option off.

The playback uses the replay already in memory when there is one, and waits for pending saves before reading from disk, so an old file is never played by mistake.

### 4.5 Saving

- Files are saved in a **background thread**. The data is written to `name.nxr.tmp` and then **renamed**, so a crash while saving can't leave a half-written replay.
- **Autosave** runs every few seconds of recording and when the level ends, and keeps timestamped backups (old ones are pruned). *Restore Last Autosave* is in the Bot window.
- On quit, an in-progress recording is saved and the queue is flushed before the layer is destroyed.

### 4.6 File format

Replays are `.nxr` binary files. The first four bytes are a magic string: `NXR9` (current), with `NXR8` and `NXR7` still readable.

```
magic "NXR9"
version string, level name, level id, tps, fps, macro name, total frames, layout hash
events[]   : u64 count, then u32 packed each
frames[]   : u64 count, then per row: frame, p1 state, p2 state, hold mask, full flag
supers[]   : u64 count, then per entry: frame, p1 blob, p2 blob
stats      : optional level stats (u8 present flag + values)
```

*Export JSON* writes the same data as human-readable JSON next to the replay.

### 4.7 Merge

*Merge Replays* combines two macros into a new one:

- **Append** joins macro 2 after macro 1 at a cut frame, shifting the frames of macro 2 (the report shows cut frame, shift, and counts).
- **P1 + P2** takes player 1's inputs from macro 1 and player 2's from macro 2.

### 4.8 Click Indicator

Reads the same replay and draws each click on screen during playback, and also when you play manually with a replay selected. It has Zone Follow and Line effects, a Mirror HUD, colors, fade time and slide distance. Clicks and sounds are throttled so the indicator doesn't cost FPS.

### 4.9 What the bot does *not* do

- It does not make an impossible run possible. It replays a recorded one.
- State repair assumes the same level version and the same GD build. A replay made on another level layout will desync, and the layout hash only protects the super-frame blobs.
- Speedhack (`m_timeWarp` other than 1) is handled by counting frames manually, which is less exact than the normal path.

---

## 5. The menu

There are two layouts. Change them in **Settings → Menu Layout**.

| | Panel | Table |
|---|---|---|
| Technology | cocos2d nodes in a popup | Dear ImGui |
| Default on | Android | Windows, macOS |
| Look | tabs in one popup | one draggable, collapsible, scrollable window per tab |
| Input | touch | mouse and touch |

Common behavior: every on/off control is a check box, positions and collapsed state are saved, a tap on a row toggles it, a tap on the corner triangle opens its settings, holding a row shows its description.

### 5.1 Table layout internals (ImGui)

`gd-imgui-cocos` creates an ImGui context on top of the game's OpenGL context and gives the mod a draw callback each frame. NXR draws all windows there. A few problems had to be solved for touch screens:

- **Touch scroll.** ImGui has no touch scroll. `touchScroll()` turns a finger drag into scroll on the current window, and `tapButton()` only fires a press when the finger lifts without having dragged. This is why scrolling doesn't press buttons.
- **Touch must not reach the game.** The game receives touches through the cocos touch dispatcher, independent of ImGui. The first tap on a menu window used to also reach the level as a jump, while the release was consumed by the menu, so the player kept jumping as if held. Now, every frame, NXR records the on-screen rectangle of each ImGui window (`noteRect()` before every `ImGui::End()`). `UILayer::ccTouchBegan` and `EditorUI::ccTouchBegan` are hooked: if the touch starts inside one of those rectangles, it returns `false` and the game never sees the touch. A touch that starts outside the menu plays normally.
- **Popups.** Hack settings, and the macro pickers in the Bot window, are ImGui windows. In **Popup** mode they have a title bar and an X. In **Clean** mode they have no title bar, are locked in place (`NoMove`, so dragging scrolls the list instead of moving the window) and close when you tap outside.

### 5.2 Panel layout internals (cocos)

Built from `CCMenu`, `CCScale9Sprite` and custom `Popup` subclasses (`NXRMenu`, `NXRHacksTab`, `NXRHacksLayer`, `NXRHackSettingsPopup`). Scrolling cancels a pending button press once the finger moves, for tabs, the sidebar and popups.

### 5.3 Opening the menu

A floating NXR button (mobile) or the **Open Menu key** (Tab by default, changeable). *Keybinds Mode* lets you bind any hack to a key by tapping its row and then pressing the key. Esc or *Clear* removes a binding.

---

## 6. Other hacks, how they work

This is a map of the main ideas. Each hack's own `.cpp` has the details.

### Player

| Hack | Idea |
|---|---|
| **Noclip** | Hooks `PlayLayer::destroyPlayer` and returns early (except for the anti-cheat spike). Per-player toggle, death tint, accuracy tracking, optional limits (min accuracy, max deaths). Turned off automatically while the bot is active. |
| **Easy Straight** | Locks the Ship or Wave at the current height when you tap fast, and releases when you hold. Time is measured in game ticks, not wall clock, so it doesn't depend on FPS. |
| **Show Trajectory** | Creates two "fake" player objects and simulates N steps with the **game's own physics**: one where jump is held, one where it is released. Draws both paths and stops with a hitbox where the player dies. |
| **Show Hitbox** | Draws outlines (or filled boxes) for the player and every object, with separate colors for blocks, spikes, triggers and player. |
| **FPS Limiter** | Hooks `CCDirector::setAnimationInterval` to set the real frame rate, and a `GJBaseGameLayer` hook for the physics **TPS**. FPS 0 means uncapped. TPS 240 is the game default. |
| **Unlock All** | Hooks `GameManager` unlock checks. |

### Utils

| Hack | Idea |
|---|---|
| **Auto Clicker** | Hold/release frames, clicks per second, or clicks by angle. Works per player. |
| **Ship / Wave / UFO Straight** | Manual (you click to stay straight) or Auto (auto-click while staying straight). |
| **Easy Black Orb / Dash Orb** | *No Touch* ignores the orb. *Auto Click* hits it, and the click goes through the bot's input path, so playback also hits it. |

### Level

| Hack | Idea |
|---|---|
| **Auto Pick Up Coin** | Collects every coin from `GJBaseGameLayer`. |
| **Gamemode Swapper** | Adds a button in the pause menu to switch gamemode, speed and platformer mode. |
| **Show Coin** | Draws a line from the player to every coin. |
| **Startpos Switcher** | `<` `N` `>` indicator, plus Q/E on PC. |
| **Layout Mode** | Strips the level down to its gameplay layout. Pick a style, choose what to hide, recolor solids, hazards and interactables, and set opacity. |
| **Nox Utils** | A panel with auto clicker, input flip on death, orb and pad clicks, and extra clicks. |

### Creator (editor)

Level Edit, Free Scroll, Zoom Bypass, Editor Extension, Hide UI, Smooth Editor Trail, Playtest Zoom Bypass, Reset Percent on Save, Custom Object Bypass, Toolbox Button Bypass, Verification Bypass, No (C) Mark, Copy Hack, Macro Buff. Most of these hook `EditorUI`, `EditorPauseLayer`, `LevelEditorLayer` or `LevelInfoLayer` and remove a limit or add a menu entry.

---

## 7. Config, keybinds and files on disk

| File | Where | Content |
|---|---|---|
| `config3.json` | mod save dir | all hack states and settings |
| `keybinds.json` | mod save dir | hack and action keybinds (cleared keys are saved too) |
| `nxr_macro/*.nxr` | see below | replays and backups |

Replay folder:

- **Windows / macOS:** `<mod save dir>/nxr_macro`
- **Android:** `/storage/emulated/0/Android/media/com.geode.launcher/game/nxr_macro` (so you can reach it with a file manager)

The config is loaded at start and saved when the menu closes.

---

## 8. Building

You need the Geode SDK and CLI installed (`GEODE_SDK` set).

```bash
geode build               # desktop, from the project root
```

The Android build needs the Android NDK. The easiest way for all three platforms is **GitHub Actions**: `.github/workflows/build.yml` builds Windows, macOS and Android64 in parallel with `geode-sdk/build-geode-mod`, combines them into one `.geode` file and uploads it. A GitHub release is created when you push a tag.

Dependencies:

- Geode SDK (and its bundled libraries)
- [`gd-imgui-cocos`](https://github.com/matcool/gd-imgui-cocos) (fetched with CPM)
- nlohmann/json (`libs/json.hpp`)

If a build fails, read the first `error:` line in the log. Often it is a hook that does not exist on one platform, or a header that clashes with the `using namespace geode::prelude` that Geode adds. Don't include `imgui_internal.h` in a file that also includes Geode headers, because it makes `log` ambiguous. Use only the public ImGui API.

---

## 9. Adding a new hack

1. Create `src/hacks/<group>/nxr_my_hack.cpp` (CMake will find it).
2. Register it and hook what you need:

```cpp
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_config.hpp"
#include "../../core/nxr_safe_hook.hpp"

NXR_HACK_CREATE("Player", "My Hack", "What it does", false);

class $modify(NXRMyHackPlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        auto& hack = NXR::Gui::get().getWindow("Player").findHackByName("My Hack");
        NXR::tryAddHook(self, hack, "PlayLayer::update");   // hook turns on/off with the hack

        hack.setForm([key = hack.formatAdditionalSetting("amount")](NXR::Form& form) {
            form.addConfigIntInput("Amount", key, 0, 100, 10);
        });
    }

    void update(float dt) {
        PlayLayer::update(dt);
        int amount = NXRConfig::get().get<int>("nxr.player.my_hack::amount", 10);
        // ...
    }
};
```

3. Add a line to `changelog.md`.

Rules that keep the mod stable:

- Always call the original unless you mean to block it.
- Guard against null pointers (`m_player1`, `PlayLayer::get()`).
- Never read a config key without a default.
- If the hack touches inputs, think about what the bot records and replays.

---

## 10. Known limits and troubleshooting

- **Menu tap makes the character jump (old versions).** Fixed in v1.4.5 by the touch rectangles (see 5.1). If you still see it, make sure no other mod hooks `UILayer::ccTouchBegan` at a higher priority.
- **Replay desyncs.** Check that the TPS, FPS limiter, *Practice Fixes* and *Fix Random* are the same for record and playback. Different GD builds or level versions can't be guaranteed.
- **Playback ignores my touch.** That is *Ignore Inputs*. Turn it off in the Bot window if you want to take over.
- **A hack does nothing on one platform.** Look in the Geode log for `NXR: hook '...' not available`. That hook was skipped on purpose.
- **Android can't find replays.** The folder is `Android/media/com.geode.launcher/game/nxr_macro`.
- **Config reset.** Delete `config3.json` while the game is closed.

When reporting a bug, send the Geode log and the steps. For bot desyncs, also send the `.nxr` file.

---

## 11. Project layout

```
mod.json                  mod metadata, resources, fonts
CMakeLists.txt            builds all src/**/*.cpp, links imgui-cocos
changelog.md              shown in-game; update on every change
about.md                  short feature list for the mod page
res/                      sprites, click sound, GoogleSans font
libs/json.hpp             nlohmann/json
.github/workflows/        CI: build 3 platforms, package, release
src/
  core/                   registry, config, keybinds, bot data + IO, state capture, utils
  hacks/
    bot/                  engine (record/playback), gui, overlay, click indicator
    player/               noclip, easy straight, trajectory, hitbox, fps limiter, unlock all
    utils/                auto clicker, straights, easy orbs
    level/                auto coin, gamemode swapper, show coin, startpos, layout mode, nox utils
    creator/              editor hacks
  interface/
    cocos/                Panel layout, popups, overlay button, bot pickers
    imgui/                Table layout (ImGui windows, touch handling)
```

---

## 12. Credits and license

- [Geode](https://geode-sdk.org) for the SDK and the modding platform
- [gd-imgui-cocos](https://github.com/matcool/gd-imgui-cocos) by matcool for ImGui in Geometry Dash
- [Dear ImGui](https://github.com/ocornut/imgui) by Omar Cornut
- [nlohmann/json](https://github.com/nlohmann/json)
- GoogleSans font (`res/GoogleSans-Regular.ttf`)

This project uses the **Noxora Attribution License (NAL) v1.0**. See `LICENSE.txt`.
