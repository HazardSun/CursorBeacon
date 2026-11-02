#include "core/scale_controller.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace cb {
namespace {

double smoothstep(double edge0, double edge1, double value) {
    if (edge1 <= edge0) {
        return value >= edge1 ? 1.0 : 0.0;
    }
    const double t = std::clamp((value - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

}  // namespace

ScaleController::ScaleController(ScaleConfig config) : config_(std::move(config)) {
    const auto finite_clamp = [](double value, double low, double high,
                                 double fallback) {
        return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
    };

    config_.max_scale = finite_clamp(config_.max_scale, 1.0, 8.0, 4.0);
    config_.hold_ms = finite_clamp(config_.hold_ms, 0.0, 5000.0, 800.0);
    config_.grow_tau_ms = finite_clamp(config_.grow_tau_ms, 1.0, 2000.0, 60.0);
    config_.shrink_tau_ms = finite_clamp(config_.shrink_tau_ms, 1.0, 4000.0, 180.0);
    config_.update_hz = finite_clamp(config_.update_hz, 1.0, 120.0, 30.0);
    config_.start_energy = finite_clamp(config_.start_energy, 0.0, 0.99, 0.35);
    config_.activation_ms =
        finite_clamp(config_.activation_ms, 0.0, 2000.0, 250.0);
    config_.cooldown_ms = finite_clamp(config_.cooldown_ms, 0.0, 5000.0, 600.0);
    config_.min_apply_scale =
        finite_clamp(config_.min_apply_scale, 1.0, config_.max_scale, 1.10);

    std::vector<double> ladder;
    ladder.reserve(config_.ladder.size() + 1);
    for (double step : config_.ladder) {
        if (std::isfinite(step) && step >= 1.0 && step <= 8.0) {
            ladder.push_back(step);
        }
    }
    config_.ladder = std::move(ladder);
    if (config_.ladder.empty()) {
        config_.ladder = {1.0, 1.25, 1.5, 2.0, 3.0, 4.0};
    }
    std::sort(config_.ladder.begin(), config_.ladder.end());
    config_.ladder.erase(
        std::unique(config_.ladder.begin(), config_.ladder.end()),
        config_.ladder.end());
    if (config_.ladder.front() > 1.0) {
        config_.ladder.insert(config_.ladder.begin(), 1.0);
    }
}

double ScaleController::quantize(double scale) const {
    if (!std::isfinite(scale)) {
        scale = 1.0;
    }
    scale = std::clamp(scale, 1.0, config_.max_scale);

    double best = 1.0;
    double best_distance = std::abs(scale - best);
    for (double step : config_.ladder) {
        if (step > config_.max_scale + 1e-9) {
            continue;
        }
        const double distance = std::abs(scale - step);
        if (distance < best_distance) {
            best = step;
            best_distance = distance;
        }
    }

    const double max_distance = std::abs(scale - config_.max_scale);
    if (max_distance < best_distance) {
        best = config_.max_scale;
    }

    return best <= 1.001 ? 1.0 : best;
}

ScaleController::Output ScaleController::update(double energy, int64_t now_us) {
    if (!std::isfinite(energy)) {
        energy = 0.0;
    }
    energy = std::clamp(energy, 0.0, 1.0);
    if (last_update_us_ == 0) {
        last_update_us_ = now_us;
        last_emit_us_ = now_us;
        return {applied_, false, false};
    }

    const double dt_ms =
        std::clamp(static_cast<double>(now_us - last_update_us_) / 1000.0, 0.1, 100.0);
    last_update_us_ = now_us;

    const bool in_cooldown = now_us < cooldown_until_us_;
    const bool above = energy > config_.start_energy;

    if (above && !in_cooldown) {
        if (above_since_us_ == 0) {
            above_since_us_ = now_us;
        }
    } else {
        above_since_us_ = 0;
    }

    const bool activated =
        above && !in_cooldown &&
        now_us - above_since_us_ >=
            static_cast<std::int64_t>(config_.activation_ms * 1000.0);

    if (activated) {
        hold_until_us_ = now_us + static_cast<std::int64_t>(config_.hold_ms * 1000.0);
        const double desired =
            1.0 + (config_.max_scale - 1.0) *
                      smoothstep(config_.start_energy, 0.95, energy);
        peak_ = std::max(peak_, desired);
        target_ = peak_;
    } else if (!in_cooldown && peak_ > 1.001 && now_us < hold_until_us_) {
        target_ = peak_;
    } else {
        target_ = 1.0;
        peak_ = 1.0;
    }

    const double tau_ms = target_ > applied_ ? config_.grow_tau_ms : config_.shrink_tau_ms;
    const double alpha = 1.0 - std::exp(-dt_ms / std::max(1.0, tau_ms));
    applied_ += (target_ - applied_) * alpha;

    if (target_ <= 1.0 && applied_ < 1.02) {
        applied_ = 1.0;
    }
    if (applied_ < config_.min_apply_scale) {
        applied_ = 1.0;
    }

    const double quantized = quantize(applied_);
    const bool restore = quantized <= 1.001 && emitted_ > 1.001;
    const bool changed = std::abs(quantized - emitted_) > 1e-6;
    const int64_t min_interval_us =
        static_cast<int64_t>(1'000'000.0 / config_.update_hz);
    const bool rate_ok = now_us - last_emit_us_ >= min_interval_us;

    if (restore || (changed && rate_ok)) {
        emitted_ = quantized;
        last_emit_us_ = now_us;
        if (restore) {
            cooldown_until_us_ =
                now_us + static_cast<std::int64_t>(config_.cooldown_ms * 1000.0);
            above_since_us_ = 0;
            peak_ = 1.0;
            target_ = 1.0;
        }
        return {quantized, true, restore};
    }

    return {emitted_, false, false};
}

void ScaleController::reset(int64_t now_us) {
    applied_ = 1.0;
    target_ = 1.0;
    peak_ = 1.0;
    emitted_ = 1.0;
    hold_until_us_ = 0;
    last_update_us_ = now_us;
    last_emit_us_ = now_us;
    above_since_us_ = 0;
    cooldown_until_us_ = 0;
}

}  // namespace cb
