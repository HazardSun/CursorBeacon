#include "platform/fullscreen.h"

#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>

#include <array>
#include <string>

namespace cb {
namespace {

Rect to_rect(const RECT& rect) {
    return Rect{rect.left, rect.top, rect.right, rect.bottom};
}

std::wstring window_class(HWND hwnd) {
    std::array<wchar_t, 256> buffer{};
    const int length = GetClassNameW(hwnd, buffer.data(), static_cast<int>(buffer.size()));
    if (length <= 0) {
        return {};
    }
    return std::wstring(buffer.data(), static_cast<std::size_t>(length));
}

std::wstring window_title(HWND hwnd) {
    std::array<wchar_t, 512> buffer{};
    DWORD_PTR result = 0;
    SendMessageTimeoutW(hwnd, WM_GETTEXT, buffer.size(),
                        reinterpret_cast<LPARAM>(buffer.data()),
                        SMTO_ABORTIFHUNG | SMTO_BLOCK, 50, &result);
    if (result == 0) {
        return {};
    }
    return std::wstring(buffer.data(), static_cast<std::size_t>(result));
}

bool is_shell_window(HWND hwnd, const std::wstring& class_name) {
    if (hwnd == GetShellWindow()) {
        return true;
    }
    return class_name == L"Progman" || class_name == L"WorkerW" ||
           class_name == L"Shell_TrayWnd";
}

bool get_frame_bounds(HWND hwnd, RECT& rect) {
    const HRESULT result =
        DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(rect));
    if (SUCCEEDED(result)) {
        return true;
    }
    return GetWindowRect(hwnd, &rect) != FALSE;
}

}  // namespace

FullscreenService::FullscreenService(MonitorService& monitors) : monitors_(monitors) {}

FullscreenState FullscreenService::evaluate(Point cursor, bool collect_details) {
    FullscreenState state;

    QUERY_USER_NOTIFICATION_STATE quns = QUNS_ACCEPTS_NOTIFICATIONS;
    const HRESULT quns_result = SHQueryUserNotificationState(&quns);
    state.quns_state = static_cast<int>(quns);

    if (SUCCEEDED(quns_result)) {
        if (quns == QUNS_NOT_PRESENT) {
            state.suppressed = true;
            state.reason = FullscreenReason::Locked;
            return state;
        }
        if (quns == QUNS_PRESENTATION_MODE) {
            state.suppressed = true;
            state.reason = FullscreenReason::Presentation;
            return state;
        }
    }

    const auto cursor_monitor = monitors_.for_point(cursor);
    if (!cursor_monitor) {
        return state;
    }

    const HWND foreground = GetForegroundWindow();
    if (foreground == nullptr) {
        return state;
    }

    const std::wstring class_name = window_class(foreground);
    if (is_shell_window(foreground, class_name)) {
        return state;
    }

    state.foreground = reinterpret_cast<uintptr_t>(foreground);
    state.class_name = class_name;
    if (collect_details) {
        state.title = window_title(foreground);
    }

    WindowInfo info;
    info.hwnd = state.foreground;
    info.visible = IsWindowVisible(foreground) != FALSE;
    info.minimized = IsIconic(foreground) != FALSE;
    info.shell = false;

    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(foreground, DWMWA_CLOAKED, &cloaked,
                                        sizeof(cloaked)))) {
        info.cloaked = cloaked != 0;
    }

    const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(foreground, GWL_STYLE));
    const DWORD ex_style =
        static_cast<DWORD>(GetWindowLongPtrW(foreground, GWL_EXSTYLE));
    info.has_caption = (style & WS_CAPTION) == WS_CAPTION;
    info.popup = (style & WS_POPUP) != 0;
    info.topmost = (ex_style & WS_EX_TOPMOST) != 0;

    RECT frame{};
    if (get_frame_bounds(foreground, frame)) {
        info.frame = to_rect(frame);
        state.frame = info.frame;
    }

    const HMONITOR window_monitor =
        MonitorFromWindow(foreground, MONITOR_DEFAULTTONEAREST);
    if (window_monitor != nullptr) {
        MONITORINFO monitor_info{};
        monitor_info.cbSize = sizeof(monitor_info);
        if (GetMonitorInfoW(window_monitor, &monitor_info)) {
            info.monitor = to_rect(monitor_info.rcMonitor);
            state.window_monitor = info.monitor;
        }
        state.window_on_cursor_monitor =
            reinterpret_cast<uintptr_t>(window_monitor) == cursor_monitor->handle;
    }

    const bool quns_fullscreen = quns == QUNS_RUNNING_D3D_FULL_SCREEN || quns == QUNS_BUSY;
    state.quns_fullscreen = quns_fullscreen;

    if (is_fullscreen_on_monitor(cursor_monitor->monitor, cursor_monitor->work, info,
                                 state.window_on_cursor_monitor, quns_fullscreen)) {
        state.suppressed = true;
        state.reason = quns == QUNS_RUNNING_D3D_FULL_SCREEN
                           ? FullscreenReason::ForegroundD3D
                           : FullscreenReason::ForegroundGeometry;
    }

    return state;
}

const char* to_string(FullscreenReason reason) {
    switch (reason) {
        case FullscreenReason::None:
            return "none";
        case FullscreenReason::Locked:
            return "locked";
        case FullscreenReason::Presentation:
            return "presentation";
        case FullscreenReason::ForegroundGeometry:
            return "foreground-geometry";
        case FullscreenReason::ForegroundD3D:
            return "foreground-d3d";
    }
    return "unknown";
}

}  // namespace cb
