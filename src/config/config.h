#pragma once

#include "core/scale_controller.h"
#include "core/shake_detector.h"

#include <string>
#include <vector>

namespace cb {

struct Config {
    bool enabled{true};
    double max_scale{4.0};
    int sensitivity{5};
    double hold_ms{800.0};
    double grow_tau_ms{60.0};
    double shrink_tau_ms{180.0};
    double update_hz{30.0};
    std::string sampling_mode{"auto"};  // auto / rawinput / polling
    std::vector<std::wstring> cursor_set{L"all"};
    bool pause_on_fullscreen{true};
    bool pause_on_click{true};
    bool verbose{false};

    [[nodiscard]] static Config defaults();
    [[nodiscard]] static Config load(const std::wstring& path);
    bool save(const std::wstring& path) const;

    [[nodiscard]] ShakeConfig shake_config() const;
    [[nodiscard]] ScaleConfig scale_config() const;
};

[[nodiscard]] std::wstring default_config_path();

}  // namespace cb
