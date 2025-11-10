#pragma once

#include <format>
#include "sf_containers/fixed_array.hpp"
#include "sf_core/defines.hpp"

#define LOG_WARN_ENABLED 1

#if SF_RELEASE == 1
#define LOG_INFO_ENABLED 0
#define LOG_DEBUG_ENABLED 0
#define LOG_TRACE_ENABLED 0
#define LOG_ERROR_ENABLED 0
#else
#define LOG_INFO_ENABLED 1
#define LOG_DEBUG_ENABLED 1
#define LOG_TRACE_ENABLED 1
#define LOG_ERROR_ENABLED 1
#endif

namespace sf {
void platform_console_write(char* message_buff, u16 written_count, u8 color);
void platform_console_write_error(char* message_buff, u16 written_count, u8 color);

enum struct LogLevel : u8 {
    LOG_LEVEL_FATAL,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_TRACE,
    LOG_LEVEL_TEST,
    COUNT
};

const FixedArray<const char*, static_cast<u8>(LogLevel::COUNT)> log_level_as_str = {
    "[FATAL]: ",
    "[ERROR]: ",
    "[WARN]: ",
    "[INFO]: ",
    "[DEBUG]: ",
    "[TRACE]: ",
    "[TEST]: ",
};

static constexpr u16 OUTPUT_PRINT_BUFFER_CAPACITY{ 2056 };

template<typename... Args>
SF_EXPORT void log_output(LogLevel log_level, std::format_string<Args...> fmt, Args&&... args) {
    char message_buff[OUTPUT_PRINT_BUFFER_CAPACITY] = {0};
    auto write_res1 = std::format_to_n(message_buff, OUTPUT_PRINT_BUFFER_CAPACITY, "{}", log_level_as_str[static_cast<usize>(log_level)]);
    auto write_res2 = std::format_to_n(message_buff + write_res1.size, OUTPUT_PRINT_BUFFER_CAPACITY - write_res1.size, fmt, std::forward<Args>(args)...);

    switch (log_level) {
        case LogLevel::LOG_LEVEL_FATAL:
        case LogLevel::LOG_LEVEL_ERROR:
            sf::platform_console_write_error(message_buff, static_cast<u16>(write_res2.size), static_cast<u8>(log_level));
            break;
        default:
            sf::platform_console_write(message_buff, static_cast<u16>(write_res2.size), static_cast<u8>(log_level));
            break;
    }
}

} // sf

#define LOG_FATAL(fmt, ...) log_output(sf::LogLevel::LOG_LEVEL_FATAL, fmt, ##__VA_ARGS__);
#define LOG_TEST(fmt, ...) log_output(sf::LogLevel::LOG_LEVEL_TEST, fmt, ##__VA_ARGS__);

#if LOG_ERROR_ENABLED == 1
#define LOG_ERROR(fmt, ...) log_output(sf::LogLevel::LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__);
#else
#define LOG_ERROR(fmt, ...)
#endif

#if LOG_WARN_ENABLED == 1
#define LOG_WARN(fmt, ...) log_output(sf::LogLevel::LOG_LEVEL_WARN, fmt, ##__VA_ARGS__);
#else
#define LOG_WARN(fmt, ...)
#endif

#if LOG_INFO_ENABLED == 1
#define LOG_INFO(fmt, ...) log_output(sf::LogLevel::LOG_LEVEL_INFO, fmt, ##__VA_ARGS__);
#else
#define LOG_INFO(fmt, ...)
#endif

#if LOG_DEBUG_ENABLED == 1
#define LOG_DEBUG(fmt, ...) log_output(sf::LogLevel::LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__);
#else
#define LOG_DEBUG(fmt, ...)
#endif

#if LOG_TRACE_ENABLED == 1
#define LOG_TRACE(fmt, ...) log_output(sf::LogLevel::LOG_LEVEL_TRACE, fmt, ##__VA_ARGS__);
#else
#define LOG_TRACE(ftm, ...)
#endif
