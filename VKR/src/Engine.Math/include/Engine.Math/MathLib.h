#pragma once

#if defined(_MSC_VER)

#if MATH_EXPORT
#define MATH_API __declspec(dllexport)
#else
#define MATH_API __declspec(dllimport)
#endif

#define MATH_INLINE __forceinline

#elif defined(__GNUC__)

#ifdef MATH_EXPORT
#define MATH_API __attribute__((visibility("default")))
#else
#define MATH_API
#endif

#define MATH_INLINE __attribute__((always_inline)) inline

#endif

#define NOMINMAX // Defining this prevents "min" and "max" macros from being defined in Windows headers, which may conflict with glm::min and glm::max functions.

#define MATH_INTERFACE virtual

#pragma warning(push, 0)
#define GLM_FORCE_CTOR_INIT
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/quaternion.hpp"
#pragma warning(pop)