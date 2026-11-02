#include "util/log.h"

#include <windows.h>

#include <cstdio>
#include <mutex>

namespace cb {
namespace {

std::mutex g_mutex;
LogLevel g_level = LogLevel::Info;

const char* level_name(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:
            return "debug";
        case LogLevel::Info:
            return "info";
        case LogLevel::Warn:
            return "warn";
        case LogLevel::Error:
            return "error";
    }
    return "info";
}

}  // namespace

void set_log_level(LogLevel level) {
    std::lock_guard lock(g_mutex);
    g_level = level;
}

LogLevel log_level() {
    std::lock_guard lock(g_mutex);
    return g_level;
}

void log(LogLevel level, const std::string& message) {
    std::lock_guard lock(g_mutex);
    if (level < g_level) {
        return;
    }
    SYSTEMTIME time{};
    GetLocalTime(&time);
    (void)std::fprintf(stderr,
                       "[%02u:%02u:%02u.%03u][cursorbeacon %s] %s\n",
                       time.wHour, time.wMinute, time.wSecond,
                       time.wMilliseconds, level_name(level), message.c_str());
    (void)std::fflush(stderr);
}

}  // namespace cb
