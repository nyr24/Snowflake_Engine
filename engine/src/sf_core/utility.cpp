#include "sf_core/utility.hpp"
#include "sf_core/logger.hpp"

namespace sf {

[[noreturn]] void panic(const char* message) {
    LOG_FATAL("{}", message);
    std::exit(1);
}

} // sf
