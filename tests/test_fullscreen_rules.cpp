#include "core/fullscreen_rules.h"

#include <doctest/doctest.h>

namespace {

const cb::Rect kMonitor{0, 0, 1920, 1080};
const cb::Rect kWork{0, 0, 1920, 1040};

cb::WindowInfo borderless_fullscreen() {
    cb::WindowInfo window;
    window.visible = true;
    window.frame = kMonitor;
    window.monitor = kMonitor;
    window.has_caption = false;
    window.popup = true;
    return window;
}

}  // namespace

TEST_CASE("borderless frame covering the monitor is fullscreen") {
    CHECK(cb::is_fullscreen_on_monitor(kMonitor, kWork, borderless_fullscreen(), true, false));
}

TEST_CASE("maximized window that stops at the work area is not fullscreen") {
    cb::WindowInfo window;
    window.visible = true;
    window.frame = kWork;
    window.monitor = kMonitor;
    window.has_caption = true;
    CHECK_FALSE(cb::is_fullscreen_on_monitor(kMonitor, kWork, window, true, false));
}

TEST_CASE("captioned window covering the taskbar area is fullscreen") {
    cb::WindowInfo window;
    window.visible = true;
    window.frame = kMonitor;
    window.monitor = kMonitor;
    window.has_caption = true;
    CHECK(cb::is_fullscreen_on_monitor(kMonitor, kWork, window, true, false));
}

TEST_CASE("captioned window on an auto-hide taskbar desktop is not fullscreen") {
    cb::WindowInfo window;
    window.visible = true;
    window.frame = kMonitor;
    window.monitor = kMonitor;
    window.has_caption = true;
    CHECK_FALSE(cb::is_fullscreen_on_monitor(kMonitor, kMonitor, window, true, false));
}

TEST_CASE("window on another monitor is not fullscreen for this monitor") {
    CHECK_FALSE(cb::is_fullscreen_on_monitor(kMonitor, kWork, borderless_fullscreen(), false, true));
}

TEST_CASE("minimized, cloaked and shell windows are ignored") {
    auto window = borderless_fullscreen();
    window.minimized = true;
    CHECK_FALSE(cb::is_fullscreen_on_monitor(kMonitor, kWork, window, true, true));

    window = borderless_fullscreen();
    window.cloaked = true;
    CHECK_FALSE(cb::is_fullscreen_on_monitor(kMonitor, kWork, window, true, true));

    window = borderless_fullscreen();
    window.shell = true;
    CHECK_FALSE(cb::is_fullscreen_on_monitor(kMonitor, kWork, window, true, true));
}

TEST_CASE("geometry tolerance accepts one or two pixels of slack") {
    auto window = borderless_fullscreen();
    window.frame = cb::Rect{1, 1, 1919, 1079};
    CHECK(cb::is_fullscreen_on_monitor(kMonitor, kWork, window, true, false));
}

TEST_CASE("QUNS hint promotes a captioned monitor-sized window") {
    cb::WindowInfo window;
    window.visible = true;
    window.frame = kMonitor;
    window.monitor = kMonitor;
    window.has_caption = true;
    CHECK(cb::is_fullscreen_on_monitor(kMonitor, kMonitor, window, true, true));
}
