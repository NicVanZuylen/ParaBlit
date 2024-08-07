#pragma once
#include "Engine.Math/MathLib.h"
#include "Vector2.h"

#pragma warning (push)
#pragma warning (disable : 26495) // 'data' member of Vector is intentionally uninitialized.

namespace Eng::Math
{
	template <typename T>
	struct TVector3
	{
		using ScalarT = T;
		using Self = TVector3<T>;
		using GLMT = glm::tvec3<T>;
		
		union
		{
			struct
			{
				T x;
				T y;
				T z;
			};
			struct
			{
				T r;
				T g;
				T b;
			};
			T data[3];
		};

		// -----------------------------------------------------------------
		// Constructors

		TVector3()
			: x(0.0f)
			, y(0.0f)
			, z(0.0f)
		{
		}

		TVector3(const GLMT& vec)
			: x(vec.x)
			, y(vec.y)
			, z(vec.z)
		{
		}

		TVector3(T all)
			: x(all)
			, y(all)
			, z(all)
		{
		}

		TVector3(T x_, T y_, T z_)
			: x(x_)
			, y(y_)
			, z(z_)
		{
		}

		TVector3(const TVector2<T>& xy, T z_)
			: x(xy.x)
			, y(xy.y)
			, z(z_)
		{
		}

		TVector3(T x_, const TVector2<T>& yz)
			: x(x_)
			, y(yz.x)
			, z(yz.y)
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

		MATH_INLINE operator const TVector2<T>& () const
		{
			return *((const TVector2<T>*)this);
		}

		// -----------------------------------------------------------------

		// -----------------------------------------------------------------
		// Copy Assignment

		MATH_INLINE Self& operator = (const T* data_)
		{
			x = data_[0];
			y = data_[1];
			z = data_[2];
			return *this;
		}

		MATH_INLINE Self& operator = (const Self& other)
		{
			x = other.x;
			y = other.y;
			z = other.z;
			return *this;
		}

		// -----------------------------------------------------------------

		// -----------------------------------------------------------------
		// Comparison

		MATH_INLINE bool operator == (const T* data_) const
		{
			return x == data_[0] && y == data_[1] && z == data_[2];
		}

		MATH_INLINE bool operator == (const Self& other) const
		{
			return x == other.x && y == other.y && z == other.z;
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
			return { x + val, y + val, z + val };
		}

		MATH_INLINE Self& operator += (const T& val)
		{
			x += val;
			y += val;
			z += val;
			return *this;
		}

		MATH_INLINE Self operator - (const T& val) const
		{
			return { x - val, y - val, z - val };
		}

		MATH_INLINE Self& operator -= (const T& val)
		{
			x -= val;
			y -= val;
			z -= val;
			return *this;
		}

		MATH_INLINE Self operator * (const T& val) const
		{
			return { x * val, y * val, z * val };
		}

		MATH_INLINE Self& operator *= (const T& val)
		{
			x *= val;
			y *= val;
			z *= val;
			return *this;
		}

		MATH_INLINE Self operator / (const T& val) const
		{
			return { x / val, y / val, z / val };
		}

		MATH_INLINE Self& operator /= (const T& val)
		{
			x /= val;
			y /= val;
			z /= val;
			return *this;
		}

		MATH_INLINE Self operator % (const T& val) const
		{
			return { x % val, y % val, z % val };
		}

		MATH_INLINE Self& operator %= (const T& val)
		{
			x %= val;
			y %= val;
			z %= val;
			return *this;
		}

		MATH_INLINE Self operator + (const Self& other) const
		{
			return { x + other.x, y + other.y, z + other.z };
		}

		MATH_INLINE Self& operator += (const Self& other)
		{
			x += other.x;
			y += other.y;
			z += other.z;
			return *this;
		}

		MATH_INLINE Self operator - (const Self& other) const
		{
			return { x - other.x, y - other.y, z - other.z };
		}

		MATH_INLINE Self& operator -= (const Self& other)
		{
			x -= other.x;
			y -= other.y;
			z -= other.z;
			return *this;
		}

		MATH_INLINE Self operator * (const Self& other) const
		{
			return { x * other.x, y * other.y, z * other.z };
		}

		MATH_INLINE Self& operator *= (const Self& other)
		{
			x *= other.x;
			y *= other.y;
			z *= other.z;
			return *this;
		}

		MATH_INLINE Self operator / (const Self& other) const
		{
			return { x / other.x, y / other.y, z / other.z };
		}

		MATH_INLINE Self& operator /= (const Self& other)
		{
			x /= other.x;
			y /= other.y;
			z /= other.z;
			return *this;
		}

		MATH_INLINE Self operator % (const Self& other) const
		{
			return { x % other.x, y % other.y, z % other.z };
		}

		MATH_INLINE Self& operator %= (const Self& other)
		{
			x %= other.x;
			y %= other.y;
			z %= other.z;
			return *this;
		}

		MATH_INLINE Self operator - () const
		{
			return { -x, -y, -z };
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
			return { glm::abs(x), glm::abs(y), glm::abs(z) };
		}

		MATH_INLINE Self Min(const Self& other) const
		{
			return { glm::min(x, other.x), glm::min(y, other.y), glm::min(z, other.z) };
		}

		MATH_INLINE Self Max(const Self& other) const
		{
			return { glm::max(x, other.x), glm::max(y, other.y), glm::max(z, other.z) };
		}

		MATH_INLINE Self AsRadians() const
		{
			return { glm::radians(x), glm::radians(y), glm::radians(z) };
		}

		MATH_INLINE Self AsDegrees() const
		{
			return { glm::degrees(x), glm::degrees(y), glm::degrees(z) };
		}

		MATH_INLINE Self& Lerp(const Self& other, const T& a)
		{
			*this = glm::mix((const GLMT&)*this, (const GLMT&)other, a);
			return *this;
		}

		MATH_INLINE Self& MixWith(const Self& other, const T& a)
		{
			return Lerp(other, a);
		}

		// -----------------------------------------------------------------
	};
	static_assert(sizeof(TVector3<float>) == sizeof(float) * 3);

	template<typename T>
	TVector3<T> operator + (T scalar, const TVector3<T>& vec)
	{
		return vec + scalar;
	}

	template<typename T>
	TVector3<T> operator - (T scalar, const TVector3<T>& vec)
	{
		return { scalar - vec.x, scalar - vec.y, scalar - vec.z };
	}

	template<typename T>
	TVector3<T> operator * (T scalar, const TVector3<T>& vec)
	{
		return vec * scalar;
	}

	template<typename T>
	TVector3<T> operator / (T scalar, const TVector3<T>& vec)
	{
		return { scalar / vec.x, scalar / vec.y, scalar / vec.z };
	}

	template<typename T>
	MATH_INLINE T Dot(const TVector3<T>& a, const TVector3<T>& b) { return a.Dot(b); }
	template<typename T>
	MATH_INLINE T Length(const TVector3<T>& vec) { return vec.Length(); }
	template<typename T>
	MATH_INLINE T LengthSqr(const TVector3<T>& vec) { return vec.LengthSqr(); }

	using Vector3f = TVector3<float>;
	using Vector3d = TVector3<double>;
	using Vector3i = TVector3<int>;
	using Vector3l = TVector3<int64_t>;
	using Vector3u = TVector3<uint32_t>;
	using Vector3ul = TVector3<uint64_t>;
}

#pragma warning (pop)