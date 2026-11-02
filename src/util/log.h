#pragma once

#include <string>

namespace cb {

enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warn = 2,
    Error = 3,
};

void set_log_level(LogLevel level);
[[nodiscard]] LogLevel log_level();
void log(LogLevel level, const std::string& message);

inline void log_debug(const std::string& message) { log(LogLevel::Debug, message); }
inline void log_info(const std::string& message) { log(LogLevel::Info, message); }
inline void log_warn(const std::string& message) { log(LogLevel::Warn, message); }
inline void log_error(const std::string& message) { log(LogLevel::Error, message); }

}  // namespace cb
