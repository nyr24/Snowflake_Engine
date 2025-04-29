#pragma once

#include <format>
#include <iostream>
#include <array>
#include <string_view>
#include "sf_platform.hpp"
#include "sf_types.hpp"

#define LOG_WARN_ENABLED 1
#define LOG_INFO_ENABLED 1
#define LOG_DEBUG_ENABLED 1
#define LOG_TRACE_ENABLED 1

#if SF_RELEASE == 1
#define LOG_DEBUG_ENABLED 0
#define LOG_TRACE_ENABLED 0
#endif

namespace sf_core {
    enum class LogLevel : u8 {
        LOG_LEVEL_FATAL,
        LOG_LEVEL_ERROR,
        LOG_LEVEL_WARN,
        LOG_LEVEL_INFO,
        LOG_LEVEL_DEBUG,
        LOG_LEVEL_TRACE,
    };

    constexpr std::array<const char*, 6> log_level_as_str = {
        "[FATAL]: ",
        "[ERROR]: ",
        "[WARN]: ",
        "[INFO]: ",
        "[DEBUG]: ",
        "[TRACE]: ",
    };

    bool init_logging();
    void shutdown_logging();

    template<typename... Args>
    SF_EXPORT void log_output(LogLevel log_level, std::format_string<Args...> fmt, Args&&... args) {
        constexpr usize BUFF_LEN{ 32000 };
        i8 message_buff[BUFF_LEN] = {0};
        const std::format_to_n_result res = std::format_to_n(message_buff, BUFF_LEN, fmt, args...);

        switch (log_level) {
            case LogLevel::LOG_LEVEL_FATAL:
            case LogLevel::LOG_LEVEL_ERROR:
                std::cerr << log_level_as_str[static_cast<usize>(log_level)] << std::string_view(const_cast<const i8*>(message_buff), res.out) << '\n';
                break;
            default:
                std::cout << log_level_as_str[static_cast<usize>(log_level)] << std::format(fmt, args...) << '\n';
                break;
        }
    }
}

#define LOG_FATAL(fmt, ...) log_output(LogLevel::LOG_LEVEL_FATAL, ##__VA_ARGS__);

#define LOG_ERROR(fmt, ...) log_output(LogLevel::LOG_LEVEL_ERROR, ##__VA_ARGS__);

#if LOG_WARN_ENABLED == 1
#define LOG_WARN(fmt, ...) log_output(LogLevel::LOG_LEVEL_WARN, ##__VA_ARGS__);
#endif

#if LOG_INFO_ENABLED == 1
#define LOG_INFO(fmt, ...) log_output(LogLevel::LOG_LEVEL_INFO, ##__VA_ARGS__);
#endif

#if LOG_DEBUG_ENABLED == 1
#define LOG_DEBUG(fmt, ...) log_output(LogLevel::LOG_LEVEL_DEBUG, ##__VA_ARGS__);
#endif

#if LOG_TRACE_ENABLED == 1
#define LOG_TRACE(fmt, ...) log_output(LogLevel::LOG_LEVEL_TRACE, ##__VA_ARGS__);
#endif
