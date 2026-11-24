# NuClip follow-up

## Recording

- Integrate OBS libobs, or another hook, for games that block Desktop Duplication.
- Capture a specific window without falling back to a full display.
- Keep the replay buffer healthy across sleep, display changes, and device loss. A busy display that belongs to another app still needs a manual Retry.
- Measure CPU and GPU cost while a game is running and tune the default preset from that.
- A lower capture resolution, if it is ever added, belongs with capture options. Quality records the selected display at its native size.

## Audio

- Record a process loopback for the foreground game so system sounds can stay off that track.
- Let the user pick Discord, or another voice app, when it cannot be detected.
- Do not offer separate Discord, game, or application tracks until that capture exists.

## Editor

- Waveforms are one cached image per audio track, drawn after FFmpeg finishes `showwavespic` off the UI thread. A clip with no audio leaves the lane empty.
- Trim drags move the playhead with the handle. The preview follows the handle and drops a seek once the handle has moved on.

## Applications

- Add further games in `src/platform/AppCatalog.cpp`. Match the executable, and a title needle when the executable is shared with a launcher or a runtime.

## Clips and export

- Let preview volume go above 100% the way export already does. The player caps a track at 100%.
- Verify Discord compression against real clips at the 10 MB default and surface a clear result when three attempts are still over the target.
- Add WebM only where the installed FFmpeg build actually encodes it. The current export path uses VP9 and Opus for that format.

## Hotkeys

- Numpad Enter and the main Enter key share one virtual-key code in `RegisterHotKey`, so a binding for one can also match the other.
- Binding Escape itself needs a path that does not treat Esc as cancel.

## Distribution

- Design the branded installer after the recording path stays reliable. Qt Installer Framework is the candidate. Keep it separate from the recording engine.
- Add an update check that talks to the future NuClip site. Do not put that logic in the capture process.
