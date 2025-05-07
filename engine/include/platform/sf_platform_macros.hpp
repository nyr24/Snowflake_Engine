#pragma once

// Win32
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#define SF_PLATFORM_WINDOWS 1
#ifndef _WIN64
#error "64-bit is required on Windows!"
#endif

// Linux
#elif defined(__linux__) || defined(__gnu_linux__)
#define SF_PLATFORM_LINUX 1
#if defined(SF_BUILD_WAYLAND) && defined(SF_BUILD_X11)
#error "Can't build for wayland and x11 simultaneously"
#elif ! defined(SF_BUILD_WAYLAND) && ! defined(SF_BUILD_X11)
#error "You need to choose for building either for wayland or x11 if you are on linux"
#elif defined(SF_BUILD_WAYLAND)
#define SF_PLATFORM_WAYLAND
#elif defined(SF_BUILD_X11)
#define SF_PLATFORM_X11
#endif

#if defined(__ANDROID__)
#define SF_PLATFORM_ANDROID 1
#endif
#elif defined(__unix__)
// Catch anything not caught by the above.
#define SF_PLATFORM_UNIX 1
// Posix
#elif defined(_POSIX_VERSION)
#define SF_PLATFORM_POSIX 1

// Apple platforms
#elif __APPLE__
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
