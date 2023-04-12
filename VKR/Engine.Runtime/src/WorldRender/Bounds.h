#pragma once
#pragma warning(push, 0)
#define GLM_FORCE_CTOR_INIT
#include "glm/glm.hpp"
#pragma warning(pop)

namespace Eng
{
	struct Bounds
	{
		glm::vec3 m_origin = { 0.0f, 0.0f, 0.0f };
		glm::vec3 m_extents = { 1.0f, 1.0f, 1.0f };

		Bounds() = default;

		Bounds(const glm::vec3& origin, const glm::vec3& extents)
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

		inline bool IsZero() const { return m_origin + m_extents == glm::vec3(0.0f); }

		inline float MaxX() const { return m_origin.x + m_extents.x; }
		inline float MaxY() const { return m_origin.y + m_extents.y; }
		inline float MaxZ() const { return m_origin.z + m_extents.z; }

		inline glm::vec3 Centre() const
		{
			return m_origin + (m_extents * 0.5f);
		}

		inline float Volume() const
		{
			return m_extents.x * m_extents.y * m_extents.z;
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
			m_extents.x = glm::max(MaxX(), other.MaxX());
			m_extents.y = glm::max(MaxY(), other.MaxY());
			m_extents.z = glm::max(MaxZ(), other.MaxZ());

			m_origin.x = glm::min(m_origin.x, other.m_origin.x);
			m_origin.y = glm::min(m_origin.y, other.m_origin.y);
			m_origin.z = glm::min(m_origin.z, other.m_origin.z);

			m_extents -= m_origin;
		}

		inline void Transform(const glm::mat4& matrix)
		{
			struct Vertices
			{
				union
				{
					struct
					{
						glm::vec4 botLeft;
						glm::vec4 botRight;
						glm::vec4 botBackLeft;
						glm::vec4 botBackRight;
						glm::vec4 topLeft;
						glm::vec4 topRight;
						glm::vec4 topBackLeft;
						glm::vec4 topBackRight;
					};
					glm::vec4 vertices[8];
				};
			};

			Vertices v
			{
				glm::vec4(m_origin, 1.0f),
				glm::vec4(m_origin + glm::vec3(m_extents.x, 0.0f, 0.0f), 1.0f),
				glm::vec4(m_origin + glm::vec3(0.0f, 0.0f, m_extents.z), 1.0f),
				glm::vec4(m_origin + glm::vec3(m_extents.x, 0.0f, m_extents.z), 1.0f)
			};

			v.topLeft = v.botLeft;
			v.topLeft.y += m_extents.y;
			v.topRight = v.botRight;
			v.topRight.y += m_extents.y;
			v.topBackLeft = v.botBackLeft;
			v.topBackLeft.y += m_extents.y;
			v.topBackRight = v.botBackRight;
			v.topBackRight.y += m_extents.y;

			glm::vec4 newOrigin(INFINITY);
			glm::vec4 newExtents(-INFINITY);
			for (glm::vec4& vert : v.vertices)
			{
				vert = matrix * vert;
				newOrigin = glm::vec4(glm::min(newOrigin.x, vert.x), glm::min(newOrigin.y, vert.y), glm::min(newOrigin.z, vert.z), 0.0f);
				newExtents = glm::vec4(glm::max(newExtents.x, vert.x), glm::max(newExtents.y, vert.y), glm::max(newExtents.z, vert.z), 0.0f);
			}

			m_origin = newOrigin;
			m_extents = newExtents;
			m_extents -= m_origin;
		}

		inline bool IsIntersectingWithRay(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, float& outDistance)
		{
			// Cyrus-Beck clipping method, where t = ray distance.

			float tMinX = (m_origin.x - rayOrigin.x) / rayDirection.x;
			float tMaxX = (MaxX() - rayOrigin.x) / rayDirection.x;
			float tMinY = (m_origin.y - rayOrigin.y) / rayDirection.y;
			float tMaxY = (MaxY() - rayOrigin.y) / rayDirection.y;
			float tMinZ = (m_origin.z - rayOrigin.z) / rayDirection.z;
			float tMaxZ = (MaxZ() - rayOrigin.z) / rayDirection.z;

			float tMin = glm::max(glm::max(glm::min(tMinX, tMaxX), glm::min(tMinY, tMaxY)), glm::min(tMinZ, tMaxZ));
			float tMax = glm::min(glm::min(glm::max(tMinX, tMaxX), glm::max(tMinY, tMaxY)), glm::max(tMinZ, tMaxZ));

			outDistance = glm::max(tMin, tMax);

			if (tMax < 0.0f)
				return false;

			if (tMin > tMax)
				return false;

			return true;
		}
	};
};