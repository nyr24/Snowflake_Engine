#pragma once

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
