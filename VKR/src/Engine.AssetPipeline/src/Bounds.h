#pragma once
#include "Engine.Math/Scalar.h"
#include "Engine.Math/Vector3.h"
#include "Engine.Math/Matrix4.h"

namespace AssetPipeline
{
	struct Bounds
	{
		Eng::Math::Vector3f m_origin = { 0.0f, 0.0f, 0.0f };
		Eng::Math::Vector3f m_extents = { 1.0f, 1.0f, 1.0f };

		Bounds() = default;

		Bounds(const Eng::Math::Vector3f& origin, Eng::Math::Vector3f& extents)
		{
			m_origin = origin;
			m_extents = extents;
		}

		static const Bounds Identity()
		{
			Bounds b;
			b.m_origin = Eng::Math::Vector3f(0.0f);
			b.m_extents = Eng::Math::Vector3f(1.0f);

			return b;
		}

		inline bool IsZero() const { return m_origin + m_extents == Eng::Math::Vector3f(0.0f); }
		inline bool IsIdentity() const 
		{ 
			return m_origin == Eng::Math::Vector3f(0.0f) && m_extents == Eng::Math::Vector3f(1.0f);
		}

		inline float MaxX() const { return m_origin.x + m_extents.x; }
		inline float MaxY() const { return m_origin.y + m_extents.y; }
		inline float MaxZ() const { return m_origin.z + m_extents.z; }

		inline Eng::Math::Vector3f Center() const
		{
			return m_origin + (m_extents * 0.5f);
		}

		inline float Volume() const
		{
			return m_extents.x * m_extents.y * m_extents.z;
		}

		inline bool operator == (const Bounds& other) const
		{
			return m_origin == other.m_origin && m_extents == other.m_extents;
		}

		inline bool IntersectsWith(const Bounds& other) const
		{
			return	(m_origin.x <= other.MaxX() && MaxX() >= other.m_origin.x) &&
				(m_origin.y <= other.MaxY() && MaxY() >= other.m_origin.y) &&
				(m_origin.z <= other.MaxZ() && MaxZ() >= other.m_origin.z);
		}

		inline bool Contains(const Eng::Math::Vector3f& point) const
		{
			return point.x >= m_origin.x && point.y >= m_origin.y && point.z >= m_origin.z
				&& point.x <= MaxX() && point.y <= MaxY() && point.z <= MaxZ();
		}

		inline bool RoughlyContains(const Eng::Math::Vector3f& point) const
		{
			static constexpr float Tolerance = 0.001f;

			return point.x >= m_origin.x - Tolerance
				&& point.y >= m_origin.y - Tolerance
				&& point.z >= m_origin.z - Tolerance
				&& point.x <= MaxX() + Tolerance
				&& point.y <= MaxY() + Tolerance
				&& point.z <= MaxZ() + Tolerance;
		}

		inline bool Encapsulates(const Bounds& other) const
		{
			return (m_origin.x <= other.m_origin.x && m_origin.y <= other.m_origin.y && m_origin.z <= other.m_origin.z)
				&& (MaxX() >= other.MaxX() && MaxY() >= other.MaxY() && MaxZ() >= other.MaxZ());
		}

		inline void Encapsulate(const Bounds& other)
		{
			m_extents.x = Eng::Math::Max(MaxX(), other.MaxX());
			m_extents.y = Eng::Math::Max(MaxY(), other.MaxY());
			m_extents.z = Eng::Math::Max(MaxZ(), other.MaxZ());

			m_origin.x = Eng::Math::Min(m_origin.x, other.m_origin.x);
			m_origin.y = Eng::Math::Min(m_origin.y, other.m_origin.y);
			m_origin.z = Eng::Math::Min(m_origin.z, other.m_origin.z);

			m_extents -= m_origin;
		}

		inline void Encapsulate(const Eng::Math::Vector3f& point)
		{
			m_extents.x = Eng::Math::Max(MaxX(), point.x);
			m_extents.y = Eng::Math::Max(MaxY(), point.y);
			m_extents.z = Eng::Math::Max(MaxZ(), point.z);

			m_origin.x = Eng::Math::Min(m_origin.x, point.x);
			m_origin.y = Eng::Math::Min(m_origin.y, point.y);
			m_origin.z = Eng::Math::Min(m_origin.z, point.z);

			m_extents -= m_origin;
		}

		inline void Transform(const Eng::Math::Matrix4& matrix)
		{
			struct Vertices
			{
				Eng::Math::Vector4f vertices[8];
			};

			Vertices v
			{
				Eng::Math::Vector4f(m_origin),
				Eng::Math::Vector4f(m_origin.x + m_extents.x, m_origin.y, m_origin.z, 1.0f),
				Eng::Math::Vector4f(m_origin.x, m_origin.y, m_origin.z + m_extents.z, 1.0f),
				Eng::Math::Vector4f(m_origin.x + m_extents.x, m_origin.y, m_origin.z + m_extents.z, 1.0f),
				Eng::Math::Vector4f(m_origin.x, m_origin.y + m_extents.y, m_origin.z, 1.0f),
				Eng::Math::Vector4f(m_origin.x + m_extents.x, m_origin.y + m_extents.y, m_origin.z, 1.0f),
				Eng::Math::Vector4f(m_origin.x, m_origin.y + m_extents.y, m_origin.z + m_extents.z, 1.0f),
				Eng::Math::Vector4f(m_origin.x + m_extents.x, m_origin.y + m_extents.y, m_origin.z + m_extents.z, 1.0f)
			};

			Eng::Math::Vector3f newOrigin(INFINITY);
			Eng::Math::Vector3f newExtents(-INFINITY);
			for (Eng::Math::Vector4f& vert : v.vertices)
			{
				vert = matrix * vert;
				newOrigin = Eng::Math::Vector3f(Eng::Math::Min(newOrigin.x, vert.x), Eng::Math::Min(newOrigin.y, vert.y), Eng::Math::Min(newOrigin.z, vert.z));
				newExtents = Eng::Math::Vector3f(Eng::Math::Max(newExtents.x, vert.x), Eng::Math::Max(newExtents.y, vert.y), Eng::Math::Max(newExtents.z, vert.z));
			}

			m_origin = newOrigin;
			m_extents = newExtents - newOrigin;
		}
	};
};