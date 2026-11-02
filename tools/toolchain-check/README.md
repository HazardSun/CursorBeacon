# Toolchain check

这个小工程用来验证本地工具链能编译、链接并运行 CursorBeacon 需要的 Win32 API 家族。它不是产品代码，可以随时删除。

## 构建

在 Git Bash 中：

```bash
cd tools/toolchain-check
source ../env.sh

cmake -S . -B build-cmake -G Ninja \
  -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_RC_COMPILER="$(cygpath -m "$LLVM_MINGW")/bin/windres.exe" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake
```

## 验证

```bash
./build-cmake/toolchain-check.exe
```

预期输出类似：

```text
ok=1 cursor=(539,1167) monitor=(0,0)-(1920,1200) dpi=(96,96) hr=0x00000000 base=32 quns=5/quns_hr=0x00000000 raw=0 wts=1 sum=10 pos=1 label=CursorBeacon toolchain
```

- `ok=1`：`GetPhysicalCursorPos` / `MonitorFromPoint` / `GetMonitorInfoW` 正常。
- `hr=0x00000000`：`GetDpiForMonitor` 成功。
- `base=32`：`GetSystemMetricsForDpi(SM_CXCURSOR, dpi)` 正常。
- `quns=5`：`SHQueryUserNotificationState` 返回 `QUNS_ACCEPTS_NOTIFICATIONS`（当前不是全屏）。
- `raw=0`：冒烟测试没有创建消息窗口，`RegisterRawInputDevices` 预期失败；正式实现会创建 `HWND_MESSAGE` 窗口。
- `wts=1`：WTS 符号已链接。
- `sum=10 pos=1`：C++20 `std::span` / concepts 正常。

GUI 目标 `build-cmake/toolchain-check-gui.exe` 会嵌入 `app.manifest`（PerMonitorV2 + asInvoker），可用 `llvm-readobj --file-headers` 确认 `Subsystem: IMAGE_SUBSYSTEM_WINDOWS_GUI`，用 `--coff-resources` 确认 `MANIFEST (ID 24)`。不要直接运行它，它会弹一个消息框。

## 已覆盖的 API

- 光标/显示器：`GetPhysicalCursorPos`、`MonitorFromPoint`、`GetMonitorInfoW`、`GetDpiForMonitor`、`GetSystemMetricsForDpi`
- 全屏/窗口：`GetForegroundWindow`、`DwmGetWindowAttribute`、`SHQueryUserNotificationState`
- 输入：`RegisterRawInputDevices`
- 光标/系统参数：`SetSystemCursor`、`SystemParametersInfoW`
- 会话/注册表/COM：`WTSRegisterSessionNotification`、`RegOpenKeyExW`、`CoInitializeEx`、`SHGetKnownFolderPath`
- 计时器：`CreateWaitableTimerExW`
- 资源/清单：`windres` + `app.manifest`
