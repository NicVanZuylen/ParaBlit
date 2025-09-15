#pragma once

#if MATH_EXPORT
#define MATH_API __declspec(dllexport)
#else
#define MATH_API __declspec(dllimport)
#endif

#define NOMINMAX // Defining this prevents "min" and "max" macros from being defined in Windows headers, which may conflict with glm::min and glm::max functions.

#define MATH_INTERFACE virtual
#define MATH_INLINE __forceinline

#pragma warning(push, 0)
#define GLM_FORCE_CTOR_INIT
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/quaternion.hpp"
#pragma warning(pop)