# NuClip

NuClip is a native Windows application for recording a rolling gameplay buffer, saving clips, and preparing them for export. The interface is Qt 6 QML. Capture, encoding, and export run outside the UI thread.

This is a development build. It is not an installer.

## What works

- Dark, light, and system appearance, with accent colors that apply immediately and are saved. The interface uses a viewfinder mark and a narrow icon rail rather than a dashboard of cards. Accent color is reserved for live recording, the primary save action, and the selected trim.
- Persistent settings in `%LOCALAPPDATA%\NuClip\settings.json`.
- Global hotkeys for saving a clip, alternate durations, screenshots, and replay controls. Letters, numbers, symbols, function keys, and modifier-only chords can be assigned. Windows `RegisterHotKey` is used first. A low-level keyboard hook covers chords that API cannot register. Esc cancels capture. A conflicting NuClip shortcut can be replaced from the row.
- A replay buffer and a continuous session recording. Replay starts on launch unless "Automatically start replay when NuClip launches" is turned off in Settings. The buffer stays rolling until a clip or screenshot is saved. A leftover capture from a previous NuClip process is released before the buffer starts. If the display is still busy, the window shows one short reason instead of the capture log.
- Display capture through FFmpeg Desktop Duplication (`ddagrab`) at the selected display's native resolution. Quality presets set the bitrate from that resolution, the frame rate, and the codec. H.264, H.265, and AV1 appear only when a probed encoder can produce them. Auto chooses the encoder for the GPU that owns the captured display: NVIDIA NVENC, AMD AMF, or Intel Quick Sync, then software.
- System loopback audio and microphone capture through WASAPI, stored as separate tracks when both are enabled.
- Clip library with search, filters, rename, delete, and thumbnails.
- A small capture overlay appears on the recorded display when a clip or screenshot is saved, when a continuous recording starts or stops, and when capture fails. It is click-through, stays out of the saved video, and can be moved, sized, and turned off in Settings. Closing the window leaves NuClip in the tray.
- Application names come from one catalog. A known game is shown by its name, such as Roblox or VALORANT, instead of the executable or the raw window title. Unknown apps fall back to the product name, then a cleaned title. Steam, Epic, and other launchers stay out of the way when the game window is the one on screen. A custom name is saved in settings and reused for that application. New clip files use the display name. Existing files are left where they are.
- Non-destructive trim and a second export, including an optional Discord-sized copy that does not replace the full-quality file.
- Editor preview that loads the selected file, shows a frame, and plays each audio track on its own player so mute and volume (up to 100%) are heard before export. Dragging either trim handle seeks that preview to the frame under the handle. The playhead seeks independently. Waveform images are generated in the background and cached beside thumbnails.

## What this build does not do

- OBS libobs and game-hook capture are not integrated. NuClip records a display. If a game blocks desktop capture, this build cannot record that game.
- Separate Discord, game, and per-application audio tracks are not available. System audio is the whole output mix.
- Preview volume above 100% is heard at 100%. The export still uses the higher level.
- Bare Escape cancels shortcut capture, so that key is not assigned from the listener. Ctrl+Alt+Delete stays reserved by Windows.
- Audio meters move only while a recording is running.
- A custom installer is not built yet.

## Requirements

- Windows 10 or Windows 11, 64-bit
- Visual Studio 2022 Build Tools, with the x64 MSVC toolset and the Windows SDK
- CMake 3.21 or newer
- Qt 6.8 for MSVC 2022 (Core, Gui, Widgets, Qml, Quick, QuickControls2, QuickDialogs2, Multimedia, Concurrent, Svg)
- Ninja
- The FFmpeg and FFprobe builds placed at `third_party/ffmpeg/ffmpeg.exe` and `third_party/ffmpeg/ffprobe.exe`

This workspace was developed with Qt `6.8.3` at `C:\Users\Joseph\Qt\6.8.3\msvc2022_64`, Ninja at `third_party/tools/ninja.exe`, and a current BtbN FFmpeg GPL build that includes `ddagrab` plus NVENC, AMF, and Quick Sync encoders for H.264, HEVC, and AV1, along with `libx264`, `libx265`, and `libsvtav1`. NuClip still hides any of those that fail a short encode test on the current PC.

## Build

From a Visual Studio x64 developer prompt:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH=C:\Users\Joseph\Qt\6.8.3\msvc2022_64 -DCMAKE_MAKE_PROGRAM=third_party\tools\ninja.exe
cmake --build build
```

The build copies Qt and FFmpeg beside `build\NuClip.exe`.

## Check a real recording

```bat
build\NuClip.exe --self-test
```

The test records the desktop for a few seconds, saves a clip in a temporary folder, and writes `build\self-test.txt`. A passing run contains `codec_name=h264` and `PASS`. It does not write into the user's Videos folder.

## Layout

`src/core` holds settings, theme, paths, and logging. `src/recording` owns the FFmpeg replay buffer. `src/audio` captures WASAPI into a named pipe. `src/platform` covers hotkeys, monitors, devices, the game-window detector, and the tray. `src/library`, `src/editor`, `src/export`, and `src/storage` cover clips and output. `qml` is the interface.

Clips default to `Videos\NuClip\Clips`. The rolling buffer lives in `%LOCALAPPDATA%\NuClip\buffer` and is discarded when a replay stops.
