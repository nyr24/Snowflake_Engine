#pragma once

#include "sf_core/defines.hpp"

#ifdef SF_ASSERTS_ENABLED
#if _MSC_VER
#include <intrin.h>
#define DEBUG_BREAK() __debugbreak()
#else
#define DEBUG_BREAK() __builtin_trap()
#endif

namespace sf {
SF_EXPORT void report_assertion_failure(const char* expression, const char* message, const char* file, i32 line);
}

#define SF_ASSERT(expr)                                                         \
    {                                                                           \
        if (expr) {                                                             \
        } else {                                                                \
            sf::report_assertion_failure(#expr, "", __FILE__, __LINE__);   \
            DEBUG_BREAK();                                                      \
        }                                                                       \
    }                                                                           \

#define SF_ASSERT_MSG(expr, msg)                                                \
    {                                                                           \
        if (expr) {                                                             \
        } else {                                                                \
            sf::report_assertion_failure(#expr, #msg, __FILE__, __LINE__); \
            DEBUG_BREAK();                                                      \
        }                                                                       \
    }                                                                           \

#ifdef SF_DEBUG
#define SF_ASSERT_DEBUG(expr)                                                   \
    {                                                                           \
        if (expr) {                                                             \
        } else {                                                                \
            sf::report_assertion_failure(#expr, "", __FILE__, __LINE__);   \
            DEBUG_BREAK();                                                      \
        }                                                                       \
    }                                                                           \

#else
#define SF_ASSERT_DEBUG(expr)
#endif

#else
#define SF_ASSERT(expr)
#define SF_ASSERT_MSG(expr, msg)
#define SF_ASSERT_DEBUG(expr)
#endif // SF_ASSERTS_ENABLED
