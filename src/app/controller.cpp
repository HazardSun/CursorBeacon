#include "app/controller.h"

#include "core/shake_detector.h"
#include "core/state_machine.h"
#include "lifecycle/service_control.h"
#include "platform/cursor_effector.h"
#include "platform/cursor_renderer.h"
#include "platform/cursor_theme.h"
#include "platform/fullscreen.h"
#include "platform/monitors.h"
#include "platform/pointer_source.h"
#include "util/log.h"
#include "util/time.h"

#include <windows.h>

#include <algorithm>
#include <memory>
#include <sstream>

namespace cb {

int run_controller(const Config& config, bool apply_effects,
                   std::atomic<bool>& stop, int stats_seconds,
                   bool print_stats) {
    if (!config.enabled) {
        log_error("controller disabled by configuration");
        return 1;
    }

    CursorEffector::recover_if_needed();

    struct HandleCloser {
        void operator()(HANDLE handle) const {
            if (handle != nullptr) {
                CloseHandle(handle);
            }
        }
    };
    std::unique_ptr<void, HandleCloser> stop_event(create_stop_event());

    MonitorService monitors;
    FullscreenService fullscreen(monitors);
    SampleQueue queue;
    PointerSource pointer(queue, monitors);

    ShakeDetector detector(config.shake_config());
    StateMachine state(config.scale_config());

    CursorTheme theme;
    theme.reload();
    const std::vector<CursorThemeEntry> selected = theme.selected(config.cursor_set);
    if (apply_effects && selected.empty()) {
        log_error("no cursor entries selected; check cursor_set in config");
        return 1;
    }

    CursorRenderer renderer;
    CursorEffector effector;

    POINT initial{};
    if (!GetPhysicalCursorPos(&initial) && !GetCursorPos(&initial)) {
        initial = POINT{0, 0};
    }

    Sample last_sample{now_us(), initial.x, initial.y, 0,
                       monitors.dpi_for_point(Point{initial.x, initial.y})};

    if (!pointer.start()) {
        log_error("failed to start pointer source");
        return 1;
    }

    std::ostringstream startup;
    startup << "started mode="
            << (pointer.mode() == PointerSource::Mode::RawInput ? "rawinput" : "polling")
            << " raw_available=" << (pointer.raw_input_available() ? "yes" : "no")
            << " monitors=" << monitors.snapshot().size()
            << " cursors=" << selected.size()
            << " max_scale=" << config.max_scale
            << " sensitivity=" << config.sensitivity
            << " effects=" << (apply_effects ? "on" : "off");
    log_info(startup.str());

    if (config.verbose) {
        for (const MonitorInfo& monitor : monitors.snapshot()) {
            std::ostringstream line;
            line << "monitor device=";
            line << std::string(monitor.device.begin(), monitor.device.end());
            line << " rect=" << monitor.monitor.left << "," << monitor.monitor.top
                 << " " << monitor.monitor.right << "x" << monitor.monitor.bottom
                 << " work=" << monitor.work.left << "," << monitor.work.top << " "
                 << monitor.work.right << "x" << monitor.work.bottom
                 << " dpi=" << monitor.dpi
                 << " primary=" << (monitor.primary ? "yes" : "no");
            log_info(line.str());
        }
        for (const CursorThemeEntry& entry : selected) {
            std::ostringstream line;
            line << "cursor " << std::string(entry.registry_name.begin(),
                                             entry.registry_name.end())
                 << " ocr=" << entry.ocr_id
                 << " path="
                 << (entry.path.empty()
                         ? "<stock>"
                         : std::string(entry.path.begin(), entry.path.end()));
            log_info(line.str());
        }
    }

    const int64_t start_us = now_us();
    int64_t last_fullscreen_check_us = 0;
    int64_t last_monitor_refresh_us = 0;
    int64_t last_stats_us = 0;
    uint64_t samples_processed = 0;
    double peak_energy = 0.0;
    FullscreenState last_fullscreen;
    bool effects_enabled = apply_effects;
    bool logged_effect_error = false;
    bool was_active = false;
    uintptr_t last_cursor_monitor = 0;
    if (const auto monitor = monitors.for_point(Point{initial.x, initial.y})) {
        last_cursor_monitor = monitor->handle;
    }

    while (!stop.load()) {
        if (stop_requested(stop_event.get())) {
            break;
        }

        const bool active =
            state.state() == State::Shaking || state.state() == State::Holding ||
            state.state() == State::Shrinking || detector.energy() > 0.01;
        queue.wait(active ? 5 : 100);

        const int64_t now = now_us();

        if (stats_seconds > 0 &&
            now - start_us >= static_cast<int64_t>(stats_seconds) * 1'000'000) {
            break;
        }

        Sample sample{};
        bool had_sample = false;
        while (queue.pop(sample)) {
            detector.add_sample(sample);
            last_sample = sample;
            peak_energy = std::max(peak_energy, detector.energy());
            ++samples_processed;
            had_sample = true;
        }

        if (now - last_monitor_refresh_us > 5'000'000) {
            monitors.refresh();
            last_monitor_refresh_us = now;
        }

        bool monitor_changed = false;
        if (had_sample) {
            if (const auto monitor = monitors.for_point(Point{last_sample.x, last_sample.y})) {
                if (monitor->handle != last_cursor_monitor) {
                    last_cursor_monitor = monitor->handle;
                    monitor_changed = true;
                }
            }
        }

        bool suppressed = false;
        const int64_t fullscreen_interval = active ? 100'000 : 1'000'000;
        const bool force_fullscreen = monitor_changed || (active && !was_active);
        if (config.pause_on_fullscreen &&
            (force_fullscreen || now - last_fullscreen_check_us > fullscreen_interval)) {
            last_fullscreen = fullscreen.evaluate(Point{last_sample.x, last_sample.y});
            last_fullscreen_check_us = now;
        }
        was_active = active;
        if (config.pause_on_fullscreen) {
            suppressed = last_fullscreen.suppressed;
        }

        if (suppressed) {
            detector.reset();
        } else {
            // Raw Input stops emitting when the mouse stops moving. Without
            // this, the last energy value would stay frozen and the hold timer
            // would be renewed forever, so auto-shrink would never happen.
            detector.decay(now);
        }

        if (config.pause_on_click && detector.button_down()) {
            suppressed = true;
        }

        const StateMachine::Decision decision =
            state.update(suppressed ? 0.0 : detector.energy(), suppressed, now);

        if (decision.restore && effects_enabled && effector.applied()) {
            if (!effector.restore()) {
                log_error("restore failed");
            } else {
                log_info("cursor restored");
            }
        }

        if (decision.apply && decision.scale > 1.01 && effects_enabled) {
            const auto frames = renderer.render(selected, last_sample.dpi, decision.scale);
            if (frames.empty()) {
                if (!logged_effect_error) {
                    log_error("cursor renderer produced no frames; disabling effects");
                    logged_effect_error = true;
                }
                effects_enabled = false;
            } else if (!effector.apply(frames, decision.scale)) {
                if (!logged_effect_error) {
                    log_error("cursor apply failed: " +
                              std::string(effector.last_error().begin(),
                                          effector.last_error().end()));
                    logged_effect_error = true;
                }
                effects_enabled = false;
            } else {
                std::ostringstream line;
                line << "scale " << decision.scale << "x -> "
                     << renderer.target_px() << "px dpi=" << last_sample.dpi;
                log_info(line.str());
            }
        }

        // If the cursor crossed into another monitor while enlarged, rebuild
        // the frames for that monitor's DPI even if the scale did not change.
        if (effects_enabled && effector.applied() &&
            last_sample.dpi != renderer.dpi() &&
            state.scale().applied_scale() > 1.01) {
            const auto frames =
                renderer.render(selected, last_sample.dpi, state.scale().applied_scale());
            if (!frames.empty()) {
                effector.apply(frames, state.scale().applied_scale());
            }
        }

        if (print_stats && now - last_stats_us > 2'000'000) {
            last_stats_us = now;
            DWORD handle_count = 0;
            GetProcessHandleCount(GetCurrentProcess(), &handle_count);
            const DWORD gdi_objects = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
            const DWORD user_objects = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);

            std::ostringstream line;
            line << "stats samples=" << samples_processed
                 << " handles=" << handle_count
                 << " gdi=" << gdi_objects
                 << " user=" << user_objects
                 << " mode="
                 << (pointer.mode() == PointerSource::Mode::RawInput ? "rawinput"
                                                                     : "polling")
                 << " raw_events=" << pointer.raw_events()
                 << " polled=" << pointer.polled_samples()
                 << " watchdog=" << pointer.watchdog_checks()
                 << " energy=" << detector.energy()
                 << " reversals=" << detector.reversals()
                 << " total_rev=" << detector.total_reversals()
                 << " amplitude=" << detector.amplitude_dip()
                 << " path=" << detector.path_length_dip()
                 << " net=" << detector.net_displacement_dip()
                 << " return_ratio="
                 << (detector.path_length_dip() > 1.0
                         ? detector.net_displacement_dip() /
                               detector.path_length_dip()
                         : 1.0)
                 << " net_amp_ratio="
                 << (detector.amplitude_dip() > 1.0
                         ? detector.net_displacement_dip() /
                               detector.amplitude_dip()
                         : 1.0)
                 << " peak_speed=" << detector.peak_speed_dip_s()
                 << " min_cos=" << detector.min_cosine()
                 << " vref=" << (detector.has_vref() ? "yes" : "no")
                 << " dist_since_rev=" << detector.distance_since_reversal_dip()
                 << " button=" << (detector.button_down() ? "yes" : "no")
                 << " peak_energy=" << peak_energy
                 << " scale=" << state.scale().applied_scale()
                 << " state=" << to_string(state.state())
                 << " suppressed=" << (suppressed ? "yes" : "no")
                 << " reason=" << to_string(last_fullscreen.reason)
                 << " last_sample=" << (had_sample ? "yes" : "no");
            log_info(line.str());
        }
    }

    pointer.stop();
    if (effects_enabled && effector.applied()) {
        effector.restore();
    } else if (!effects_enabled) {
        CursorEffector::restore_system_cursors();
    }

    log_info("stopped");
    return 0;
}

}  // namespace cb
