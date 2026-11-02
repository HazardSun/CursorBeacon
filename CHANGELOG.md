# Changelog

All notable changes to CursorBeacon are documented here.

## 0.1.0 — 2026-11-02

### Added

- Windows service mode (`--run`) with tray-less background operation
- Progressive cursor enlargement from 1x to 4x (configurable up to 8x)
- Automatic shrink after 800 ms hold; click-to-restore
- Per-monitor DPI support via `GetDpiForMonitor` and `GetSystemMetricsForDpi`
- Per-monitor fullscreen suppression using foreground geometry + QUNS
- Raw Input sampling with 10 Hz watchdog and 125 Hz polling fallback
- Cursor theme loading from `HKCU\Control Panel\Cursors`
- `SetSystemCursor` replacement with restore via `SPI_SETCURSORS`
- Crash/exit recovery with a dirty flag and `--restore`
- Single-instance guard and a named stop event (`--stop`)
- CLI: `--run`, `--stats`, `--test-scale`, `--dump-monitors`, `--dump-cursors`, `--restore`, `--stop`, `--self-test`, `--version`
- Built-in `--self-test` for environments where a separate test binary is blocked
- English and Simplified Chinese READMEs
- Security review and threat model in `docs/REVIEW.md`

### Fixed

- **Auto-shrink could stop working when the mouse became completely still**: Raw Input stops producing samples, so energy froze at its last value and the hold timer was renewed forever. The controller now decays energy over time (`ShakeDetector::decay`).
- **False triggers from ordinary mouse movement** (the pointer randomly grew and shrank): added higher speed/amplitude thresholds, path-length and net-displacement filters, a 250 ms activation delay, and a 600 ms cooldown.
- **Mid-shake scaling chattered at the reversal threshold**: default requirement is 3 reversals in 600 ms, with 2 at high sensitivity and 4 at low sensitivity.
- **Duplicate producers**: Raw Input samples are ignored while the polling fallback is active.
- **Unnecessary `SPI_SETCURSORS` calls**: restoration now runs only when an enlargement is actually active.
- **CLI numeric overflow/NaN**: all scale/sensitivity/hold/duration values are checked for finiteness and clamped.
- **Renderer overflow on extreme scales**: scale is clamped to 1–8 and target pixels to 8–512 before `lround`.
- **Unbounded configuration/registry reads**: config capped at 1 MB, registry strings at 64 KB.
- **Destructor exception risk**: `CursorEffector` catches restore failures during destruction and falls back to a non-allocating restore.
- **Timestamp 0 handling** in the detector (latent test/pure-logic bug).
- Static-analysis warnings: signed bitwise operations, ignored return values, and explicit size handling.

### Known limitations

- No tray icon, settings dialog or auto-start yet
- Fullscreen detection is foreground-window based; background/z-order scanning is pending
- Apps with custom-drawn cursors are unaffected
- `.ani` animated cursors may appear static while enlarged
- Cursor theme changes require a service restart
- Mixed 100%/150%/200% DPI setups need broader testing
- The binary is unsigned

### Verification

- 26 doctest cases and 13 built-in self-test checks pass
- Slow circular movement and net-progress zig-zag navigation produce 0 scale changes
- A deliberate 4 Hz shake grows to 4x and auto-shrinks after stopping
- Release binary ~940 KB; idle working set ~10.8 MB; idle CPU ~0.05% of 4 cores
