# NXR

**NXR** is a feature-rich Geometry Dash mod menu built for Geode.

NXR provides a large collection of gameplay utilities, botting/TAS tools, editor extensions, visual utilities, quality-of-life features, and experimental functions in one unified interface.

> [!WARNING]
> NXR is intended for offline, testing, customization, and experimental use.
> Some features may affect gameplay, level verification, or online functionality.

## ✨ Features

### 🎮 Gameplay

- No-Clip
- Unlock Icons
- FPS Limiter
- Hitbox Display
- Trajectory Display
- Auto Coin
- Layout Mode
- Start Position Switcher
- Hide UI
- Free Scroll
- Playtest Zoom Bypass
- Verified Bypass
- Remove C Mark
- Easy Black Orb
- Easy Dash Orb
- Straight Ship
- Straight Wave
- Straight UFO
- Buff Macro
- Custom Object Bypass
- Zoom Bypass
- Reset Percent on Save
- Toolbox Button Bypass
- Copy Hack
- Level Edit
- Auto Clicker
- CBF Bypass

## 🤖 Botting / TAS

NXR includes an integrated botting and TAS system for recording and replaying inputs.

### Macro System

- Record macro
- Play macro
- Create macro
- Delete macro
- Load macro
- Select macro
- Macro information
- Macro autosave
- Export macro to JSON
- Merge macros
- Record without restarting
- Browse and reply to macros
- Bot speedhack
- Bot menu
- Ignore input
- Always save JSON

The macro system is designed for testing, experimentation, tool-assisted gameplay, and creating reproducible gameplay inputs.

## 🛠️ Editor

NXR adds multiple editor-related utilities and bypasses.

### Editor Features

- Editor extensions
- Smooth editor trail
- Level Edit
- Custom Object Bypass
- Zoom Bypass
- Playtest Zoom Bypass
- Free Scroll
- Toolbox Button Bypass
- Reset Percent on Save
- Hide UI
- Fixed Particle
- Random Fixed

These tools are intended to make level creation, testing, and experimentation more flexible.

## ⚡ Visual & Utility Features

- Click Indicators
- FPS Limiter
- Hitbox
- Trajectory
- Hide UI
- Fixed Particle
- Random Fixed
- Legacy Render option
- Custom fonts
- Utility toolbox
- Gameplay information utilities

## 🧰 Scarlet Utils Integration

NXR integrates functionality from **Scarlet Utils** to provide additional utilities and improve the modding experience.

Special thanks to the developers and contributors of Scarlet Utils.

## 📦 Requirements

NXR requires:

- Geometry Dash
- Geode
- A supported platform/build of Geometry Dash

### Supported Platforms

NXR is intended to support:

- Windows
- macOS
- Android

Platform availability may depend on the current NXR release.

## 📥 Installation

### Using Geode

The recommended way to install NXR is through the Geode ecosystem when an official Geode Index release is available.

1. Install Geode.
2. Open the Geode mod loader.
3. Search for `NXR`.
4. Install the latest available version.
5. Restart Geometry Dash if required.

### Manual Installation

If you are installing a `.geode` file manually:

1. Download the `.geode` file from the GitHub Releases page.
2. Open the Geode mods directory.
3. Place the `.geode` file into the mods folder.
4. Launch Geometry Dash.
5. Open the Geode mod menu.
6. Enable NXR.

## 🚀 Releases

Official releases are published through GitHub Releases.

Each release may contain builds for supported platforms.

Check the **Releases** section of this repository for the latest version.

## 🔧 Development

NXR is built using the Geode SDK.

### Build Requirements

You will generally need:

- Git
- CMake
- Geode SDK
- A supported C++ development environment
- Platform-specific build tools

GitHub Actions is used to automate builds for supported platforms.

## 📂 Project Structure

```text
NoxoraUtils/
├── .github/
│   └── workflows/
│       └── build.yml
│
├── src/
│   └── ...
│
├── include/
│   └── ...
│
├── res/
│   ├── *.png
│   ├── click_indicator.mp3
│   └── GoogleSans-Regular.ttf
│
├── mod.json
├── CMakeLists.txt
├── README.md
└── LICENSE