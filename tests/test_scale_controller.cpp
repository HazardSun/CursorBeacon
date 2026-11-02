#include "core/scale_controller.h"

#include <doctest/doctest.h>

namespace {

cb::ScaleConfig test_config() {
    cb::ScaleConfig config;
    config.max_scale = 4.0;
    config.hold_ms = 300.0;
    config.grow_tau_ms = 60.0;
    config.shrink_tau_ms = 120.0;
    config.update_hz = 60.0;
    return config;
}

}  // namespace

TEST_CASE("scale grows with energy and restores after hold") {
    cb::ScaleController controller(test_config());
    int64_t now = 0;
    bool saw_change = false;
    double last_scale = 1.0;

    for (int i = 0; i < 120; ++i) {
        now += 10'000;
        const auto output = controller.update(1.0, now);
        if (output.changed) {
            saw_change = true;
            last_scale = output.scale;
            CHECK(output.scale >= 1.0);
            CHECK(output.scale <= 4.0);
        }
    }

    CHECK(saw_change);
    CHECK(last_scale >= 3.0);
    CHECK(controller.applied_scale() > 3.0);

    bool restored = false;
    for (int i = 0; i < 200; ++i) {
        now += 10'000;
        const auto output = controller.update(0.0, now);
        if (output.restore) {
            restored = true;
        }
    }

    CHECK(restored);
    CHECK(controller.applied_scale() == doctest::Approx(1.0));
}

TEST_CASE("scale does not apply below the minimum threshold") {
    auto config = test_config();
    config.start_energy = 0.5;
    cb::ScaleController controller(config);

    bool changed = false;
    int64_t now = 0;
    for (int i = 0; i < 30; ++i) {
        now += 10'000;
        changed = controller.update(0.2, now).changed || changed;
    }
    CHECK_FALSE(changed);
    CHECK(controller.applied_scale() == doctest::Approx(1.0));
}

TEST_CASE("reset returns to normal immediately") {
    cb::ScaleController controller(test_config());
    int64_t now = 0;
    for (int i = 0; i < 100; ++i) {
        now += 10'000;
        controller.update(1.0, now);
    }
    CHECK(controller.applied_scale() > 1.0);

    controller.reset(now);
    CHECK(controller.applied_scale() == doctest::Approx(1.0));
    CHECK(controller.target_scale() == doctest::Approx(1.0));
}
