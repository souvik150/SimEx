#pragma once

#include <cstring>

#include "utils/Logger.h"

namespace logging::detail {
inline const char* trimPath(const char* file) {
#ifdef PROJECT_ROOT
    constexpr std::size_t root_len = sizeof(PROJECT_ROOT) - 1;
    if (root_len > 0 && std::strncmp(file, PROJECT_ROOT, root_len) == 0) {
        file += root_len;
        if (*file == '/' || *file == '\\') {
            ++file;
        }
    }
#endif
    return file;
}
} // namespace logging::detail


// VA_ARGS -> vardic arguments
#define LOG_AT_LEVEL(level, fmt, ...) \
    logging::logger().log( \
        spdlog::source_loc{logging::detail::trimPath(__FILE__), __LINE__, SPDLOG_FUNCTION}, \
        level, fmt, ##__VA_ARGS__)

#define LOG_TRACE(fmt, ...) LOG_AT_LEVEL(spdlog::level::trace, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) LOG_AT_LEVEL(spdlog::level::debug, fmt, ##__VA_ARGS__)

#if defined(ENABLE_INFO_LOGS)
#define LOG_INFO(fmt, ...) LOG_AT_LEVEL(spdlog::level::info, fmt, ##__VA_ARGS__)
#else
#define LOG_INFO(...) ((void)0)
#endif

#define LOG_TIME(fmt, ...) LOG_AT_LEVEL(spdlog::level::info, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  LOG_AT_LEVEL(spdlog::level::warn,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG_AT_LEVEL(spdlog::level::err,   fmt, ##__VA_ARGS__)
