#include "logger.hpp"

int main() {
    sf_core::log_output(sf_core::Log_level::LOG_LEVEL_ERROR, "Hello, {}", "Sam!");
    sf_core::log_output(sf_core::Log_level::LOG_LEVEL_FATAL, "Hello, {}", "Sam!");
    sf_core::log_output(sf_core::Log_level::LOG_LEVEL_INFO, "Hello, {}", "Sam!");
    sf_core::log_output(sf_core::Log_level::LOG_LEVEL_WARN, "Hello, {}", "Sam!");
    sf_core::log_output(sf_core::Log_level::LOG_LEVEL_DEBUG, "Hello, {}", "Sam!");
    sf_core::log_output(sf_core::Log_level::LOG_LEVEL_TRACE, "Hello, {}", "Sam!");
    return 0;
}
