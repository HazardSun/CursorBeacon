#pragma once

#include "core/fullscreen_rules.h"
#include "platform/monitors.h"

#include <string>

namespace cb {

enum class FullscreenReason {
    None,
    Locked,
    Presentation,
    ForegroundGeometry,
    ForegroundD3D,
};

struct FullscreenState {
    bool suppressed{};
    FullscreenReason reason{FullscreenReason::None};
    bool quns_fullscreen{};
    int quns_state{};
    uintptr_t foreground{};
    std::wstring title;
    std::wstring class_name;
    Rect frame;
    Rect window_monitor;
    bool window_on_cursor_monitor{};
};

class FullscreenService {
public:
    explicit FullscreenService(MonitorService& monitors);

    [[nodiscard]] FullscreenState evaluate(Point cursor, bool collect_details = false);

private:
    MonitorService& monitors_;
};

[[nodiscard]] const char* to_string(FullscreenReason reason);

}  // namespace cb
