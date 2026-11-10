#pragma once

#include <cstdint>
#include <limits>

namespace cb {

// Convert a QPC counter to microseconds without overflowing.
//
// The naive `counter * 1'000'000 / frequency` overflows on a 10 MHz QPC
// after about 10.6 days of system uptime, producing a negative timestamp
// and silently breaking every time-based decision in the service.
inline std::int64_t qpc_to_us(std::int64_t counter, std::int64_t frequency) {
    if (counter <= 0 || frequency <= 0) {
        return 0;
    }

    const std::int64_t seconds = counter / frequency;
    const std::int64_t remainder = counter % frequency;

    constexpr std::int64_t kMicrosPerSecond = 1'000'000;
    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();

    if (seconds > (kMax - kMicrosPerSecond) / kMicrosPerSecond) {
        return kMax;
    }

    return seconds * kMicrosPerSecond +
           (remainder * kMicrosPerSecond) / frequency;
}

std::int64_t now_us();

}  // namespace cb
