#include "app/controller.h"
#include "app/self_test.h"
#include "config/config.h"
#include "lifecycle/crash_guard.h"
#include "lifecycle/service_control.h"
#include "lifecycle/single_instance.h"
#include "platform/cursor_effector.h"
#include "platform/cursor_renderer.h"
#include "platform/cursor_theme.h"
#include "platform/fullscreen.h"
#include "platform/monitors.h"
#include "util/log.h"
#include "util/time.h"

#include <windows.h>
#include <objbase.h>
#include <shellapi.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

using namespace cb;

namespace {

constexpr const char* kVersion = "0.1.0";

std::atomic<bool> g_stop{false};

BOOL WINAPI console_control(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT ||
        type == CTRL_CLOSE_EVENT || type == CTRL_SHUTDOWN_EVENT ||
        type == CTRL_LOGOFF_EVENT) {
        g_stop = true;
        return TRUE;
    }
    return FALSE;
}

std::string to_utf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(CP_UTF8, 0, value.data(),
                                           static_cast<int>(value.size()), nullptr, 0,
                                           nullptr, nullptr);
    if (length <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                        result.data(), length, nullptr, nullptr);
    return result;
}

bool has_flag(const std::vector<std::wstring>& args, const wchar_t* flag) {    for (const std::wstring& arg : args) {
        if (arg == flag) {
            return true;
        }
    }
    return false;
}

std::wstring option_value(const std::vector<std::wstring>& args, const wchar_t* option,
                          const wchar_t* fallback = L"") {
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == option) {
            return args[i + 1];
        }
    }
    return fallback;
}

double optional_number_after(const std::vector<std::wstring>& args,
                             const wchar_t* option, double fallback) {
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == option && args[i + 1].rfind(L"--", 0) != 0) {
            try {
                return std::stod(args[i + 1]);
            } catch (...) {
                return fallback;
            }
        }
    }
    return fallback;
}

int print_monitors() {
    MonitorService monitors;
    const auto list = monitors.snapshot();
    std::printf("monitors=%zu\n", list.size());
    for (std::size_t i = 0; i < list.size(); ++i) {
        const MonitorInfo& monitor = list[i];
        std::printf("  [%zu] device=%s rect=(%d,%d)-(%d,%d) work=(%d,%d)-(%d,%d) "
                    "dpi=%u primary=%s\n",
                    i, to_utf8(monitor.device).c_str(),
                    monitor.monitor.left, monitor.monitor.top,
                    monitor.monitor.right, monitor.monitor.bottom,
                    monitor.work.left, monitor.work.top, monitor.work.right,
                    monitor.work.bottom, monitor.dpi,
                    monitor.primary ? "yes" : "no");
    }

    POINT cursor{};
    if (!GetPhysicalCursorPos(&cursor) && !GetCursorPos(&cursor)) {
        std::printf("cursor=unavailable\n");
        return 1;
    }
    const auto cursor_monitor = monitors.for_point(Point{cursor.x, cursor.y});
    std::printf("cursor=(%ld,%ld) dpi=%u monitor=%s\n", cursor.x, cursor.y,
                cursor_monitor ? cursor_monitor->dpi : 96u,
                cursor_monitor ? to_utf8(cursor_monitor->device).c_str() : "unknown");

    FullscreenService fullscreen(monitors);
    const FullscreenState state =
        fullscreen.evaluate(Point{cursor.x, cursor.y}, true);
    std::printf("quns=%d suppressed=%s reason=%s foreground=0x%llX\n",
                state.quns_state, state.suppressed ? "yes" : "no",
                to_string(state.reason),
                static_cast<unsigned long long>(state.foreground));
    std::printf("foreground class=%s title=%s\n",
                to_utf8(state.class_name).c_str(), to_utf8(state.title).c_str());
    std::printf("foreground frame=(%d,%d)-(%d,%d) monitor=(%d,%d)-(%d,%d) "
                "on_cursor_monitor=%s\n",
                state.frame.left, state.frame.top, state.frame.right,
                state.frame.bottom, state.window_monitor.left,
                state.window_monitor.top, state.window_monitor.right,
                state.window_monitor.bottom,
                state.window_on_cursor_monitor ? "yes" : "no");
    return 0;
}

int print_cursors() {
    MonitorService monitors;
    CursorTheme theme;
    theme.reload();
    std::printf("cursor theme entries=%zu\n", theme.entries().size());
    for (const CursorThemeEntry& entry : theme.entries()) {
        std::printf("  %-12s ocr=%-5lu %s\n",
                    to_utf8(entry.registry_name).c_str(),
                    static_cast<unsigned long>(entry.ocr_id),
                    entry.path.empty() ? "<stock>" : to_utf8(entry.path).c_str());
    }
    for (const MonitorInfo& monitor : monitors.snapshot()) {
        const int base = GetSystemMetricsForDpi(SM_CXCURSOR, monitor.dpi);
        std::printf("monitor dpi=%u base_cursor=%dpx 4x=%dpx\n", monitor.dpi,
                    base, base * 4);
    }
    return 0;
}

bool cursor_hotspot(HCURSOR cursor, int& x, int& y, SIZE& size) {
    ICONINFO info{};
    if (!GetIconInfo(cursor, &info)) {
        return false;
    }
    x = static_cast<int>(info.xHotspot);
    y = static_cast<int>(info.yHotspot);
    size = SIZE{0, 0};

    const auto read_size = [&](HBITMAP handle) {
        if (handle == nullptr) {
            return false;
        }
        BITMAP bitmap{};
        if (GetObjectW(handle, sizeof(bitmap), &bitmap) == 0) {
            return false;
        }
        size = SIZE{bitmap.bmWidth, bitmap.bmHeight};
        return true;
    };
    if (!read_size(info.hbmColor)) {
        (void)read_size(info.hbmMask);
    }

    if (info.hbmMask != nullptr) {
        DeleteObject(info.hbmMask);
    }
    if (info.hbmColor != nullptr) {
        DeleteObject(info.hbmColor);
    }
    return true;
}

int test_scale(double scale) {
    if (!std::isfinite(scale)) {
        scale = 1.0;
    }
    scale = std::clamp(scale, 1.0, 8.0);

    CursorEffector::recover_if_needed();

    MonitorService monitors;
    POINT cursor{};
    if (!GetPhysicalCursorPos(&cursor) && !GetCursorPos(&cursor)) {
        log_error("cursor position unavailable");
        return 1;
    }
    const uint32_t dpi = monitors.dpi_for_point(Point{cursor.x, cursor.y});

    CursorTheme theme;
    theme.reload();
    const auto selected = theme.selected(CursorTheme::default_cursor_names());
    if (selected.empty()) {
        log_error("no cursor entries selected");
        return 1;
    }

    CursorRenderer renderer;
    const auto frames = renderer.render(selected, dpi, scale);
    if (frames.empty()) {
        log_error("renderer produced no frames");
        return 1;
    }

    std::printf("test-scale %.2fx dpi=%u target=%dpx frames=%zu\n", scale, dpi,
                renderer.target_px(), frames.size());

    for (const RenderedCursor& frame : frames) {
        const CursorThemeEntry* entry = nullptr;
        for (const CursorThemeEntry& candidate : selected) {
            if (candidate.ocr_id == frame.ocr_id) {
                entry = &candidate;
                break;
            }
        }
        if (entry == nullptr) {
            continue;
        }

        int original_x = 0;
        int original_y = 0;
        SIZE original_size{};
        HCURSOR original = nullptr;
        bool original_owned = false;
        if (!entry->path.empty()) {
            original = reinterpret_cast<HCURSOR>(LoadImageW(
                nullptr, entry->path.c_str(), IMAGE_CURSOR, 0, 0,
                LR_LOADFROMFILE | LR_DEFAULTSIZE));
            original_owned = original != nullptr;
        }
        if (original == nullptr) {
            original = LoadCursorW(nullptr, MAKEINTRESOURCEW(entry->ocr_id));
        }

        if (original != nullptr &&
            cursor_hotspot(original, original_x, original_y, original_size)) {
            int scaled_x = 0;
            int scaled_y = 0;
            SIZE scaled_size{};
            if (cursor_hotspot(frame.handle, scaled_x, scaled_y, scaled_size)) {
                const int expected_x =
                    original_size.cx > 0 && scaled_size.cx > 0
                        ? static_cast<int>(std::lround(
                              static_cast<double>(original_x) *
                              static_cast<double>(scaled_size.cx) /
                              static_cast<double>(original_size.cx)))
                        : original_x;
                const int expected_y =
                    original_size.cy > 0 && scaled_size.cy > 0
                        ? static_cast<int>(std::lround(
                              static_cast<double>(original_y) *
                              static_cast<double>(scaled_size.cy) /
                              static_cast<double>(original_size.cy)))
                        : original_y;
                std::printf(
                    "hotspot %-10s original=(%d,%d) %ldx%ld -> scaled=(%d,%d) "
                    "%ldx%ld expected=(%d,%d)\n",
                    to_utf8(entry->registry_name).c_str(), original_x,
                    original_y, static_cast<long>(original_size.cx),
                    static_cast<long>(original_size.cy), scaled_x, scaled_y,
                    static_cast<long>(scaled_size.cx),
                    static_cast<long>(scaled_size.cy), expected_x, expected_y);
            }
        }

        if (original_owned && original != nullptr) {
            DestroyCursor(original);
        }
    }

    CursorEffector effector;
    if (!effector.apply(frames, scale)) {
        const std::string error(effector.last_error().begin(),
                                effector.last_error().end());
        log_error("apply failed: " + error);
        return 1;
    }

    Sleep(2000);
    if (!effector.restore()) {
        log_error("restore failed");
        return 1;
    }
    std::printf("test-scale restored\n");
    return 0;
}

void print_help() {
    std::printf(
        "CursorBeacon (development build)\n"
        "Usage:\n"
        "  cursorbeacon --run                 run the controller (Ctrl+C to stop)\n"
        "  cursorbeacon --run --duration N    run for N seconds (development)\n"
        "  cursorbeacon --stats [seconds]     sample and print statistics, no effects\n"
        "  cursorbeacon --test-scale [N]      enlarge for 2 seconds, then restore\n"
        "  cursorbeacon --dump-monitors       print monitor, DPI and fullscreen state\n"
        "  cursorbeacon --dump-cursors        print cursor theme paths\n"
        "  cursorbeacon --restore             restore the system cursor scheme\n"
        "  cursorbeacon --stop                ask a running instance to exit and restore\n"
        "  cursorbeacon --self-test           run built-in logic checks\n"
        "  cursorbeacon --version             print the version\n"
        "Options:\n"
        "  --config PATH   config file (default %%APPDATA%%\\CursorBeacon\\config.ini)\n"
        "  --scale N       max scale override (1..8)\n"
        "  --sensitivity N shake sensitivity override (1..10)\n"
        "  --hold-ms N     hold time override in milliseconds\n"
        "  --verbose       debug logging\n");
}

}  // namespace

int run_main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::wstring> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    if (argv != nullptr) {
        LocalFree(reinterpret_cast<HLOCAL>(argv));
    }

    const HRESULT com_result =
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool com_initialized = SUCCEEDED(com_result);

    const std::wstring default_path = default_config_path();
    Config config = Config::load(option_value(args, L"--config", default_path.c_str()));

    const double scale_override = optional_number_after(args, L"--scale", 0.0);
    if (std::isfinite(scale_override) && scale_override > 0.0) {
        config.max_scale = std::clamp(scale_override, 1.0, 8.0);
    }
    const double sensitivity_override =
        optional_number_after(args, L"--sensitivity", 0.0);
    if (std::isfinite(sensitivity_override) && sensitivity_override > 0.0) {
        config.sensitivity = static_cast<int>(std::lround(
            std::clamp(sensitivity_override, 1.0, 10.0)));
    }
    const double hold_override = optional_number_after(args, L"--hold-ms", -1.0);
    if (std::isfinite(hold_override) && hold_override >= 0.0) {
        config.hold_ms = std::clamp(hold_override, 0.0, 5000.0);
    }
    if (!std::isfinite(config.max_scale)) {
        config.max_scale = 4.0;
    }
    if (!std::isfinite(config.hold_ms)) {
        config.hold_ms = 800.0;
    }
    config.max_scale = std::clamp(config.max_scale, 1.0, 8.0);
    config.sensitivity = std::clamp(config.sensitivity, 1, 10);
    config.hold_ms = std::clamp(config.hold_ms, 0.0, 5000.0);
    if (has_flag(args, L"--verbose")) {
        config.verbose = true;
        set_log_level(LogLevel::Debug);
    }

    int result = 0;

    if (has_flag(args, L"--help") || has_flag(args, L"-h")) {
        print_help();
    } else if (has_flag(args, L"--version")) {
        std::printf("CursorBeacon %s (win64)\n", kVersion);
    } else if (has_flag(args, L"--self-test")) {
        result = run_self_tests();
    } else if (has_flag(args, L"--restore")) {
        const bool recovered = CursorEffector::recover_if_needed();
        const bool restored = CursorEffector::restore_system_cursors();
        std::printf("restore recovered_dirty=%s restored=%s\n",
                    recovered ? "yes" : "no", restored ? "yes" : "no");
        result = restored ? 0 : 1;
    } else if (has_flag(args, L"--stop")) {
        if (!request_stop()) {
            std::printf("CursorBeacon is not running\n");
            result = 1;
        } else {
            std::printf("stop requested\n");
            Sleep(600);
            CursorEffector::restore_system_cursors();
        }
    } else if (has_flag(args, L"--dump-monitors")) {
        result = print_monitors();
    } else if (has_flag(args, L"--dump-cursors")) {
        result = print_cursors();
    } else if (has_flag(args, L"--test-scale")) {
        SingleInstance instance;
        if (!instance.acquire()) {
            log_error("another CursorBeacon instance is already running");
            result = 1;
        } else {
            install_crash_handlers();
            const double value = optional_number_after(args, L"--test-scale", config.max_scale);
            result = test_scale(value);
        }
    } else if (has_flag(args, L"--stats")) {
        const double seconds_arg =
            optional_number_after(args, L"--stats", 5.0);
        const int seconds =
            std::isfinite(seconds_arg)
                ? std::clamp(static_cast<int>(std::lround(seconds_arg)), 0, 86'400)
                : 5;
        result = run_controller(config, false, g_stop, seconds, true);
    } else if (has_flag(args, L"--run") || args.empty()) {
        SingleInstance instance;
        if (!instance.acquire()) {
            log_error("another CursorBeacon instance is already running");
            result = 1;
        } else {
            install_crash_handlers();
            SetConsoleCtrlHandler(&console_control, TRUE);
            const double duration_arg =
                optional_number_after(args, L"--duration", 0.0);
            const int duration =
                (std::isfinite(duration_arg) && duration_arg > 0.0)
                    ? std::clamp(static_cast<int>(std::lround(duration_arg)), 1,
                                 86'400)
                    : 0;
            result = run_controller(config, true, g_stop, duration, config.verbose);
            SetConsoleCtrlHandler(&console_control, FALSE);
        }
    } else {
        print_help();
        result = 1;
    }

    if (com_initialized) {
        CoUninitialize();
    }
    return result;
}

int main() {
    try {
        return run_main();
    } catch (const std::exception& error) {
        (void)std::fprintf(stderr, "fatal: %s\n", error.what());
    } catch (...) {
        (void)std::fprintf(stderr, "fatal: unknown error\n");
    }
    return 1;
}
