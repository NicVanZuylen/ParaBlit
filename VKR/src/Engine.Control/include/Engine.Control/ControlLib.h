#pragma once

#include <cstdint>

#if defined(_MSC_VER)

#if CONTROL_EXPORT
#define CONTROL_API __declspec(dllexport)
#else
#define CONTROL_API __declspec(dllimport)
#endif

#elif defined(__GNUC__)

#if CONTROL_EXPORT
#define CONTROL_API __attribute__((visibility("default")))
#else
#define CONTROL_API
#endif

#endif

#define CONTROL_INTERFACE virtual