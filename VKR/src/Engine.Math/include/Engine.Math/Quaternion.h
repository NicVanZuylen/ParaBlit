#pragma once
#include "Engine.Math/MathLib.h"
#include "Vectors.h"
#include "Matrix4.h"

#pragma warning (push)
#pragma warning (disable : 26495) // 'data' member of Vector is intentionally uninitialized.

namespace Eng::Math
{
	template <typename T>
	struct TQuaternion
	{
		using Self = TQuaternion<T>;
		using GLMT = glm::tquat<T>;
		
		union
		{
			struct
			{
				T x;
				T y;
				T z;
				T w;
			};
			GLMT quat;
			T data[4];
		};

		// -----------------------------------------------------------------
		// Constructors

		TQuaternion()
			: quat(glm::identity<GLMT>())
		{
		}

		TQuaternion(const GLMT& quat_)
			: quat(quat_)
		{
		}

		TQuaternion(T all)
			: x(all)
			, y(all)
			, z(all)
			, w(all)
		{
		}

		TQuaternion(T x_, T y_, T z_, T w_)
			: x(x_)
			, y(y_)
			, z(z_)
			, w(w_)
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

		MATH_INLINE operator T* ()
		{
			return data;
		}

		MATH_INLINE operator const T* () const
		{
			return data;
		}

		// -----------------------------------------------------------------

		// -----------------------------------------------------------------
		// Copy Assignment

		MATH_INLINE Self& operator = (const T* data)
		{
			x = data[0];
			y = data[1];
			z = data[2];
			w = data[3];
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

		MATH_INLINE bool operator == (const T* data) const
		{
			return x == data[0] && y == data[1] && z == data[2] && w == data[3];
		}

		MATH_INLINE bool operator == (const Self& other) const
		{
			return x == other.x && y == other.y && z == other.z && w == other.w;
		}

		// -----------------------------------------------------------------

		// -----------------------------------------------------------------
		// Arithmetic

		MATH_INLINE Self& operator * (const Self& other)
		{
			quat *= other.quat;
			return *this;
		}

		MATH_INLINE Self operator * (const Self& other) const
		{
			return quat * other.quat;
		}

		MATH_INLINE Self& operator *= (const Self& other)
		{
			quat *= other.quat;
			return *this;
		}

		// -----------------------------------------------------------------

		// -----------------------------------------------------------------
		// Math functions

		MATH_INLINE Self ToIdentity()
		{
			quat = glm::identity<GLMT>();
		}

		MATH_INLINE static Self Identity()
		{
			return glm::identity<GLMT>();
		}

		MATH_INLINE static Self FromEuler(const TVector3<T>& eulerAngles)
		{
			return GLMT(glm::vec3(eulerAngles.x, eulerAngles.y, eulerAngles.z));
		}

		MATH_INLINE Self& Rotate(T angle, const TVector3<T>& axis)
		{
			quat = glm::rotate(quat, angle, glm::vec3(axis.x, axis.y, axis.z));
			return *this;
		}

		MATH_INLINE Self& RotateX(T angle)
		{
			quat = glm::rotate(quat, angle, glm::vec3(1.0f, 0.0f, 0.0f));
			return *this;
		}

		MATH_INLINE Self& RotateY(T angle)
		{
			quat = glm::rotate(quat, angle, glm::vec3(0.0f, 1.0f, 0.0f));
			return *this;
		}

		MATH_INLINE Self& RotateZ(T angle)
		{
			quat = glm::rotate(quat, angle, glm::vec3(0.0f, 0.0f, 1.0f));
			return *this;
		}

		MATH_INLINE Self Rotated(T angle, const TVector3<T>& axis) const
		{
			return glm::rotate(quat, angle, axis);
		}

		MATH_INLINE Self RotatedX(T angle) const
		{
			return glm::rotate(quat, angle, glm::vec3(1.0f, 0.0f, 0.0f));
		}

		MATH_INLINE Self RotatedY(T angle) const
		{
			return glm::rotate(quat, angle, glm::vec3(0.0f, 1.0f, 0.0f));
		}

		MATH_INLINE Self RotatedZ(T angle) const
		{
			return glm::rotate(quat, angle, glm::vec3(0.0f, 0.0f, 1.0f));
		}

		MATH_INLINE TVector3<T> ToEuler() const
		{
			return glm::eulerAngles(quat);
		}

		MATH_INLINE TMatrix4<T> ToMatrix4() const
		{
			return glm::toMat4(quat);
		}

		// -----------------------------------------------------------------
	};
	static_assert(sizeof(TQuaternion<float>) == sizeof(float) * 4);

	template<typename T>
	MATH_INLINE void Rotate(TQuaternion<T>& quat, T angle, const TVector3<T>& axis)
	{
		quat = glm::rotate(quat, angle, axis);
	}

	template<typename T>
	MATH_INLINE void RotateX(TQuaternion<T>& quat, T angle)
	{
		quat = glm::rotate(quat, angle, glm::vec3(1.0f, 0.0f, 0.0f));
	}

	template<typename T>
	MATH_INLINE void RotateY(TQuaternion<T>& quat, T angle)
	{
		quat = glm::rotate(quat, angle, glm::vec3(0.0f, 1.0f, 0.0f));
	}

	template<typename T>
	MATH_INLINE void RotateZ(TQuaternion<T>& quat, T angle)
	{
		quat = glm::rotate(quat, angle, glm::vec3(0.0f, 0.0f, 1.0f));
	}

	using Quaternion = TQuaternion<float>;
	using Quaterniond = TQuaternion<double>;
}

#pragma warning (pop)