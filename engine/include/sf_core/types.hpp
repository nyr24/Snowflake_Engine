#pragma once

// Unsigned int types.
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned long long usize;

// Signed int types.
typedef char i8;
typedef short i16;
typedef int i32;
typedef long long i64;
typedef long long isize;

// Floating point types
typedef float f32;
typedef double f64;

// Pointer types
#if defined(__x86_64__) || defined(_M_X64)
typedef unsigned long long usize;
typedef long long isize;
#else
typedef unsigned long usize;
typedef long isize;
#endif

// Ensure all types are of the correct size.
static_assert(sizeof(u8) == 1, "Expected u8 to be 1 byte.");
static_assert(sizeof(u16) == 2, "Expected u16 to be 2 bytes.");
static_assert(sizeof(u32) == 4, "Expected u32 to be 4 bytes.");
static_assert(sizeof(u64) == 8, "Expected u64 to be 8 bytes.");

static_assert(sizeof(i8) == 1, "Expected i8 to be 1 byte.");
static_assert(sizeof(i16) == 2, "Expected i16 to be 2 bytes.");
static_assert(sizeof(i32) == 4, "Expected i32 to be 4 bytes.");
static_assert(sizeof(i64) == 8, "Expected i64 to be 8 bytes.");

static_assert(sizeof(f32) == 4, "Expected f32 to be 4 bytes.");
static_assert(sizeof(f64) == 8, "Expected f64 to be 8 bytes.");

#if defined(__x86_64__) || defined(_M_X64)
static_assert(sizeof(usize) == 8, "Expected usize to be 8 bytes on x86_64 arch");
static_assert(sizeof(isize) == 8, "Expected isize to be 8 bytes on x86_64 arch");
#else
static_assert(sizeof(usize) == 4, "Expected usize to be 4 bytes on x86 arch");
static_assert(sizeof(isize) == 4, "Expected isize to be 4 bytes on x86 arch");
#endif
