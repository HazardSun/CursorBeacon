#include "platform/monitors.h"

#include <windows.h>
#include <shellscalingapi.h>

#include <utility>

namespace cb {
namespace {

BOOL CALLBACK enum_monitor(HMONITOR handle, HDC, LPRECT, LPARAM user_data) {
    auto* output = reinterpret_cast<std::vector<MonitorInfo>*>(user_data);

    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(handle, &info)) {
        return TRUE;
    }

    MonitorInfo monitor;
    monitor.handle = reinterpret_cast<uintptr_t>(handle);
    monitor.monitor = Rect{info.rcMonitor.left, info.rcMonitor.top,
                           info.rcMonitor.right, info.rcMonitor.bottom};
    monitor.work = Rect{info.rcWork.left, info.rcWork.top,
                        info.rcWork.right, info.rcWork.bottom};
    monitor.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    monitor.device = info.szDevice;

    UINT dpi_x = 96;
    UINT dpi_y = 96;
    if (SUCCEEDED(GetDpiForMonitor(handle, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y))) {
        monitor.dpi = dpi_x == 0 ? 96u : dpi_x;
    }

    output->push_back(std::move(monitor));
    return TRUE;
}

}  // namespace

MonitorService::MonitorService() {
    refresh();
}

void MonitorService::refresh() {
    std::vector<MonitorInfo> next;
    EnumDisplayMonitors(nullptr, nullptr, enum_monitor,
                        reinterpret_cast<LPARAM>(&next));

    std::lock_guard lock(mutex_);
    monitors_ = std::move(next);
}

std::vector<MonitorInfo> MonitorService::snapshot() const {
    std::lock_guard lock(mutex_);
    return monitors_;
}

std::optional<MonitorInfo> MonitorService::for_point(Point point) const {
    {
        std::lock_guard lock(mutex_);
        for (const MonitorInfo& monitor : monitors_) {
            if (monitor.monitor.contains(point)) {
                return monitor;
            }
        }
    }

    const HMONITOR handle =
        MonitorFromPoint(POINT{point.x, point.y}, MONITOR_DEFAULTTONEAREST);
    if (handle == nullptr) {
        return std::nullopt;
    }

    std::lock_guard lock(mutex_);
    for (const MonitorInfo& monitor : monitors_) {
        if (monitor.handle == reinterpret_cast<uintptr_t>(handle)) {
            return monitor;
        }
    }
    return std::nullopt;
}

uint32_t MonitorService::dpi_for_point(Point point) const {
    const auto monitor = for_point(point);
    return monitor ? monitor->dpi : 96u;
}

bool MonitorService::find_by_handle(uintptr_t handle, MonitorInfo& out) const {
    std::lock_guard lock(mutex_);
    for (const MonitorInfo& monitor : monitors_) {
        if (monitor.handle == handle) {
            out = monitor;
            return true;
        }
    }
    return false;
}

}  // namespace cb
