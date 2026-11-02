#include "app/self_test.h"

#include "core/fullscreen_rules.h"
#include "core/scale_controller.h"
#include "core/shake_detector.h"
#include "core/state_machine.h"

#include <cmath>
#include <cstdio>
#include <numbers>

namespace cb {
namespace {

int g_failures = 0;

void check(bool condition, const char* name) {
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", name);
    if (!condition) {
        ++g_failures;
    }
}

Sample sample_at(int64_t t_us, int32_t x, int32_t y) {
    return Sample{t_us, x, y, 0, 96};
}

void feed_shake(ShakeDetector& detector, int duration_ms, int64_t dt_us,
                double amplitude, double frequency) {
    const double step_s = static_cast<double>(dt_us) / 1'000'000.0;
    const int steps = static_cast<int>(duration_ms / 1000.0 / step_s);
    const double two_pi = 2.0 * std::numbers::pi_v<double>;
    for (int i = 0; i < steps; ++i) {
        const double t_s = static_cast<double>(i) * step_s;
        const int32_t x =
            1000 + static_cast<int32_t>(amplitude * std::sin(two_pi * frequency * t_s));
        const int32_t y =
            500 + static_cast<int32_t>(amplitude * 0.5 * std::sin(two_pi * frequency * t_s + 1.0));
        detector.add_sample(sample_at(static_cast<int64_t>(i) * dt_us, x, y));
    }
}

}  // namespace

int run_self_tests() {
    g_failures = 0;

    {
        ShakeDetector detector;
        for (int i = 0; i < 200; ++i) {
            detector.add_sample(sample_at(static_cast<int64_t>(i) * 8'000, 800, 600));
        }
        check(detector.energy() < 0.01, "stationary cursor does not trigger");
    }

    {
        ShakeDetector detector;
        feed_shake(detector, 1600, 8'000, 200.0, 3.0);
        check(detector.reversals() >= 3, "shake produces reversals");
        check(detector.energy() > 0.5, "shake produces high energy");
        const int64_t last = detector.last_sample_us();
        detector.decay(last + 1'500'000);
        check(detector.energy() < 0.05, "energy decays without new samples");
    }

    {
        ScaleConfig config;
        config.max_scale = 4.0;
        config.hold_ms = 300.0;
        config.update_hz = 60.0;
        ScaleController controller(config);
        int64_t now = 0;
        bool applied = false;
        for (int i = 0; i < 120; ++i) {
            now += 10'000;
            applied = controller.update(1.0, now).changed || applied;
        }
        check(applied, "scale controller emits changes");
        check(controller.applied_scale() > 3.0, "scale controller grows");

        bool restored = false;
        for (int i = 0; i < 200; ++i) {
            now += 10'000;
            restored = controller.update(0.0, now).restore || restored;
        }
        check(restored, "scale controller restores");
        check(controller.applied_scale() < 1.01, "scale controller returns to 1x");
    }

    {
        ScaleConfig config;
        config.max_scale = 4.0;
        config.hold_ms = 300.0;
        config.update_hz = 60.0;
        StateMachine machine(config);
        int64_t now = 0;
        for (int i = 0; i < 80; ++i) {
            now += 10'000;
            machine.update(1.0, false, now);
        }
        const auto decision = machine.update(0.0, true, now);
        check(decision.restore, "suppression requests restore");
        check(machine.state() == State::Suppressed, "suppression state");
    }

    {
        const Rect monitor{0, 0, 1920, 1080};
        const Rect work{0, 0, 1920, 1040};
        WindowInfo window;
        window.visible = true;
        window.frame = monitor;
        window.monitor = monitor;
        window.popup = true;
        check(is_fullscreen_on_monitor(monitor, work, window, true, false),
              "borderless fullscreen rule");

        window.popup = false;
        window.has_caption = true;
        window.frame = work;
        check(!is_fullscreen_on_monitor(monitor, work, window, true, false),
              "maximized window is not fullscreen");
        check(!is_fullscreen_on_monitor(monitor, work, window, false, true),
              "other monitor is not suppressed");
    }

    std::printf("self-test: %d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}

}  // namespace cb
