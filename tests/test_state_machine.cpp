#include "core/state_machine.h"

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

TEST_CASE("state machine applies, holds and restores") {
    cb::StateMachine machine(test_config());
    int64_t now = 0;
    bool applied = false;

    for (int i = 0; i < 120; ++i) {
        now += 10'000;
        const auto decision = machine.update(1.0, false, now);
        applied = applied || decision.apply;
        CHECK(decision.state == cb::State::Shaking);
    }
    CHECK(applied);
    CHECK(machine.scale().applied_scale() > 3.0);

    bool restored = false;
    for (int i = 0; i < 200; ++i) {
        now += 10'000;
        const auto decision = machine.update(0.0, false, now);
        if (decision.restore) {
            restored = true;
        }
    }
    CHECK(restored);
    CHECK(machine.state() == cb::State::Idle);
}

TEST_CASE("suppression restores once and stays suppressed") {
    cb::StateMachine machine(test_config());
    int64_t now = 0;
    for (int i = 0; i < 80; ++i) {
        now += 10'000;
        machine.update(1.0, false, now);
    }

    const auto first = machine.update(0.0, true, now);
    CHECK(first.restore);
    CHECK(first.state == cb::State::Suppressed);

    const auto second = machine.update(0.0, true, now + 10'000);
    CHECK_FALSE(second.restore);
    CHECK(second.state == cb::State::Suppressed);

    const auto resumed = machine.update(0.0, false, now + 20'000);
    CHECK_FALSE(resumed.restore);
    CHECK(resumed.state == cb::State::Idle);
}

TEST_CASE("request_restore leaves the machine idle") {
    cb::StateMachine machine(test_config());
    int64_t now = 0;
    for (int i = 0; i < 80; ++i) {
        now += 10'000;
        machine.update(1.0, false, now);
    }

    const auto decision = machine.request_restore(now);
    CHECK(decision.restore);
    CHECK(decision.state == cb::State::Idle);
    CHECK(machine.scale().applied_scale() == doctest::Approx(1.0));
}
