#pragma once

#include <format>
#include <array>
#include <iostream>
#include <string_view>
#include "platform/sf_platform_macros.hpp"
#include "core/sf_types.hpp"

#define LOG_WARN_ENABLED 1
#define LOG_INFO_ENABLED 1
#define LOG_DEBUG_ENABLED 1
#define LOG_TRACE_ENABLED 1

#if SF_RELEASE == 1
#define LOG_DEBUG_ENABLED 0
#define LOG_TRACE_ENABLED 0
#endif

namespace sf_platform {
    void platform_console_write(std::string_view message, u8 color);
    void platform_console_write_error(std::string_view message, u8 color);
}

namespace sf_core {
    enum class LogLevel : u8 {
        LOG_LEVEL_FATAL,
        LOG_LEVEL_ERROR,
        LOG_LEVEL_WARN,
        LOG_LEVEL_INFO,
        LOG_LEVEL_DEBUG,
        LOG_LEVEL_TRACE,
    };

    constexpr std::array<const i8*, 6> log_level_as_str = {
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
        char message_buff[BUFF_LEN] = {0};
        const std::format_to_n_result res = std::format_to_n(message_buff, BUFF_LEN, fmt, std::forward<Args>(args)...);

        constexpr usize BUFF_LEN2{ 32000 };
        char message_buff2[BUFF_LEN2] = {0};
        const std::format_to_n_result res2 = std::format_to_n(message_buff2, BUFF_LEN2, "{}{}\n", log_level_as_str[static_cast<usize>(log_level)], message_buff);

        std::cerr << std::string_view(const_cast<const i8*>(message_buff2), res2.out);

        // switch (log_level) {
        //     case LogLevel::LOG_LEVEL_FATAL:
        //     case LogLevel::LOG_LEVEL_ERROR:
                // sf_platform::platform_console_write_error(std::string_view(const_cast<const i8*>(message_buff2), res2.out), static_cast<u8>(log_level));
            //     break;
            // default:
                // sf_platform::platform_console_write(std::string_view(const_cast<const i8*>(message_buff2), res2.out), static_cast<u8>(log_level));
                // break;
        // }
    }
}

#define LOG_FATAL(fmt, ...) log_output(sf_core::LogLevel::LOG_LEVEL_FATAL, fmt, ##__VA_ARGS__);

#define LOG_ERROR(fmt, ...) log_output(sf_core::LogLevel::LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__);

#if LOG_WARN_ENABLED == 1
#define LOG_WARN(fmt, ...) log_output(sf_core::LogLevel::LOG_LEVEL_WARN, fmt, ##__VA_ARGS__);
#endif

#if LOG_INFO_ENABLED == 1
#define LOG_INFO(fmt, ...) log_output(sf_core::LogLevel::LOG_LEVEL_INFO, fmt, ##__VA_ARGS__);
#endif

#if LOG_DEBUG_ENABLED == 1
#define LOG_DEBUG(fmt, ...) log_output(sf_core::LogLevel::LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__);
#endif

#if LOG_TRACE_ENABLED == 1
#define LOG_TRACE(fmt, ...) log_output(sf_core::LogLevel::LOG_LEVEL_TRACE, fmt, ##__VA_ARGS__);
#endif
