# v1.2.2

- **Bot:** added the **Playback Death** option (on by default). Hitting an obstacle during playback now kills the player and restarts the attempt, so desyncs and macros recorded with noclip are no longer hidden. Turn it off for the old "never die during playback" behaviour.
- **Bot:** fixed a Windows build error caused by hooking an inline function (`GJBaseGameLayer::queueButton`). Input blocking during playback still works through the button queue.
- Synced the version number across `mod.json`, `CMakeLists.txt` and `about.md`.
