#include "sf_core/logger.hpp"
#include "sf_core/asserts_sf.hpp"

#ifdef SF_ASSERTS_ENABLED
SF_EXPORT void sf::report_assertion_failure(const char* expression, const char* message, const char* file, i32 line) {
    sf::log_output(LogLevel::LOG_LEVEL_FATAL, "Assertion failure: {},\n\tmessage: {},\n\tin file: {},\n\tline: {}\n", expression, message, file, line);
}
#endif

bool sf::init_logging() {
    // TODO:
    return true;
}

void sf::shutdown_logging() {
    // TODO:
}
