#include "core/shake_detector.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace {

constexpr int64_t kDt8Ms = 8000;
constexpr int64_t kDt4Ms = 4000;

cb::Sample sample_at(int64_t t_us, int32_t x, int32_t y, uint8_t buttons = 0,
                     uint32_t dpi = 96) {
    return cb::Sample{t_us, x, y, buttons, dpi};
}

double feed_shake(cb::ShakeDetector& detector, int64_t duration_ms, int64_t dt_us,
                  double amplitude_px, double frequency_hz, uint32_t dpi = 96,
                  uint8_t buttons = 0) {
    const double step_s = static_cast<double>(dt_us) / 1'000'000.0;
    const int steps = static_cast<int>(duration_ms / 1000.0 / step_s);
    const double two_pi = 2.0 * std::numbers::pi_v<double>;
    double peak_energy = 0.0;
    for (int i = 0; i < steps; ++i) {
        const double t_s = static_cast<double>(i) * step_s;
        const int32_t x = 1000 + static_cast<int32_t>(amplitude_px * std::sin(two_pi * frequency_hz * t_s));
        const int32_t y = 500 + static_cast<int32_t>(amplitude_px * 0.5 * std::sin(two_pi * frequency_hz * t_s + 1.0));
        detector.add_sample(sample_at(static_cast<int64_t>(i) * dt_us, x, y, buttons, dpi));
        peak_energy = std::max(peak_energy, detector.energy());
    }
    return peak_energy;
}

double feed_positions(cb::ShakeDetector& detector, const std::vector<int32_t>& xs,
                      int64_t dt_us) {
    double peak_energy = 0.0;
    for (std::size_t i = 0; i < xs.size(); ++i) {
        detector.add_sample(
            sample_at(static_cast<int64_t>(i) * dt_us, xs[i], 400));
        peak_energy = std::max(peak_energy, detector.energy());
    }
    return peak_energy;
}

}  // namespace

TEST_CASE("stationary cursor does not trigger") {
    cb::ShakeDetector detector;
    for (int i = 0; i < 200; ++i) {
        detector.add_sample(sample_at(static_cast<int64_t>(i) * kDt8Ms, 800, 600));
    }
    CHECK(detector.energy() == doctest::Approx(0.0));
    CHECK(detector.reversals() == 0);
}

TEST_CASE("fast straight movement does not trigger") {
    cb::ShakeDetector detector;
    int32_t x = 0;
    for (int i = 0; i < 100; ++i) {
        detector.add_sample(sample_at(static_cast<int64_t>(i) * kDt8Ms, x, 400));
        x += 40;
    }
    CHECK(detector.reversals() == 0);
    CHECK(detector.energy() < 0.05);
}

TEST_CASE("sustained shake produces high energy") {
    cb::ShakeDetector detector;
    feed_shake(detector, 1600, kDt8Ms, 200.0, 3.0);
    CHECK(detector.reversals() >= 3);
    CHECK(detector.amplitude_dip() > 200.0);
    CHECK(detector.energy() > 0.5);
}

TEST_CASE("two reversals are not enough") {
    cb::ShakeDetector detector;
    const int32_t xs[] = {0, 120, 240, 360, 240, 120, 0};
    for (int i = 0; i < 7; ++i) {
        detector.add_sample(sample_at(static_cast<int64_t>(i) * 20'000, xs[i], 400));
    }
    CHECK(detector.reversals() < 3);
    CHECK(detector.energy() < 0.05);
}

TEST_CASE("small high frequency jitter is ignored") {
    cb::ShakeDetector detector;
    for (int i = 0; i < 200; ++i) {
        const int32_t x = 1000 + ((i % 2) == 0 ? 8 : -8);
        detector.add_sample(sample_at(static_cast<int64_t>(i) * kDt4Ms, x, 500));
    }
    CHECK(detector.energy() < 0.05);
}

TEST_CASE("mouse button press suppresses detection") {
    cb::ShakeDetector detector;
    feed_shake(detector, 1200, kDt8Ms, 250.0, 3.5, 96, cb::kButtonLeft);
    CHECK(detector.button_down());
    CHECK(detector.energy() == doctest::Approx(0.0));
}

TEST_CASE("teleport resets the detector") {
    cb::ShakeDetector detector;
    detector.add_sample(sample_at(0, 100, 100));
    detector.add_sample(sample_at(kDt8Ms, 900, 100));
    CHECK(detector.reversals() == 0);
    CHECK(detector.energy() == doctest::Approx(0.0));
}

TEST_CASE("DPI normalization keeps amplitude in DIPs") {
    cb::ShakeDetector detector_96;
    cb::ShakeDetector detector_192;
    feed_shake(detector_96, 1600, kDt8Ms, 200.0, 3.0, 96);
    feed_shake(detector_192, 1600, kDt8Ms, 400.0, 3.0, 192);
    CHECK(detector_96.amplitude_dip() == doctest::Approx(detector_192.amplitude_dip()).epsilon(0.05));
    CHECK(detector_96.reversals() == detector_192.reversals());
}

TEST_CASE("zigzag with net progress does not trigger") {
    cb::ShakeDetector detector;
    const std::vector<int32_t> xs{0,   100, 200, 150, 250, 300,
                                  250, 350, 450, 400, 500, 600};
    const double peak = feed_positions(detector, xs, 20'000);

    CHECK(detector.reversals() >= 4);
    CHECK(detector.net_displacement_dip() > detector.path_length_dip() * 0.50);
    CHECK(detector.net_displacement_dip() > detector.amplitude_dip() * 0.35);
    CHECK(peak < 0.05);
}

TEST_CASE("energy decays when no new samples arrive") {
    cb::ShakeDetector detector;
    feed_shake(detector, 1600, kDt8Ms, 200.0, 3.0);
    CHECK(detector.energy() > 0.5);

    const int64_t last = detector.last_sample_us();
    detector.decay(last + 1'500'000);
    CHECK(detector.energy() < 0.05);
}

TEST_CASE("higher sensitivity lowers thresholds") {
    cb::ShakeConfig low_config;
    low_config.sensitivity = 1;
    cb::ShakeConfig high_config;
    high_config.sensitivity = 10;

    cb::ShakeDetector low(low_config);
    cb::ShakeDetector high(high_config);
    const double low_peak = feed_shake(low, 2500, kDt8Ms, 40.0, 3.0);
    const double high_peak = feed_shake(high, 2500, kDt8Ms, 40.0, 3.0);

    CHECK(high_peak > 0.05);
    CHECK(low_peak < 0.02);
    CHECK(high_peak > low_peak);
}

TEST_CASE("high sensitivity accepts two reversals") {
    cb::ShakeConfig high_config;
    high_config.sensitivity = 10;
    cb::ShakeDetector high(high_config);
    cb::ShakeDetector normal;

    const std::vector<int32_t> xs{0, 120, 240, 360, 240, 120, 0, 120, 240, 360};
    const double normal_peak = feed_positions(normal, xs, 20'000);
    const double high_peak = feed_positions(high, xs, 20'000);

    CHECK(high.reversals() >= 2);
    CHECK(high_peak > 0.1);
    CHECK(normal_peak < 0.05);
}
