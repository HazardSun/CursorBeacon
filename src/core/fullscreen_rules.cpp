#include "core/fullscreen_rules.h"

namespace cb {

bool is_fullscreen_on_monitor(const Rect& monitor_rect,
                              const Rect& work_rect,
                              const WindowInfo& window,
                              bool window_on_monitor,
                              bool quns_fullscreen,
                              int32_t tolerance) {
    if (!window.visible || window.minimized || window.cloaked || window.shell) {
        return false;
    }
    if (!window_on_monitor) {
        return false;
    }
    if (!window.frame.covers(monitor_rect, tolerance)) {
        return false;
    }

    // Borderless / popup windows that cover the whole monitor are fullscreen.
    if (window.popup || !window.has_caption) {
        return true;
    }

    // A captioned window that covers the monitor while the work area is
    // smaller (taskbar visible) is covering the taskbar; a normal maximized
    // window stops at the work area. If the taskbar is auto-hidden, work area
    // equals monitor area, so rely on the QUNS hint instead.
    if (!work_rect.covers(monitor_rect, tolerance)) {
        return true;
    }

    return quns_fullscreen;
}

}  // namespace cb
