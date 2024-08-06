#pragma once
#include "Engine.Math/MathLib.h"

#pragma warning (push)
#pragma warning (disable : 26495) // 'data' member of Vector is intentionally uninitialized.

namespace Eng::Math
{
	template <typename T>
	struct TVector2
	{
		using ScalarT = T;
		using Self = TVector2<T>;
		using GLMT = glm::tvec2<T>;
		
		union
		{
			struct
			{
				T x;
				T y;
			};
			struct
			{
				T r;
				T g;
			};
			T data[2];
		};

		// -----------------------------------------------------------------
		// Constructors

		TVector2()
			: x(0.0f)
			, y(0.0f)
		{
		}

		TVector2(const GLMT& vec)
			: x(vec.x)
			, y(vec.y)
		{
		}

		TVector2(T all)
			: x(all)
			, y(all)
		{
		}

		TVector2(T x_, T y_)
			: x(x_)
			, y(y_)
		{
		}

		// -----------------------------------------------------------------
		// Conversion

		MATH_INLINE explicit operator GLMT& ()
		{
			return *reinterpret_cast<GLMT*>(this);
		}

		MATH_INLINE explicit operator const GLMT& () const
		{
			return *reinterpret_cast<const GLMT*>(this);
		}

		MATH_INLINE explicit operator T* ()
		{
			return data;
		}

		MATH_INLINE explicit operator const T* () const
		{
			return data;
		}

		// -----------------------------------------------------------------

		// -----------------------------------------------------------------
		// Copy Assignment

		MATH_INLINE Self& operator = (const T* data_)
		{
			x = data_[0];
			y = data_[1];
			return *this;
		}

		MATH_INLINE Self& operator = (const Self& other)
		{
			x = other.x;
			y = other.y;
			return *this;
		}

		// -----------------------------------------------------------------

		// -----------------------------------------------------------------
		// Comparison

		MATH_INLINE bool operator == (const T* data_) const
		{
			return x == data_[0] && y == data_[1];
		}

		MATH_INLINE bool operator == (const Self& other) const
		{
			return x == other.x && y == other.y;
		}

		// -----------------------------------------------------------------

		// -----------------------------------------------------------------
		// Subscript

		MATH_INLINE T& operator [] (const size_t& i)
		{
			return data[i];
		}

		MATH_INLINE T operator [] (const size_t& i) const
		{
			return data[i];
		}

		// -----------------------------------------------------------------

		// -----------------------------------------------------------------
		// Arithmetic

		MATH_INLINE Self operator + (const T& val) const
		{
			return { x + val, y + val };
		}

		MATH_INLINE Self& operator += (const T& val)
		{
			x += val;
			y += val;
			return *this;
		}

		MATH_INLINE Self operator - (const T& val) const
		{
			return { x - val, y - val };
		}

		MATH_INLINE Self& operator -= (const T& val)
		{
			x -= val;
			y -= val;
			return *this;
		}

		MATH_INLINE Self operator * (const T& val) const
		{
			return { x * val, y * val };
		}

		MATH_INLINE Self& operator *= (const T& val)
		{
			x *= val;
			y *= val;
			return *this;
		}

		MATH_INLINE Self operator / (const T& val) const
		{
			return { x / val, y / val };
		}

		MATH_INLINE Self& operator /= (const T& val)
		{
			x /= val;
			y /= val;
			return *this;
		}

		MATH_INLINE Self operator % (const T& val) const
		{
			return { x % val, y % val };
		}

		MATH_INLINE Self& operator %= (const T& val)
		{
			x %= val;
			y %= val;
			return *this;
		}

		MATH_INLINE Self operator + (const Self& other) const
		{
			return { x + other.x, y + other.y };
		}

		MATH_INLINE Self& operator += (const Self& other)
		{
			x += other.x;
			y += other.y;
			return *this;
		}

		MATH_INLINE Self operator - (const Self& other) const
		{
			return { x - other.x, y - other.y };
		}

		MATH_INLINE Self& operator -= (const Self& other)
		{
			x -= other.x;
			y -= other.y;
			return *this;
		}

		MATH_INLINE Self operator * (const Self& other) const
		{
			return { x * other.x, y * other.y };
		}

		MATH_INLINE Self& operator *= (const Self& other)
		{
			x *= other.x;
			y *= other.y;
			return *this;
		}

		MATH_INLINE Self operator / (const Self& other) const
		{
			return { x / other.x, y / other.y };
		}

		MATH_INLINE Self& operator /= (const Self& other)
		{
			x /= other.x;
			y /= other.y;
			return *this;
		}

		MATH_INLINE Self operator % (const Self& other) const
		{
			return { x % other.x, y % other.y };
		}

		MATH_INLINE Self& operator %= (const Self& other)
		{
			x %= other.x;
			y %= other.y;
			return *this;
		}

		MATH_INLINE Self operator - () const
		{
			return { -x, -y };
		}

		// -----------------------------------------------------------------

		// -----------------------------------------------------------------
		// Math functions

		MATH_INLINE T Dot(const Self& other) const
		{
			const GLMT& selfRef = *((GLMT*)this);
			return glm::dot(selfRef, (const GLMT&)other);
		}

		MATH_INLINE Self Cross(const Self& other) const
		{
			using GLMT3 = glm::tvec3<T>;
			const GLMT3& selfRef = *((GLMT3*)this);
			return Self(glm::cross(selfRef, (const GLMT3&)other));
		}

		MATH_INLINE T Length() const
		{
			const GLMT& selfRef = *((GLMT*)this);
			return glm::length(selfRef);
		}

		MATH_INLINE T LengthSqr() const
		{
			const Self& selfRef = *((GLMT*)this);
			Self selfSqr = selfRef * selfRef;
			return selfSqr.x + selfSqr.y + selfSqr.z;
		}

		MATH_INLINE Self& Normalize()
		{
			GLMT& selfRef = *((GLMT*)this);
			selfRef = glm::normalize(selfRef);
			return *this;
		}

		MATH_INLINE Self Normalized() const
		{
			GLMT& selfRef = *((GLMT*)this);
			return glm::normalize(selfRef);
		}

		MATH_INLINE Self Abs() const
		{
			return { glm::abs(x), glm::abs(y) };
		}

		MATH_INLINE Self Min(const Self& other) const
		{
			return { glm::min(x, other.x), glm::min(y, other.y) };
		}

		MATH_INLINE Self Max(const Self& other) const
		{
			return { glm::max(x, other.x), glm::max(y, other.y) };
		}

		// -----------------------------------------------------------------
	};
	static_assert(sizeof(TVector2<float>) == sizeof(float) * 2);

	template<typename T>
	TVector2<T> operator + (T scalar, const TVector2<T>& vec)
	{
		return vec + scalar;
	}

	template<typename T>
	TVector2<T> operator - (T scalar, const TVector2<T>& vec)
	{
		return { scalar - vec.x, scalar - vec.y };
	}

	template<typename T>
	TVector2<T> operator * (T scalar, const TVector2<T>& vec)
	{
		return vec * scalar;
	}

	template<typename T>
	TVector2<T> operator / (T scalar, const TVector2<T>& vec)
	{
		return { scalar / vec.x, scalar / vec.y };
	}

	template<typename T>
	MATH_INLINE T Dot(const TVector2<T>& a, const TVector2<T>& b) { return a.Dot(b); }
	template<typename T>
	MATH_INLINE T Length(const TVector2<T>& vec) { return vec.Length(); }
	template<typename T>
	MATH_INLINE T LengthSqr(const TVector2<T>& vec) { return vec.LengthSqr(); }

	using Vector2f = TVector2<float>;
	using Vector2d = TVector2<double>;
}

#pragma warning (pop)