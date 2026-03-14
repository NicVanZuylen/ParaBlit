#pragma once

#if defined(_MSC_VER)

#if CLIB_EXPORT
#define CLIB_API __declspec(dllexport)
#else
#define CLIB_API __declspec(dllimport)
#endif

#endif /* defined(_MSC_VER) */

#if defined(__GNUC__)

#if CLIB_EXPORT
#define CLIB_API __attribute__((visibility("default")))
#else
#define CLIB_API 
#endif

#endif /* defined(__GNUC__) */