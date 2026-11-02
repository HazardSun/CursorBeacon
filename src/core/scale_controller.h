#pragma once

#include <cstdint>
#include <vector>

namespace cb {

struct ScaleConfig {
    double max_scale = 4.0;
    double min_apply_scale = 1.10;
    double hold_ms = 800.0;
    double grow_tau_ms = 60.0;
    double shrink_tau_ms = 180.0;
    double update_hz = 30.0;
    double start_energy = 0.35;
    double activation_ms = 250.0;
    double cooldown_ms = 600.0;
    std::vector<double> ladder{1.0, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0, 8.0};
};

class ScaleController {
public:
    struct Output {
        double scale{1.0};
        bool changed{};
        bool restore{};
    };

    explicit ScaleController(ScaleConfig config = {});

    Output update(double energy, int64_t now_us);
    void reset(int64_t now_us = 0);

    [[nodiscard]] double applied_scale() const { return applied_; }
    [[nodiscard]] double target_scale() const { return target_; }
    [[nodiscard]] bool holding() const { return target_ > 1.001; }
    [[nodiscard]] const ScaleConfig& config() const { return config_; }

private:
    [[nodiscard]] double quantize(double scale) const;

    ScaleConfig config_;
    double applied_{1.0};
    double target_{1.0};
    double peak_{1.0};
    double emitted_{1.0};
    int64_t hold_until_us_{};
    int64_t last_update_us_{};
    int64_t last_emit_us_{};
    int64_t above_since_us_{};
    int64_t cooldown_until_us_{};
};

}  // namespace cb
