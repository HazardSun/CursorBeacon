#pragma once

#include "core/scale_controller.h"

#include <cstdint>

namespace cb {

enum class State {
    Idle,
    Shaking,
    Holding,
    Shrinking,
    Suppressed,
};

class StateMachine {
public:
    struct Decision {
        State state{State::Idle};
        bool apply{};
        double scale{1.0};
        bool restore{};
    };

    explicit StateMachine(ScaleConfig config = {});

    Decision update(double energy, bool suppressed, int64_t now_us);
    Decision suppress(int64_t now_us);
    Decision request_restore(int64_t now_us = 0);
    void reset(int64_t now_us = 0);

    [[nodiscard]] State state() const { return state_; }
    [[nodiscard]] ScaleController& scale() { return scale_; }
    [[nodiscard]] const ScaleController& scale() const { return scale_; }

private:
    ScaleController scale_;
    State state_{State::Idle};
    double start_energy_{0.12};
};

[[nodiscard]] const char* to_string(State state);

}  // namespace cb
