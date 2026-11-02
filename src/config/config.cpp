#include "config/config.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace cb {
namespace {

std::string trim(std::string value) {
    const auto not_space = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(),
                value.end());
    return value;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::wstring widen_ascii(const std::string& value) {
    return std::wstring(value.begin(), value.end());
}

std::string narrow_ascii(const std::wstring& value) {
    return std::string(value.begin(), value.end());
}

std::string read_text(const std::wstring& path) {
    constexpr std::size_t kMaxConfigBytes = 1u << 20;

    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return {};
    }

    std::string result;
    char buffer[4096];
    DWORD read = 0;
    while (ReadFile(file, buffer, sizeof(buffer), &read, nullptr) && read > 0) {
        result.append(buffer, read);
        if (result.size() >= kMaxConfigBytes) {
            result.resize(kMaxConfigBytes);
            break;
        }
    }
    CloseHandle(file);
    return result;
}

bool write_text(const std::wstring& path, const std::string& text) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    const bool ok = WriteFile(file, text.data(), static_cast<DWORD>(text.size()),
                              &written, nullptr) != FALSE &&
                    written == text.size();
    CloseHandle(file);
    return ok;
}

bool parse_bool(const std::string& value, bool fallback) {
    const std::string normalized = lower(trim(value));
    if (normalized == "1" || normalized == "true" || normalized == "yes" ||
        normalized == "on") {
        return true;
    }
    if (normalized == "0" || normalized == "false" || normalized == "no" ||
        normalized == "off") {
        return false;
    }
    return fallback;
}

double parse_double(const std::string& value, double fallback) {
    try {
        return std::stod(trim(value));
    } catch (...) {
        return fallback;
    }
}

int parse_int(const std::string& value, int fallback) {
    try {
        return std::stoi(trim(value));
    } catch (...) {
        return fallback;
    }
}

std::vector<std::wstring> parse_cursor_set(const std::string& value) {
    std::vector<std::wstring> result;
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, ',')) {
        item = trim(item);
        if (!item.empty()) {
            result.push_back(widen_ascii(item));
        }
    }
    return result;
}

}  // namespace

Config Config::defaults() {
    return Config{};
}

Config Config::load(const std::wstring& path) {
    Config config = defaults();
    const std::string text = read_text(path);
    if (text.empty()) {
        return config;
    }

    std::stringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }
        const std::string key = lower(trim(line.substr(0, separator)));
        const std::string value = trim(line.substr(separator + 1));

        if (key == "enabled") {
            config.enabled = parse_bool(value, config.enabled);
        } else if (key == "max_scale") {
            config.max_scale = parse_double(value, config.max_scale);
        } else if (key == "sensitivity") {
            config.sensitivity = parse_int(value, config.sensitivity);
        } else if (key == "hold_ms") {
            config.hold_ms = parse_double(value, config.hold_ms);
        } else if (key == "grow_tau_ms") {
            config.grow_tau_ms = parse_double(value, config.grow_tau_ms);
        } else if (key == "shrink_tau_ms") {
            config.shrink_tau_ms = parse_double(value, config.shrink_tau_ms);
        } else if (key == "update_hz") {
            config.update_hz = parse_double(value, config.update_hz);
        } else if (key == "sampling_mode") {
            config.sampling_mode = lower(value);
        } else if (key == "cursor_set") {
            config.cursor_set = parse_cursor_set(value);
        } else if (key == "pause_on_fullscreen") {
            config.pause_on_fullscreen = parse_bool(value, config.pause_on_fullscreen);
        } else if (key == "pause_on_click") {
            config.pause_on_click = parse_bool(value, config.pause_on_click);
        }
    }

    config.max_scale = std::isfinite(config.max_scale)
                           ? std::clamp(config.max_scale, 1.0, 8.0)
                           : 4.0;
    config.sensitivity = std::clamp(config.sensitivity, 1, 10);
    config.hold_ms = std::isfinite(config.hold_ms)
                         ? std::clamp(config.hold_ms, 0.0, 5000.0)
                         : 800.0;
    config.grow_tau_ms = std::isfinite(config.grow_tau_ms)
                             ? std::clamp(config.grow_tau_ms, 1.0, 2000.0)
                             : 60.0;
    config.shrink_tau_ms = std::isfinite(config.shrink_tau_ms)
                               ? std::clamp(config.shrink_tau_ms, 1.0, 4000.0)
                               : 180.0;
    config.update_hz = std::isfinite(config.update_hz)
                           ? std::clamp(config.update_hz, 1.0, 120.0)
                           : 30.0;
    if (config.cursor_set.empty()) {
        config.cursor_set = defaults().cursor_set;
    }
    return config;
}

bool Config::save(const std::wstring& path) const {
    std::ostringstream output;
    output << "# CursorBeacon configuration\n";
    output << "enabled=" << (enabled ? "true" : "false") << "\n";
    output << "max_scale=" << max_scale << "\n";
    output << "sensitivity=" << sensitivity << "\n";
    output << "hold_ms=" << hold_ms << "\n";
    output << "grow_tau_ms=" << grow_tau_ms << "\n";
    output << "shrink_tau_ms=" << shrink_tau_ms << "\n";
    output << "update_hz=" << update_hz << "\n";
    output << "sampling_mode=" << sampling_mode << "\n";
    output << "cursor_set=";
    for (std::size_t i = 0; i < cursor_set.size(); ++i) {
        if (i != 0) {
            output << ",";
        }
        output << narrow_ascii(cursor_set[i]);
    }
    output << "\n";
    output << "pause_on_fullscreen=" << (pause_on_fullscreen ? "true" : "false") << "\n";
    output << "pause_on_click=" << (pause_on_click ? "true" : "false") << "\n";
    return write_text(path, output.str());
}

ShakeConfig Config::shake_config() const {
    ShakeConfig shake;
    shake.sensitivity = sensitivity;
    return shake;
}

ScaleConfig Config::scale_config() const {
    ScaleConfig scale;
    scale.max_scale = max_scale;
    scale.hold_ms = hold_ms;
    scale.grow_tau_ms = grow_tau_ms;
    scale.shrink_tau_ms = shrink_tau_ms;
    scale.update_hz = update_hz;
    return scale;
}

std::wstring default_config_path() {
    std::array<wchar_t, MAX_PATH> buffer{};
    const DWORD length = GetEnvironmentVariableW(
        L"APPDATA", buffer.data(), static_cast<DWORD>(buffer.size()));
    std::wstring base = (length > 0 && length < buffer.size())
                            ? std::wstring(buffer.data(), length)
                            : L".";
    const std::wstring directory = base + L"\\CursorBeacon";
    CreateDirectoryW(directory.c_str(), nullptr);
    return directory + L"\\config.ini";
}

}  // namespace cb
