#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) 
#define SF_PLATFORM_WINDOWS 1
#ifndef _WIN64
#error "64-bit is required on Windows!"
#endif
#elif defined(__linux__) || defined(__gnu_linux__)
// Linux OS
#define SF_PLATFORM_LINUX 1
#if defined(__ANDROID__)
#define SF_PLATFORM_ANDROID 1
#endif
#elif defined(__unix__)
// Catch anything not caught by the above.
#define SF_PLATFORM_UNIX 1
#elif defined(_POSIX_VERSION)
// Posix
#define SF_PLATFORM_POSIX 1
#elif __APPLE__
// Apple platforms
#define SF_PLATFORM_APPLE 1
#include <TargetConditionals.h>
#if TARGET_IPHONE_SIMULATOR
// iOS Simulator
#define SF_PLATFORM_IOS 1
#define SF_PLATFORM_IOS_SIMULATOR 1
#elif TARGET_OS_IPHONE
#define SF_PLATFORM_IOS 1
// iOS device
#elif TARGET_OS_MAC
// Other kinds of Mac OS
#else
#error "Unknown Apple platform"
#endif
#else
#error "Unknown platform!"
#endif

// Exports
#ifdef _MSC_VER
#define SF_EXPORT __declspec(dllexport)
#else
#define SF_EXPORT __attribute__((visibility("default")))
#endif
// Imports
#ifdef _MSC_VER
#define SF_IMPORT __declspec(dllimport)
#else
#define SF_IMPORT
#endif
