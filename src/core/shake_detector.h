#pragma once

#include "core/types.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace cb {

struct ShakeConfig {
    double window_ms = 600.0;
    double min_speed_dip_s = 800.0;
    double min_amplitude_dip = 80.0;
    double min_path_dip = 250.0;
    double max_net_return_ratio = 0.50;
    double max_net_to_amplitude_ratio = 0.35;
    int min_reversals = 3;
    double reversal_angle_deg = 120.0;
    double teleport_distance_dip = 500.0;
    double teleport_time_ms = 20.0;
    double attack_tau_ms = 50.0;
    double release_tau_ms = 200.0;
    int sensitivity = 5;  // 1 = conservative, 10 = sensitive
};

class ShakeDetector {
public:
    explicit ShakeDetector(ShakeConfig config = {});

    void reset();
    void add_sample(const Sample& sample);
    void decay(int64_t now_us);

    [[nodiscard]] double energy() const { return energy_; }
    [[nodiscard]] int reversals() const { return static_cast<int>(reversal_count_); }
    [[nodiscard]] double amplitude_dip() const { return amplitude_dip_; }
    [[nodiscard]] double peak_speed_dip_s() const { return peak_speed_; }
    [[nodiscard]] double path_length_dip() const { return path_length_dip_; }
    [[nodiscard]] double net_displacement_dip() const { return net_displacement_dip_; }
    [[nodiscard]] int64_t last_sample_us() const { return last_sample_us_; }
    [[nodiscard]] bool button_down() const { return button_down_; }
    [[nodiscard]] bool has_vref() const { return has_vref_; }
    [[nodiscard]] double distance_since_reversal_dip() const {
        return distance_since_reversal_dip_;
    }
    [[nodiscard]] double min_cosine() const { return min_cosine_; }
    [[nodiscard]] std::uint64_t total_reversals() const { return total_reversals_; }

private:
    static constexpr std::size_t kMaxSamples = 256;
    static constexpr std::size_t kMaxReversals = 64;

    struct Velocity {
        double x{};
        double y{};
        double speed{};
    };

    void push_sample(const Sample& sample);
    void drop_old_samples(int64_t now_us);
    void drop_old_reversals(int64_t now_us);
    void recompute_window_metrics();
    void add_reversal(int64_t t_us);
    [[nodiscard]] double effective_min_speed() const;
    [[nodiscard]] double effective_min_amplitude() const;
    [[nodiscard]] int effective_min_reversals() const;

    ShakeConfig config_;

    std::array<Sample, kMaxSamples> samples_{};
    std::size_t sample_head_{};
    std::size_t sample_count_{};

    std::array<int64_t, kMaxReversals> reversals_{};
    std::size_t reversal_head_{};
    std::size_t reversal_count_{};

    Velocity vref_{};
    bool has_vref_{};
    double distance_since_reversal_dip_{};

    int64_t last_sample_us_{};
    int64_t last_processed_us_{};
    bool has_last_sample_{};
    int32_t last_x_{};
    int32_t last_y_{};
    double energy_{};
    double amplitude_dip_{};
    double peak_speed_{};
    double path_length_dip_{};
    double net_displacement_dip_{};
    double min_cosine_{1.0};
    std::uint64_t total_reversals_{};
    bool button_down_{};
    uint32_t current_dpi_{96};
};

}  // namespace cb
