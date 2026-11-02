// Toolchain smoke test for CursorBeacon.
// Verifies that the installed LLVM-MinGW headers, import libraries and CRT
// can compile and link every Win32 API family the project depends on.

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000000
#endif

#include <windows.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <dwmapi.h>
#include <wtsapi32.h>
#include <shlobj.h>
#include <objbase.h>
#include <synchapi.h>

#include <concepts>
#include <cstdio>
#include <span>
#include <string>

static int sum(std::span<const int> values) {
    int total = 0;
    for (int value : values) {
        total += value;
    }
    return total;
}

static bool is_positive(std::integral auto value) {
    return value > 0;
}

int main() {
    POINT cursor{};
    const BOOL has_cursor = GetPhysicalCursorPos(&cursor);

    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    const BOOL has_monitor = GetMonitorInfoW(monitor, &monitor_info);

    UINT dpi_x = 0;
    UINT dpi_y = 0;
    const HRESULT dpi_hr = GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y);
    const int cursor_base = GetSystemMetricsForDpi(SM_CXCURSOR, dpi_x ? dpi_x : 96);

    DWORD cloaked = 0;
    HWND foreground = GetForegroundWindow();
    if (foreground != nullptr) {
        DwmGetWindowAttribute(foreground, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    }

    QUERY_USER_NOTIFICATION_STATE notification_state{};
    const HRESULT quns_hr = SHQueryUserNotificationState(&notification_state);

    RAWINPUTDEVICE raw_device{};
    raw_device.usUsagePage = 0x01;
    raw_device.usUsage = 0x02;
    raw_device.dwFlags = RIDEV_INPUTSINK;
    const BOOL raw_registered = RegisterRawInputDevices(&raw_device, 1, sizeof(raw_device));

    const auto wts_register = &WTSRegisterSessionNotification;
    const auto set_cursor = &SetSystemCursor;
    const auto system_parameters = &SystemParametersInfoW;
    const auto waitable_timer = &CreateWaitableTimerExW;
    const auto open_key = &RegOpenKeyExW;
    const auto co_init = &CoInitializeEx;
    const auto known_folder = &SHGetKnownFolderPath;
    volatile const void* referenced[] = {
        reinterpret_cast<const void*>(wts_register),
        reinterpret_cast<const void*>(set_cursor),
        reinterpret_cast<const void*>(system_parameters),
        reinterpret_cast<const void*>(waitable_timer),
        reinterpret_cast<const void*>(open_key),
        reinterpret_cast<const void*>(co_init),
        reinterpret_cast<const void*>(known_folder),
    };
    (void)referenced;

    const int values[] = {1, 2, 3, 4};
    const int total = sum(values);

    std::wstring label = L"CursorBeacon toolchain";
    std::printf(
        "ok=%d cursor=(%ld,%ld) monitor=(%ld,%ld)-(%ld,%ld) dpi=(%u,%u) hr=0x%08lX "
        "base=%d quns=%d/quns_hr=0x%08lX raw=%d wts=%d sum=%d pos=%d label=%ls\n",
        has_cursor && has_monitor,
        cursor.x, cursor.y,
        monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
        monitor_info.rcMonitor.right, monitor_info.rcMonitor.bottom,
        dpi_x, dpi_y, static_cast<unsigned long>(dpi_hr),
        cursor_base, static_cast<int>(notification_state), static_cast<unsigned long>(quns_hr),
        raw_registered, wts_register != nullptr, total, is_positive(total) ? 1 : 0,
        label.c_str());
    return 0;
}
