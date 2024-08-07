#pragma once
#include "Engine.Math/MathLib.h"
#include "Vector3.h"

#pragma warning (push)
#pragma warning (disable : 26495) // 'data' member of Vector is intentionally uninitialized.

namespace Eng::Math
{
	template <typename T>
	struct TVector4
	{
		using ScalarT = T;
		using Self = TVector4<T>;
		using GLMT = glm::tvec4<T>;
		
		union
		{
			struct
			{
				T x;
				T y;
				T z;
				T w;
			};
			struct
			{
				T r;
				T g;
				T b;
				T a;
			};
			T data[4];
		};

		// -----------------------------------------------------------------
		// Constructors

		TVector4()
			: x(0.0f)
			, y(0.0f)
			, z(0.0f)
			, w(0.0f)
		{
		}

		TVector4(const GLMT& vec)
			: x(vec.x)
			, y(vec.y)
			, z(vec.z)
			, w(vec.w)
		{
		}

		TVector4(T all)
			: x(all)
			, y(all)
			, z(all)
			, w(all)
		{
		}

		TVector4(T x_, T y_, T z_, T w_)
			: x(x_)
			, y(y_)
			, z(z_)
			, w(w_)
		{
		}

		TVector4(const TVector3<T>& xyz, T w_ = 1.0f)
			: x(xyz.x)
			, y(xyz.y)
			, z(xyz.z)
			, w(w_)
		{
		}

		TVector4(T x_, const TVector3<T>& yzw)
			: x(x_)
			, y(yzw.x)
			, z(yzw.y)
			, w(yzw.z)
		{
		}

		TVector4(const TVector2<T>& xy, const TVector2<T>& zw)
			: x(xy.x)
			, y(xy.y)
			, z(zw.x)
			, w(zw.y)
		{
		}

		TVector4(const TVector2<T>& xy, T z_ = 1.0f, T w_ = 1.0f)
			: x(xy.x)
			, y(xy.y)
			, z(z_)
			, w(w_)
		{
		}

		TVector4(T x_, T y_, const TVector2<T>& zw)
			: x(x_)
			, y(y_)
			, z(zw.x)
			, w(zw.y)
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

		MATH_INLINE operator const TVector3<T>& () const
		{
			return *((const TVector3<T>*)this);
		}

		// -----------------------------------------------------------------

		// -----------------------------------------------------------------
		// Copy Assignment

		MATH_INLINE Self& operator = (const T* data_)
		{
			x = data_[0];
			y = data_[1];
			z = data_[2];
			w = data_[3];
			return *this;
		}

		MATH_INLINE Self& operator = (const Self& other)
		{
			x = other.x;
			y = other.y;
			z = other.z;
			w = other.w;
			return *this;
		}

		// -----------------------------------------------------------------

		// -----------------------------------------------------------------
		// Comparison

		MATH_INLINE bool operator == (const T* data_) const
		{
			return x == data_[0] && y == data_[1] && z == data_[2] && w == data_[3];
		}

		MATH_INLINE bool operator == (const Self& other) const
		{
			return x == other.x && y == other.y && z == other.z && w == other.w;
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
			return { x + val, y + val, z + val, w + val };
		}

		MATH_INLINE Self& operator += (const T& val)
		{
			x += val;
			y += val;
			z += val;
			w += val;
			return *this;
		}

		MATH_INLINE Self operator - (const T& val) const
		{
			return { x - val, y - val, z - val, w - val };
		}

		MATH_INLINE Self& operator -= (const T& val)
		{
			x -= val;
			y -= val;
			z -= val;
			w -= val;
			return *this;
		}

		MATH_INLINE Self operator * (const T& val) const
		{
			return { x * val, y * val, z * val, w * val };
		}

		MATH_INLINE Self& operator *= (const T& val)
		{
			x *= val;
			y *= val;
			z *= val;
			w *= val;
			return *this;
		}

		MATH_INLINE Self operator / (const T& val) const
		{
			return { x / val, y / val, z / val, w / val };
		}

		MATH_INLINE Self& operator /= (const T& val)
		{
			x /= val;
			y /= val;
			z /= val;
			w /= val;
			return *this;
		}

		MATH_INLINE Self operator % (const T& val) const
		{
			return { x % val, y % val, z % val, w % val };
		}

		MATH_INLINE Self& operator %= (const T& val)
		{
			x %= val;
			y %= val;
			z %= val;
			w %= val;
			return *this;
		}

		MATH_INLINE Self operator + (const Self& other) const
		{
			return { x + other.x, y + other.y, z + other.z, w + other.w };
		}

		MATH_INLINE Self& operator += (const Self& other)
		{
			x += other.x;
			y += other.y;
			z += other.z;
			w += other.w;
			return *this;
		}

		MATH_INLINE Self operator - (const Self& other) const
		{
			return { x - other.x, y - other.y, z - other.z, w - other.w };
		}

		MATH_INLINE Self& operator -= (const Self& other)
		{
			x -= other.x;
			y -= other.y;
			z -= other.z;
			w -= other.w;
			return *this;
		}

		MATH_INLINE Self operator * (const Self& other) const
		{
			return { x * other.x, y * other.y, z * other.z, w * other.w };
		}

		MATH_INLINE Self& operator *= (const Self& other)
		{
			x *= other.x;
			y *= other.y;
			z *= other.z;
			w *= other.w;
			return *this;
		}

		MATH_INLINE Self operator / (const Self& other) const
		{
			return { x / other.x, y / other.y, z / other.z, w / other.w };
		}

		MATH_INLINE Self& operator /= (const Self& other)
		{
			x /= other.x;
			y /= other.y;
			z /= other.z;
			w /= other.w;
			return *this;
		}

		MATH_INLINE Self operator % (const Self& other) const
		{
			return { x % other.x, y % other.y, z % other.z, w % other.w };
		}

		MATH_INLINE Self& operator %= (const Self& other)
		{
			x %= other.x;
			y %= other.y;
			z %= other.z;
			w %= other.w;
			return *this;
		}

		MATH_INLINE Self operator - () const
		{
			return { -x, -y, -z, -w };
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
			GLMT3 res = glm::cross(selfRef, (const GLMT3&)other);
			return { res.x, res.y, res.z, w };
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
			return { glm::abs(x), glm::abs(y), glm::abs(z), glm::abs(w) };
		}

		MATH_INLINE Self Min(const Self& other) const
		{
			return { glm::min(x, other.x), glm::min(y, other.y), glm::min(z, other.z), glm::min(w, other.w) };
		}

		MATH_INLINE Self Max(const Self& other) const
		{
			return { glm::max(x, other.x), glm::max(y, other.y), glm::max(z, other.z), glm::max(w, other.w) };
		}

		MATH_INLINE Self AsRadians() const
		{
			return { glm::radians(x), glm::radians(y), glm::radians(z), glm::radians(w) };
		}

		MATH_INLINE Self AsDegrees() const
		{
			return { glm::degrees(x), glm::degrees(y), glm::degrees(z), glm::degrees(w) };
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
	static_assert(sizeof(TVector4<float>) == sizeof(float) * 4);

	template<typename T>
	TVector4<T> operator + (T scalar, const TVector4<T>& vec)
	{
		return vec + scalar;
	}

	template<typename T>
	TVector4<T> operator - (T scalar, const TVector4<T>& vec)
	{
		return { scalar - vec.x, scalar - vec.y, scalar - vec.z, scalar - vec.w };
	}

	template<typename T>
	TVector4<T> operator * (T scalar, const TVector4<T>& vec)
	{
		return vec * scalar;
	}

	template<typename T>
	TVector4<T> operator / (T scalar, const TVector4<T>& vec)
	{
		return { scalar / vec.x, scalar / vec.y, scalar / vec.z, scalar / vec.w };
	}

	template<typename T>
	MATH_INLINE T Dot(const TVector4<T>& a, const TVector4<T>& b) { return a.Dot(b); }
	template<typename T>
	MATH_INLINE T Length(const TVector4<T>& vec) { return vec.Length(); }
	template<typename T>
	MATH_INLINE T LengthSqr(const TVector4<T>& vec) { return vec.LengthSqr(); }

	using Vector4f = TVector4<float>;
	using Vector4d = TVector4<double>;
	using Vector4i = TVector4<int>;
	using Vector4l = TVector4<int64_t>;
	using Vector4u = TVector4<uint32_t>;
	using Vector4ul = TVector4<uint64_t>;
}

#pragma warning (pop)