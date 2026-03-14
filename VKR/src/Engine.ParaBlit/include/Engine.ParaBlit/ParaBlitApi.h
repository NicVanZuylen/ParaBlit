#pragma once

#include "ParaBlitDefs.h"

#if defined(_MSC_VER)

#ifdef PARABLIT_EXPORT
#define PARABLIT_API __declspec(dllexport)
#else
#define PARABLIT_API __declspec(dllimport)
#endif

#endif /* defined(_MSC_VER) */
#if defined(__GNUC__)

#ifdef PARABLIT_EXPORT
#define PARABLIT_API __attribute__((visibility("default")))
#else
#define PARABLIT_API
#endif

#endif /* defined(__GNUC__) */