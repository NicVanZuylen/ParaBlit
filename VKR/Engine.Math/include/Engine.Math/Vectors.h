#pragma once
#include "Engine.Math/MathLib.h"
#include "Vector4.h" // Includes other vector types.

namespace Eng::Math
{
	template<typename TVec>
	MATH_INLINE TVec::ScalarT Distance(const TVec& a, const TVec& b) { return (b - a).Length(); }
	template<typename TVec>
	MATH_INLINE TVec Cross(const TVec& a, const TVec& b) { return a.Cross(b); }
	template<typename TVec>
	MATH_INLINE TVec Normalize(const TVec& vec) { return vec.Normalized(); }
	template<typename TVec>
	MATH_INLINE TVec Abs(const TVec& vec) { return vec.Abs(); }
	template<typename TVec>
	MATH_INLINE TVec Min(const TVec& a, const TVec& b) { return a.Min(b); }
	template<typename TVec>
	MATH_INLINE TVec Max(const TVec& a, const TVec& b) { return a.Max(b); }
}