#include "util/time.h"

#include <doctest/doctest.h>

TEST_CASE("QPC conversion avoids 64-bit overflow") {
    constexpr std::int64_t frequency = 10'000'000;  // common 10 MHz QPC

    CHECK(cb::qpc_to_us(0, frequency) == 0);
    CHECK(cb::qpc_to_us(frequency, frequency) == 1'000'000);

    // 10 days of uptime is still below the old overflow point.
    constexpr std::int64_t ten_days = 10LL * 24 * 60 * 60 * frequency;
    CHECK(cb::qpc_to_us(ten_days, frequency) == 864'000'000'000LL);

    // 11 days (950,400 s) exceeds 2^63 / 1e6 counts, so the old
    // counter * 1'000'000 expression overflowed here.
    constexpr std::int64_t eleven_days = 11LL * 24 * 60 * 60 * frequency;
    CHECK(cb::qpc_to_us(eleven_days, frequency) == 950'400'000'000LL);

    CHECK(cb::qpc_to_us(frequency + frequency / 2, frequency) == 1'500'000);
}

TEST_CASE("QPC conversion handles invalid input") {
    CHECK(cb::qpc_to_us(-1, 10'000'000) == 0);
    CHECK(cb::qpc_to_us(1000, 0) == 0);
}
