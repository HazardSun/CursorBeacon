#pragma once

#include "core/types.h"

namespace cb {

struct WindowInfo {
    uintptr_t hwnd{};
    Rect frame;
    Rect monitor;
    bool visible{};
    bool minimized{};
    bool cloaked{};
    bool shell{};
    bool has_caption{};
    bool popup{};
    bool topmost{};
};

// Pure geometry/style rule. `window_on_monitor` should be true when the
// window's HMONITOR matches the monitor being tested.
bool is_fullscreen_on_monitor(const Rect& monitor_rect,
                              const Rect& work_rect,
                              const WindowInfo& window,
                              bool window_on_monitor,
                              bool quns_fullscreen,
                              int32_t tolerance = 2);

}  // namespace cb
