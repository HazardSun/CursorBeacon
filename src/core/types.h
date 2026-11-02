#pragma once

#include <cstdint>
#include <string>

namespace cb {

struct Point {
    int32_t x{};
    int32_t y{};
};

struct Rect {
    int32_t left{};
    int32_t top{};
    int32_t right{};
    int32_t bottom{};

    [[nodiscard]] int32_t width() const { return right - left; }
    [[nodiscard]] int32_t height() const { return bottom - top; }
    [[nodiscard]] bool empty() const { return right <= left || bottom <= top; }

    [[nodiscard]] bool contains(const Point& point) const {
        return point.x >= left && point.x < right && point.y >= top && point.y < bottom;
    }

    [[nodiscard]] bool covers(const Rect& other, int32_t tolerance = 0) const {
        return left <= other.left + tolerance && top <= other.top + tolerance &&
               right >= other.right - tolerance && bottom >= other.bottom - tolerance;
    }
};

struct Sample {
    int64_t t_us{};
    int32_t x{};
    int32_t y{};
    uint8_t buttons{};  // bit0 left, bit1 right, bit2 middle
    uint32_t dpi{96};
};

enum ButtonMask : uint8_t {
    kButtonLeft = 1,
    kButtonRight = 2,
    kButtonMiddle = 4,
};

[[nodiscard]] inline bool any_button_down(uint8_t buttons) {
    return buttons != 0;
}

constexpr double kDefaultDpi = 96.0;

[[nodiscard]] inline double px_to_dip(int32_t pixels, uint32_t dpi) {
    return dpi == 0 ? static_cast<double>(pixels)
                    : static_cast<double>(pixels) * kDefaultDpi / static_cast<double>(dpi);
}

struct MonitorInfo {
    uintptr_t handle{};
    Rect monitor;
    Rect work;
    bool primary{};
    uint32_t dpi{96};
    std::wstring device;
};

}  // namespace cb
