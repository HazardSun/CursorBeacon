# CursorBeacon 0.1.1 - Windows shake-to-find cursor

CursorBeacon makes the real Windows pointer grow when you shake the mouse, then shrink back automatically. It is built for 4K/8K and multi-monitor desktops, with per-monitor DPI and fullscreen suppression.

## Fixed in 0.1.1

- **Long-uptime failure:** an internal timestamp overflow after ~10.6 days of uptime could silently stop detection. Timestamps are now overflow-safe; an 11-day regression test was added.
- **Split-screen resize cursors:** the default now enlarges all 14 system cursor shapes, including `SizeWE`/`SizeNS`, so split-screen borders grow too.
- **Stuck enlarged cursor:** if rendering failed, the previous enlarged cursor could remain. The service now restores it before disabling effects.
- **Restore could hang:** `SPI_SETCURSORS` no longer broadcasts a synchronous setting-change message to every top-level window.
- **Input recovery:** Raw Input is re-registered every 5 seconds, and a timer failure falls back to polling, so lock/unlock, desktop switches and device changes do not disable shaking.

## Highlights

- Progressive enlargement from 1.25x to 4x (configurable up to 8x)
- Holds for 800 ms after shaking stops, then eases back to 1x
- Click any mouse button to restore immediately
- Per-monitor DPI: 4x is 192 px on a 144 DPI display, 128 px at 100%
- Fullscreen apps are suppressed on their own monitor only
- Raw Input events; 10 Hz watchdog; 125 Hz polling only when needed
- Real system cursor via `SetSystemCursor`; no overlay, no input hooks
- Crash-safe restore, `--restore` and `--stop` commands
- No administrator rights, no network, no telemetry

## Download

- `CursorBeacon-0.1.1-win64.zip`
- `CursorBeacon-0.1.1-win64.zip.sha256`

SHA-256:

```text
E2C1ADCCCC6112D127BD08F1421B6FB0512354E1E4C913325366DB6E09076301  CursorBeacon-0.1.1-win64.zip
0D32BFBA508FBD8381596AA96915CA23628FD25FC524BDFBBF8121DCE48FD404  CursorBeacon.exe
```

## Install

1. Download and extract the zip.
2. Run `start-service.ps1`.
3. Shake the mouse back and forth. Stop to shrink.

```powershell
.\start-service.ps1
.\status.ps1
.\stop-service.ps1
```

If the cursor is ever stuck large:

```powershell
.\CursorBeacon.exe --restore
```

## Requirements

- Windows 10 1809+ / Windows 11, x64
- Standard user (no administrator)
- No runtime dependencies

## Important notes

- The binary is unsigned. Verify the SHA-256 above; SmartScreen may warn.
- This is a preview release: no tray icon, settings UI or auto-start yet.
- Fullscreen detection is foreground-window based; background/z-order scanning is planned.
- Apps that draw their own cursor are not affected.
- Slow (~2 Hz) natural shakes can be rejected by the net-displacement filter; a 3-5 Hz shake with 300+ px travel is the reliable gesture.

## Feedback

Please include your Windows version, monitor layout/scaling, whether the issue is in a fullscreen app, and the output of:

```powershell
.\CursorBeacon.exe --dump-monitors
```

Logs: `%LOCALAPPDATA%\CursorBeacon\service.err.log`
