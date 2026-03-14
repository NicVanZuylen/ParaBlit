#pragma once
#include <Engine.Math/Scalar.h>
#include <Engine.Math/Vectors.h>
#include <Engine.Math/Matrix4.h>

namespace Eng
{
	using namespace Math;

	struct Bounds
	{
		Vector3f m_origin = { 0.0f, 0.0f, 0.0f };
		Vector3f m_extents = { 1.0f, 1.0f, 1.0f };

		Bounds() = default;

		Bounds(const Vector3f& origin, const Vector3f& extents)
		{
			m_origin = origin;
			m_extents = extents;
		}

		static const Bounds Identity()
		{
			return 
			{ 
				{
					0.0f, 0.0f, 0.0f
				}, 
				{
					1.0f, 1.0f, 1.0f
				} 
			};
		}

		inline bool IsZero() const { return m_origin + m_extents == Vector3f(0.0f); }
		inline bool IsIdentity() const 
		{ 
			return m_origin == Vector3f(0.0f) && m_extents == Vector3f(1.0f);
		}

		inline float MaxX() const { return m_origin.x + m_extents.x; }
		inline float MaxY() const { return m_origin.y + m_extents.y; }
		inline float MaxZ() const { return m_origin.z + m_extents.z; }

		inline Vector3f Center() const
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

		inline bool RoughlyEquals(const Bounds& other) const
		{
			static constexpr float Tolerance = 0.00001f;

			Vector3f diffOrigin = Abs(m_origin - other.m_origin);
			Vector3f diffExtents = Abs(m_extents - other.m_extents);

			return diffOrigin.x <= Tolerance && diffOrigin.y <= Tolerance && diffOrigin.z <= Tolerance 
				&& diffExtents.x <= Tolerance && diffExtents.y <= Tolerance && diffExtents.z <= Tolerance;
		}

		inline bool IntersectsWith(const Bounds& other) const
		{
			return	(m_origin.x <= other.MaxX() && MaxX() >= other.m_origin.x) &&
				(m_origin.y <= other.MaxY() && MaxY() >= other.m_origin.y) &&
				(m_origin.z <= other.MaxZ() && MaxZ() >= other.m_origin.z);
		}

		inline bool Encapsulates(const Bounds& other) const
		{
			return (m_origin.x <= other.m_origin.x && m_origin.y <= other.m_origin.y && m_origin.z <= other.m_origin.z)
				&& (MaxX() >= other.MaxX() && MaxY() >= other.MaxY() && MaxZ() >= other.MaxZ());
		}

		inline void Encapsulate(const Bounds& other)
		{
			m_extents.x = Max(MaxX(), other.MaxX());
			m_extents.y = Max(MaxY(), other.MaxY());
			m_extents.z = Max(MaxZ(), other.MaxZ());

			m_origin.x = Min(m_origin.x, other.m_origin.x);
			m_origin.y = Min(m_origin.y, other.m_origin.y);
			m_origin.z = Min(m_origin.z, other.m_origin.z);

			m_extents -= m_origin;
		}

		inline void Transform(const Eng::Math::Matrix4& matrix)
		{
			struct Vertices
			{
				// union
				// {
				// 	struct
				// 	{
				// 		Eng::Math::Vector4f botLeft;
				// 		Eng::Math::Vector4f botRight;
				// 		Eng::Math::Vector4f botBackLeft;
				// 		Eng::Math::Vector4f botBackRight;
				// 		Eng::Math::Vector4f topLeft;
				// 		Eng::Math::Vector4f topRight;
				// 		Eng::Math::Vector4f topBackLeft;
				// 		Eng::Math::Vector4f topBackRight;
				// 	};
				// 	Eng::Math::Vector4f vertices[8];
				// };
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

		inline bool IsIntersectingWithRay(const Vector3f& rayOrigin, const Vector3f& rayDirection, float& outDistance) const
		{
			// Cyrus-Beck clipping method, where t = ray distance.

			float tMinX = (m_origin.x - rayOrigin.x) / rayDirection.x;
			float tMaxX = (MaxX() - rayOrigin.x) / rayDirection.x;
			float tMinY = (m_origin.y - rayOrigin.y) / rayDirection.y;
			float tMaxY = (MaxY() - rayOrigin.y) / rayDirection.y;
			float tMinZ = (m_origin.z - rayOrigin.z) / rayDirection.z;
			float tMaxZ = (MaxZ() - rayOrigin.z) / rayDirection.z;

			float tMin = Max(Max(Min(tMinX, tMaxX), Min(tMinY, tMaxY)), Min(tMinZ, tMaxZ));
			float tMax = Min(Min(Max(tMinX, tMaxX), Max(tMinY, tMaxY)), Max(tMinZ, tMaxZ));

			outDistance = Max(tMin, tMax);

			if (tMax < 0.0f)
				return false;

			if (tMin > tMax)
				return false;

			return true;
		}

		// Returns which inner face of the AABB the ray intersects with (if any).
		// 0 == MinX, 1 == MinY, 2 == MinZ, 3 == MaxX, 4 == MaxY, 5 == MaxZ.
		// Returns -1 if the ray does not intersect with the AABB.
		inline int GetInnerFaceIntersectingWithRay(const Vector3f& rayOrigin, const Vector3f& rayDirection, float& outDistance) const
		{
			// Cyrus-Beck clipping method, where t = ray distance.

			float tMinX = (m_origin.x - rayOrigin.x) / rayDirection.x;
			float tMaxX = (MaxX() - rayOrigin.x) / rayDirection.x;
			float tMinY = (m_origin.y - rayOrigin.y) / rayDirection.y;
			float tMaxY = (MaxY() - rayOrigin.y) / rayDirection.y;
			float tMinZ = (m_origin.z - rayOrigin.z) / rayDirection.z;
			float tMaxZ = (MaxZ() - rayOrigin.z) / rayDirection.z;

			float tMin = Max(Max(Min(tMinX, tMaxX), Min(tMinY, tMaxY)), Min(tMinZ, tMaxZ));
			float tMax = Min(Min(Max(tMinX, tMaxX), Max(tMinY, tMaxY)), Max(tMinZ, tMaxZ));

			outDistance = Max(tMin, tMax);

			if (tMax < 0.0f)
				return -1;

			if (tMin > tMax)
				return -1;

			if (outDistance == tMinX)
			{
				return 0;
			}
			else if (outDistance == tMinY)
			{
				return 1;
			}
			else if (outDistance == tMinZ)
			{
				return 2;
			}
			else if (outDistance == tMaxX)
			{
				return 3;
			}
			else if (outDistance == tMaxY)
			{
				return 4;
			}
			else
			{
				return 5;
			}
		}
	};
};