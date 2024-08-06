#pragma once
#include "Engine.Math/MathLib.h"

namespace Eng::Math
{
	template<typename T>
	MATH_INLINE T ToDegrees(T radians)
	{
		return glm::degrees(radians);
	}

	template<typename T>
	MATH_INLINE T ToRadians(T degrees)
	{
		return glm::radians(degrees);
	}

	MATH_INLINE float Abs(float value)
	{
		return glm::abs(value);
	}

	MATH_INLINE double Abs(double value)
	{
		return glm::abs(value);
	}

	MATH_INLINE int32_t Abs(int32_t value)
	{
		return glm::abs(value);
	}

	MATH_INLINE int64_t Abs(int64_t value)
	{
		return glm::abs(value);
	}

	MATH_INLINE float Min(float a, float b)
	{
		return glm::min(a, b);
	}

	MATH_INLINE double Min(double a, double b)
	{
		return glm::min(a, b);
	}

	MATH_INLINE float Max(float a, float b)
	{
		return glm::max(a, b);
	}

	MATH_INLINE double Max(double a, double b)
	{
		return glm::max(a, b);
	}
}