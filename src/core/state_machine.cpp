#include "core/state_machine.h"

#include <utility>

namespace cb {

StateMachine::StateMachine(ScaleConfig config)
    : scale_(std::move(config)), start_energy_(scale_.config().start_energy) {}

StateMachine::Decision StateMachine::update(double energy, bool suppressed, int64_t now_us) {
    if (suppressed) {
        if (state_ != State::Suppressed) {
            scale_.reset(now_us);
            state_ = State::Suppressed;
            return {state_, false, 1.0, true};
        }
        return {state_, false, 1.0, false};
    }

    if (state_ == State::Suppressed) {
        state_ = State::Idle;
    }

    const ScaleController::Output output = scale_.update(energy, now_us);

    if (energy > start_energy_) {
        state_ = State::Shaking;
    } else if (output.scale > 1.001) {
        state_ = scale_.holding() ? State::Holding : State::Shrinking;
    } else {
        state_ = State::Idle;
    }

    return {state_, output.changed, output.scale, output.restore};
}

StateMachine::Decision StateMachine::suppress(int64_t now_us) {
    if (state_ == State::Suppressed) {
        return {state_, false, 1.0, false};
    }
    scale_.reset(now_us);
    state_ = State::Suppressed;
    return {state_, false, 1.0, true};
}

StateMachine::Decision StateMachine::request_restore(int64_t now_us) {
    scale_.reset(now_us);
    const bool was_enlarged = state_ != State::Idle && state_ != State::Suppressed;
    state_ = State::Idle;
    return {state_, false, 1.0, was_enlarged};
}

void StateMachine::reset(int64_t now_us) {
    scale_.reset(now_us);
    state_ = State::Idle;
}

const char* to_string(State state) {
    switch (state) {
        case State::Idle:
            return "idle";
        case State::Shaking:
            return "shaking";
        case State::Holding:
            return "holding";
        case State::Shrinking:
            return "shrinking";
        case State::Suppressed:
            return "suppressed";
    }
    return "unknown";
}

}  // namespace cb
