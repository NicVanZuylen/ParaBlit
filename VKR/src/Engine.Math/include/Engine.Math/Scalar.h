#pragma once
#include "Engine.Math/MathLib.h"

namespace Eng::Math
{
	MATH_INLINE float ToDegrees(const float& radians)
	{
		return glm::degrees(radians);
	}

	MATH_INLINE double ToDegrees(const double& radians)
	{
		return glm::degrees(radians);
	}

	MATH_INLINE float ToRadians(const float& degrees)
	{
		return glm::radians(degrees);
	}

	MATH_INLINE double ToRadians(const double& degrees)
	{
		return glm::radians(degrees);
	}

	MATH_INLINE float Abs(const float& value)
	{
		return glm::abs(value);
	}

	MATH_INLINE double Abs(const double& value)
	{
		return glm::abs(value);
	}

	MATH_INLINE int32_t Abs(const int32_t& value)
	{
		return glm::abs(value);
	}

	MATH_INLINE int64_t Abs(const int64_t& value)
	{
		return glm::abs(value);
	}

	MATH_INLINE int32_t Min(const int32_t& a, const int32_t& b)
	{
		return glm::min(a, b);
	}

	MATH_INLINE int64_t Min(const int64_t& a, const int64_t& b)
	{
		return glm::min(a, b);
	}

	MATH_INLINE uint32_t Min(const uint32_t& a, const uint32_t& b)
	{
		return glm::min(a, b);
	}

	MATH_INLINE uint64_t Min(const uint64_t& a, const uint64_t& b)
	{
		return glm::min(a, b);
	}

	MATH_INLINE float Min(const float& a, const float& b)
	{
		return glm::min(a, b);
	}

	MATH_INLINE double Min(const double& a, const double& b)
	{
		return glm::min(a, b);
	}

	MATH_INLINE float Max(const float& a, const float& b)
	{
		return glm::max(a, b);
	}

	MATH_INLINE double Max(const double& a, const double& b)
	{
		return glm::max(a, b);
	}

	MATH_INLINE int32_t Max(const int32_t& a, const int32_t& b)
	{
		return glm::max(a, b);
	}

	MATH_INLINE int64_t Max(const int64_t& a, const int64_t& b)
	{
		return glm::max(a, b);
	}

	MATH_INLINE uint32_t Max(const uint32_t& a, const uint32_t& b)
	{
		return glm::max(a, b);
	}

	MATH_INLINE uint64_t Max(const uint64_t& a, const uint64_t& b)
	{
		return glm::max(a, b);
	}

	MATH_INLINE float Lerp(const float& x, const float& y, const float& a) { return glm::mix(x, y, a); }
	MATH_INLINE double Lerp(const double& x, const double& y, const double& a) { return glm::mix(x, y, a); }

	MATH_INLINE float Mix(const float& x, const float& y, const float& a) { return Mix(x, y, a); }
	MATH_INLINE double Mix(const double& x, const double& y, const double& a) { return Mix(x, y, a); }

	MATH_INLINE float Pow(const float& x, const float& p) { return glm::pow(x, p); }
	MATH_INLINE double Pow(const double& x, const double& p) { return glm::pow(x, p); }

	MATH_INLINE float Exp(const float& x) { return glm::exp(x); }
	MATH_INLINE double Exp(const double& x) { return glm::exp(x); }

	MATH_INLINE float Sqrt(const float& x) { return glm::sqrt(x); }
	MATH_INLINE double Sqrt(const double& x) { return glm::sqrt(x); }

	template<typename T>
	MATH_INLINE T Pi()
	{
		return glm::pi<T>();
	}

	template<typename T>
	MATH_INLINE T Clamp(const T& val, const T& min, const T& max)
	{
		return glm::clamp(val, min, max);
	}

	template<typename T>
	MATH_INLINE T Sin(const T& x)
	{
		return glm::sin(x);
	}

	template<typename T>
	MATH_INLINE T Cos(const T& x)
	{
		return glm::cos(x);
	}

	template<typename T>
	MATH_INLINE T Tan(const T& x)
	{
		return glm::tan(x);
	}

	template<typename T>
	MATH_INLINE T RoundUp(const T& value, const T& target)
	{
		T mod = value % target;
		return value + (mod > 0 ? target - mod : 0);
	}

	template<typename T>
	MATH_INLINE T Floor(const T& value)
	{
		return glm::floor(value);
	}

	template<typename T>
	MATH_INLINE T Ceil(const T& value)
	{
		return glm::ceil(value);
	}

	template<typename T>
	MATH_INLINE uint32_t FloorToInt(const T& value)
	{
		return uint32_t(glm::floor(value));
	}

	template<typename T>
	MATH_INLINE uint32_t CeilToInt(const T& value)
	{
		return uint32_t(glm::ceil(value));
	}
}