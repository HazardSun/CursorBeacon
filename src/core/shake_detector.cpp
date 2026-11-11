#include "core/shake_detector.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace cb {
namespace {

constexpr double kPi = std::numbers::pi_v<double>;

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

double cos_deg(double degrees) {
    return std::cos(degrees * kPi / 180.0);
}

}  // namespace

ShakeDetector::ShakeDetector(ShakeConfig config) : config_(config) {
    const auto finite_clamp = [](double value, double low, double high,
                                 double fallback) {
        return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
    };

    config_.window_ms = finite_clamp(config_.window_ms, 50.0, 5000.0, 600.0);
    config_.min_speed_dip_s =
        finite_clamp(config_.min_speed_dip_s, 1.0, 1'000'000.0, 800.0);
    config_.min_amplitude_dip =
        finite_clamp(config_.min_amplitude_dip, 1.0, 100'000.0, 80.0);
    config_.min_path_dip =
        finite_clamp(config_.min_path_dip, 1.0, 1'000'000.0, 250.0);
    config_.max_net_return_ratio =
        finite_clamp(config_.max_net_return_ratio, 0.0, 1.0, 0.50);
    config_.max_net_to_amplitude_ratio =
        finite_clamp(config_.max_net_to_amplitude_ratio, 0.0, 1.0, 0.35);
    config_.min_reversals = std::clamp(config_.min_reversals, 2, 16);
    config_.reversal_angle_deg =
        finite_clamp(config_.reversal_angle_deg, 90.0, 179.0, 120.0);
    config_.teleport_distance_dip =
        finite_clamp(config_.teleport_distance_dip, 1.0, 1'000'000.0, 500.0);
    config_.teleport_time_ms =
        finite_clamp(config_.teleport_time_ms, 0.1, 1000.0, 20.0);
    config_.attack_tau_ms = finite_clamp(config_.attack_tau_ms, 1.0, 2000.0, 50.0);
    config_.release_tau_ms = finite_clamp(config_.release_tau_ms, 1.0, 4000.0, 200.0);
    config_.sensitivity = std::clamp(config_.sensitivity, 1, 10);

    reset();
}

void ShakeDetector::reset() {
    sample_head_ = 0;
    sample_count_ = 0;
    reversal_head_ = 0;
    reversal_count_ = 0;
    vref_ = {};
    has_vref_ = false;
    distance_since_reversal_dip_ = 0.0;
    last_sample_us_ = 0;
    last_processed_us_ = 0;
    has_last_sample_ = false;
    last_x_ = 0;
    last_y_ = 0;
    energy_ = 0.0;
    amplitude_dip_ = 0.0;
    peak_speed_ = 0.0;
    path_length_dip_ = 0.0;
    net_displacement_dip_ = 0.0;
    min_cosine_ = 1.0;
    total_reversals_ = 0;
    button_down_ = false;
    current_dpi_ = 96;
}

void ShakeDetector::decay(int64_t now_us) {
    if (!has_last_sample_ || now_us <= last_processed_us_) {
        return;
    }

    const double dt_ms = std::clamp(
        static_cast<double>(now_us - last_processed_us_) / 1000.0, 0.1, 1000.0);
    const double alpha =
        1.0 - std::exp(-dt_ms / std::max(1.0, config_.release_tau_ms));
    energy_ = clamp01(energy_ + (0.0 - energy_) * alpha);
    last_processed_us_ = now_us;
}

double ShakeDetector::effective_min_speed() const {
    const double t = std::clamp((config_.sensitivity - 1) / 9.0, 0.0, 1.0);
    return config_.min_speed_dip_s * (1.25 - 0.5 * t);
}

double ShakeDetector::effective_min_amplitude() const {
    const double t = std::clamp((config_.sensitivity - 1) / 9.0, 0.0, 1.0);
    return config_.min_amplitude_dip * (1.25 - 0.5 * t);
}

int ShakeDetector::effective_min_reversals() const {
    if (config_.sensitivity >= 8) {
        return std::max(2, config_.min_reversals - 1);
    }
    if (config_.sensitivity <= 2) {
        return config_.min_reversals + 1;
    }
    return config_.min_reversals;
}

void ShakeDetector::push_sample(const Sample& sample) {
    if (sample_count_ == kMaxSamples) {
        sample_head_ = (sample_head_ + 1) % kMaxSamples;
        --sample_count_;
    }
    const std::size_t index = (sample_head_ + sample_count_) % kMaxSamples;
    samples_[index] = sample;
    ++sample_count_;
}

void ShakeDetector::drop_old_samples(int64_t now_us) {
    const int64_t cutoff = now_us - static_cast<int64_t>(config_.window_ms * 1000.0);
    while (sample_count_ > 0 && samples_[sample_head_].t_us < cutoff) {
        sample_head_ = (sample_head_ + 1) % kMaxSamples;
        --sample_count_;
    }
}

void ShakeDetector::add_reversal(int64_t t_us) {
    if (reversal_count_ == kMaxReversals) {
        reversal_head_ = (reversal_head_ + 1) % kMaxReversals;
        --reversal_count_;
    }
    const std::size_t index = (reversal_head_ + reversal_count_) % kMaxReversals;
    reversals_[index] = t_us;
    ++reversal_count_;
    ++total_reversals_;
}

void ShakeDetector::drop_old_reversals(int64_t now_us) {
    const int64_t cutoff = now_us - static_cast<int64_t>(config_.window_ms * 1000.0);
    while (reversal_count_ > 0 && reversals_[reversal_head_] < cutoff) {
        reversal_head_ = (reversal_head_ + 1) % kMaxReversals;
        --reversal_count_;
    }
}

void ShakeDetector::recompute_window_metrics() {
    amplitude_dip_ = 0.0;
    peak_speed_ = 0.0;
    path_length_dip_ = 0.0;
    net_displacement_dip_ = 0.0;
    if (sample_count_ < 2) {
        return;
    }

    int32_t min_x = samples_[sample_head_].x;
    int32_t max_x = min_x;
    int32_t min_y = samples_[sample_head_].y;
    int32_t max_y = min_y;

    const std::size_t last_index = (sample_head_ + sample_count_ - 1) % kMaxSamples;
    const uint32_t dpi = samples_[last_index].dpi == 0 ? 96u : samples_[last_index].dpi;
    const double dpi_scale = kDefaultDpi / static_cast<double>(dpi);

    for (std::size_t i = 1; i < sample_count_; ++i) {
        const Sample& previous = samples_[(sample_head_ + i - 1) % kMaxSamples];
        const Sample& current = samples_[(sample_head_ + i) % kMaxSamples];

        min_x = std::min(min_x, current.x);
        max_x = std::max(max_x, current.x);
        min_y = std::min(min_y, current.y);
        max_y = std::max(max_y, current.y);

        const double dt_s = static_cast<double>(current.t_us - previous.t_us) / 1'000'000.0;
        if (dt_s <= 0.0001) {
            continue;
        }
        const double dx = static_cast<double>(current.x - previous.x) * dpi_scale;
        const double dy = static_cast<double>(current.y - previous.y) * dpi_scale;
        const double segment = std::hypot(dx, dy);
        path_length_dip_ += segment;
        const double speed = segment / dt_s;
        peak_speed_ = std::max(peak_speed_, speed);
    }

    const Sample& first = samples_[sample_head_];
    const Sample& last = samples_[last_index];
    net_displacement_dip_ =
        std::hypot(static_cast<double>(last.x - first.x) * dpi_scale,
                   static_cast<double>(last.y - first.y) * dpi_scale);

    amplitude_dip_ = std::hypot(static_cast<double>(max_x - min_x),
                                static_cast<double>(max_y - min_y)) *
                     dpi_scale;
}

void ShakeDetector::add_sample(const Sample& sample) {
    if (sample.t_us < 0) {
        return;
    }

    if (!has_last_sample_) {
        last_sample_us_ = sample.t_us;
        last_processed_us_ = sample.t_us;
        last_x_ = sample.x;
        last_y_ = sample.y;
        has_last_sample_ = true;
        current_dpi_ = sample.dpi == 0 ? 96u : sample.dpi;
        push_sample(sample);
        return;
    }

    const int64_t dt_us = sample.t_us - last_sample_us_;
    if (dt_us <= 0) {
        return;
    }

    // A long gap means the cursor was not being sampled (sleep, RDP, lock).
    // Start a fresh window instead of treating the jump as velocity.
    if (dt_us > 250'000) {
        reset();
        last_sample_us_ = sample.t_us;
        last_processed_us_ = sample.t_us;
        last_x_ = sample.x;
        last_y_ = sample.y;
        has_last_sample_ = true;
        current_dpi_ = sample.dpi == 0 ? 96u : sample.dpi;
        push_sample(sample);
        return;
    }

    if (any_button_down(sample.buttons)) {
        reset();
        button_down_ = true;
        last_sample_us_ = sample.t_us;
        last_processed_us_ = sample.t_us;
        last_x_ = sample.x;
        last_y_ = sample.y;
        has_last_sample_ = true;
        current_dpi_ = sample.dpi == 0 ? 96u : sample.dpi;
        return;
    }

    button_down_ = false;

    const uint32_t dpi = sample.dpi == 0 ? 96u : sample.dpi;
    const double dt_s = static_cast<double>(dt_us) / 1'000'000.0;
    const double dt_ms = static_cast<double>(dt_us) / 1000.0;
    const double dpi_scale = kDefaultDpi / static_cast<double>(dpi);

    const int32_t dx_px = sample.x - last_x_;
    const int32_t dy_px = sample.y - last_y_;
    const double dx = static_cast<double>(dx_px) * dpi_scale;
    const double dy = static_cast<double>(dy_px) * dpi_scale;
    const double distance_dip = std::hypot(dx, dy);

    if (distance_dip > config_.teleport_distance_dip && dt_ms < config_.teleport_time_ms) {
        reset();
        last_sample_us_ = sample.t_us;
        last_processed_us_ = sample.t_us;
        last_x_ = sample.x;
        last_y_ = sample.y;
        has_last_sample_ = true;
        current_dpi_ = dpi;
        push_sample(sample);
        return;
    }

    const double speed = distance_dip / dt_s;
    const double min_speed = effective_min_speed();
    const double min_amplitude = effective_min_amplitude();

    if (speed >= min_speed) {
        const Velocity current{dx / dt_s, dy / dt_s, speed};
        if (has_vref_ && vref_.speed > 0.0) {
            const double dot = current.x * vref_.x + current.y * vref_.y;
            const double denominator = current.speed * vref_.speed;
            const double cosine = denominator > 0.0 ? std::clamp(dot / denominator, -1.0, 1.0) : 1.0;
            min_cosine_ = std::min(min_cosine_, cosine);

            if (cosine <= cos_deg(config_.reversal_angle_deg) &&
                distance_since_reversal_dip_ >= min_amplitude) {
                add_reversal(sample.t_us);
                distance_since_reversal_dip_ = 0.0;
                vref_ = current;
            }
        } else {
            vref_ = current;
            has_vref_ = true;
        }
    }

    distance_since_reversal_dip_ += distance_dip;
    push_sample(sample);
    last_sample_us_ = sample.t_us;
    last_x_ = sample.x;
    last_y_ = sample.y;
    current_dpi_ = dpi;

    drop_old_samples(sample.t_us);
    drop_old_reversals(sample.t_us);
    recompute_window_metrics();

    double raw_energy = 0.0;
    const int required_reversals = effective_min_reversals();
    const double return_ratio =
        path_length_dip_ > 1.0 ? net_displacement_dip_ / path_length_dip_ : 1.0;
    const double net_to_amplitude =
        amplitude_dip_ > 1.0 ? net_displacement_dip_ / amplitude_dip_ : 1.0;
    if (static_cast<int>(reversal_count_) >= required_reversals &&
        amplitude_dip_ >= min_amplitude &&
        path_length_dip_ >= config_.min_path_dip &&
        return_ratio <= config_.max_net_return_ratio &&
        net_to_amplitude <= config_.max_net_to_amplitude_ratio) {
        const double reversal_score =
            clamp01((static_cast<double>(reversal_count_) - 1.0) /
                    std::max(1.0, static_cast<double>(required_reversals) - 1.0));
        const double amplitude_score = clamp01(amplitude_dip_ / (min_amplitude * 1.5));
        const double speed_score = clamp01(peak_speed_ / (min_speed * 2.0));
        raw_energy = reversal_score * amplitude_score * speed_score;
    }

    const double smoothing_dt_ms =
        last_processed_us_ == 0
            ? 16.0
            : std::clamp(static_cast<double>(sample.t_us - last_processed_us_) / 1000.0, 0.1, 100.0);
    const double tau = raw_energy > energy_ ? config_.attack_tau_ms : config_.release_tau_ms;
    const double alpha = 1.0 - std::exp(-smoothing_dt_ms / std::max(1.0, tau));
    energy_ = clamp01(energy_ + (raw_energy - energy_) * alpha);
    last_processed_us_ = sample.t_us;
}

}  // namespace cb
