#pragma once
#include "Engine.Math/MathLib.h"
#include "Vectors.h"

#pragma warning (push)
#pragma warning (disable : 26495) // 'data' member of Vector is intentionally uninitialized.

namespace Eng::Math
{
	template <typename T>
	struct TMatrix4
	{
		using Self = TMatrix4<T>;
		using VecT = TVector4<T>;
		using GLMT = glm::tmat4x4<T>;
		
		union
		{
			VecT columns[4];
			T data[16];
			GLMT mat;
		};

		// -----------------------------------------------------------------
		// Constructors

		TMatrix4()
			: mat(glm::identity<GLMT>())
		{
		}

		TMatrix4(const GLMT& mat_)
			: mat(mat_)
		{
		}

		TMatrix4(VecT x, VecT y, VecT z, VecT w)
		{
			columns[0] = x;
			columns[1] = y;
			columns[2] = z;
			columns[3] = w;
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

		MATH_INLINE Self& operator = (const T* data_)
		{
			std::memcpy(data, data_, sizeof(Self));
			return *this;
		}

		MATH_INLINE Self& operator = (const Self& other)
		{
			std::memcpy(data, other.data, sizeof(Self));
			return *this;
		}

		// -----------------------------------------------------------------

		// -----------------------------------------------------------------
		// Subscript
		
		MATH_INLINE const VecT& operator [] (size_t i) const
		{
			return columns[i];
		}

		MATH_INLINE VecT& operator [] (size_t i)
		{
			return columns[i];
		}

		// -----------------------------------------------------------------

		// -----------------------------------------------------------------
		// Comparison

		MATH_INLINE bool operator == (const T* data_) const
		{
			return memcmp(data, data_, sizeof(Self)) == 0;
		}

		MATH_INLINE bool operator == (const Self& other) const
		{
			return mat == other.mat;
		}

		// -----------------------------------------------------------------

		// -----------------------------------------------------------------
		// Arithmetic

		MATH_INLINE Self operator * (const Self& other) const
		{
			return mat * other.mat;
		}

		MATH_INLINE TVector4<T> operator * (const TVector4<T>& vec) const
		{
			return mat * (const TVector4<T>::GLMT&)vec;
		}

		MATH_INLINE Self& operator *= (const Self& other)
		{
			mat *= other.mat;
			return *this;
		}

		// -----------------------------------------------------------------

		// -----------------------------------------------------------------
		// Math functions

		MATH_INLINE Self ToIdentity()
		{
			mat = glm::identity<GLMT>();
		}

		MATH_INLINE static Self Identity()
		{
			return glm::identity<GLMT>();
		}

		MATH_INLINE Self& Invert()
		{
			mat = glm::inverse(mat);
			return *this;
		}

		MATH_INLINE Self Inverse() const
		{
			return glm::inverse(mat);
		}

		MATH_INLINE Self& Translate(const TVector3<T>& translation)
		{
			mat = glm::translate(mat, (const TVector3<T>::GLMT&)translation);
			return *this;
		}

		MATH_INLINE Self Translated(const TVector3<T>& translation) const
		{
			return glm::translate(mat, (const TVector3<T>::GLMT&)translation);
		}

		MATH_INLINE Self& Scale(const TVector3<T>& scale)
		{
			mat = glm::scale(mat, (const TVector3<T>::GLMT&)scale);
			return *this;
		}

		MATH_INLINE Self Scaled(const TVector3<T>& scale) const
		{
			return glm::scale(mat, (const TVector3<T>::GLMT&)scale);
		}

		MATH_INLINE Self& Rotate(T angle, const TVector3<T>& axis)
		{
			mat = glm::rotate(mat, angle, (const TVector3<T>::GLMT&)axis);
			return *this;
		}

		MATH_INLINE Self& RotateX(T angle)
		{
			mat = glm::rotate(mat, angle, glm::vec3(1.0f, 0.0f, 0.0f));
			return *this;
		}

		MATH_INLINE Self& RotateY(T angle)
		{
			mat = glm::rotate(mat, angle, glm::vec3(0.0f, 1.0f, 0.0f));
			return *this;
		}

		MATH_INLINE Self& RotateZ(T angle)
		{
			mat = glm::rotate(mat, angle, glm::vec3(0.0f, 0.0f, 1.0f));
			return *this;
		}

		MATH_INLINE Self Rotated(T angle, const TVector3<T>& axis) const
		{
			return glm::rotate(mat, angle, (const TVector3<T>::GLMT&)axis);
		}

		MATH_INLINE Self RotatedX(T angle) const
		{
			return glm::rotate(mat, angle, glm::vec3(1.0f, 0.0f, 0.0f));
		}

		MATH_INLINE Self RotatedY(T angle) const
		{
			return glm::rotate(mat, angle, glm::vec3(0.0f, 1.0f, 0.0f));
		}

		MATH_INLINE Self RotatedZ(T angle) const
		{
			return glm::rotate(mat, angle, glm::vec3(0.0f, 0.0f, 1.0f));
		}

		MATH_INLINE static Self LookAt(const TVector3<T>& eye, const TVector3<T>& center, const TVector3<T>& up)
		{
			using VecGLMT = TVector3<T>::GLMT;
			return Self(glm::lookAt((const VecGLMT&)eye, (const VecGLMT&)center, (const VecGLMT&)up));
		}

		// -----------------------------------------------------------------
	};
	static_assert(sizeof(TMatrix4<float>) == sizeof(float) * 16);

	template<typename TMat>
	MATH_INLINE TMat Inverse(const TMat& mat)
	{
		return mat.Inverse();
	}

	template<typename TMat>
	MATH_INLINE TMat& Invert(TMat& mat)
	{
		return mat.Invert();
	}

	template<typename T>
	MATH_INLINE void Translate(TMatrix4<T>& mat, const TVector3<T>& translation)
	{
		mat = glm::translate(mat, translation);
	}

	template<typename T>
	MATH_INLINE TMatrix4<T> Translated(const TMatrix4<T>& mat, const TVector3<T>& translation)
	{
		return glm::translate(mat, translation);
	}

	template<typename T>
	MATH_INLINE void Scale(TMatrix4<T>& mat, const TVector3<T>& scale)
	{
		mat = glm::scale(mat, scale);
	}

	template<typename T>
	MATH_INLINE TMatrix4<T> Scaled(const TMatrix4<T>& mat, const TVector3<T>& scale)
	{
		return glm::scale(mat, scale);
	}

	template<typename T>
	MATH_INLINE void Rotate(TMatrix4<T>& mat, T angle, const TVector3<T>& axis)
	{
		mat = glm::rotate(mat, angle, axis);
	}

	template<typename T>
	MATH_INLINE void RotateX(TMatrix4<T>& mat, T angle)
	{
		mat = glm::rotate(mat, angle, glm::vec3(1.0f, 0.0f, 0.0f));
	}

	template<typename T>
	MATH_INLINE void RotateY(TMatrix4<T>& mat, T angle)
	{
		mat = glm::rotate(mat, angle, glm::vec3(0.0f, 1.0f, 0.0f));
	}

	template<typename T>
	MATH_INLINE void RotateZ(TMatrix4<T>& mat, T angle)
	{
		mat = glm::rotate(mat, angle, glm::vec3(0.0f, 0.0f, 1.0f));
	}

	template<typename T>
	MATH_INLINE TMatrix4<T> Rotated(const TMatrix4<T>& mat, T angle, const TVector3<T>& axis)
	{
		return glm::rotate(mat, angle, axis);
	}

	template<typename T>
	MATH_INLINE TMatrix4<T> RotatedX(const TMatrix4<T>& mat, T angle)
	{
		return glm::rotate(mat, angle, glm::vec3(1.0f, 0.0f, 0.0f));
	}

	template<typename T>
	MATH_INLINE TMatrix4<T> RotatedY(const TMatrix4<T>& mat, T angle)
	{
		return glm::rotate(mat, angle, glm::vec3(0.0f, 1.0f, 0.0f));
	}

	template<typename T>
	MATH_INLINE TMatrix4<T> RotatedZ(const TMatrix4<T>& mat, T angle)
	{
		return glm::rotate(mat, angle, glm::vec3(0.0f, 0.0f, 1.0f));
	}

	using Matrix4 = TMatrix4<float>;
	using Matrix4d = TMatrix4<double>;
}

#pragma warning (pop)