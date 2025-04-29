#include "logger.hpp"
#include "asserts.hpp"

SF_EXPORT void sf_core::report_assertion_failure(const char* expression, const char* message, const char* file, i32 line) {
    sf_core::log_output(LogLevel::LOG_LEVEL_FATAL, "Assertion failure: {},\n\tmessage: {},\n\tin file: {},\n\tline: {}\n", expression, message, file, line);
}

bool sf_core::init_logging() {
    // TODO:
    return true;
}

void sf_core::shutdown_logging() {
    // TODO:
}
