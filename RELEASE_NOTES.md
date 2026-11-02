# CursorBeacon 0.1.0 — Windows shake-to-find cursor

CursorBeacon makes the real Windows pointer grow when you shake the mouse, then shrink back automatically. It is built for 4K/8K and multi-monitor desktops, with per-monitor DPI and fullscreen suppression.

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

- `CursorBeacon-0.1.0-win64.zip`
- `CursorBeacon-0.1.0-win64.zip.sha256`

SHA-256:

```text
0BE3E2007DC15042B02D607A1D164E7F668ADA759E3BB8F7AA944B1CEC18C306  CursorBeacon-0.1.0-win64.zip
C40E25FCEE6C802E20C46353F9ABAB00E9106201994F0B38EE93A1EF69FB8723  CursorBeacon.exe
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

## Feedback

Please include your Windows version, monitor layout/scaling, whether the issue is in a fullscreen app, and the output of:

```powershell
.\CursorBeacon.exe --dump-monitors
```

Logs: `%LOCALAPPDATA%\CursorBeacon\service.err.log`
