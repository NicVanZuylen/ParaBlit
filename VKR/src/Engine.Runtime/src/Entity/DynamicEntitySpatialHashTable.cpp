#include "DynamicEntitySpatialHashTable.h"
#include "RenderGraphPasses/DebugLinePass.h"

namespace Eng
{
	using namespace Math;

	DynamicEntitySpatialHashTable::~DynamicEntitySpatialHashTable()
	{
	}

	void DynamicEntitySpatialHashTable::Init(CLib::Allocator* allocator)
	{
		m_allocator = allocator;
		
	}

	void DynamicEntitySpatialHashTable::Destroy()
	{
		
	}

	void DynamicEntitySpatialHashTable::Reset()
	{
		m_entityCount = Math::Max(m_entityCount, 1u);
		m_cellDimensions = 2.0f * Math::Max(m_averageMaxDimension / m_entityCount, 0.5f);
		m_averageMaxDimension = 0.0f;
		m_entityCount = 0;

		// TODO: Rather than deleting all cell entries into the hash table (thus, freeing all vector elements) every frame,
		// we should just clear the cell vectors and wait until the cell count exceeds a certain threshold (determined by memory budget for the SHT)
		// before clearing the whole hash map. There is probably a noteworthy amount of overhead for completely clearing the map rather than just zeroing the cell vector counts.
		m_map.clear();
	}

	Entity* DynamicEntitySpatialHashTable::RaycastGetEntity(const Vector3f& rayOrigin, const Vector3f& rayDirection, float rayRange)
	{
		Vector3i cellPos = ToCellPosition(rayOrigin);
		float distanceSearched = 0.0f;
		while (distanceSearched < rayRange)
		{
			Bounds cellBounds
			(
				Vector3f
				(
					float(cellPos.x),
					float(cellPos.y),
					float(cellPos.z)
				) * m_cellDimensions,
				Vector3f(m_cellDimensions)
			);

			auto it = m_map.find(cellPos);
			if (it != m_map.end())
			{
				auto& cell = it->second;
				Entity* hitEntity = nullptr;
				float minDist = INFINITY;

				for (Entity* e : cell)
				{
					const Bounds& entityBounds = e->GetComponent<DynamicEntityTracker>()->GetEntityWorldBounds();

					float dist;
					if (entityBounds.IsIntersectingWithRay(rayOrigin, rayDirection, dist) && dist < minDist && dist < rayRange)
					{
						minDist = dist;
						hitEntity = e;
					}
				}

				if (hitEntity != nullptr)
				{
					return hitEntity;
				}
			}

			float cellDist;
			int cellFace = cellBounds.GetInnerFaceIntersectingWithRay(rayOrigin, rayDirection, cellDist);
			distanceSearched = cellDist;

			switch (cellFace)
			{
				case 0:
				{
					cellPos.x -= 1;
					break;
				}
				case 1:
				{
					cellPos.y -= 1;
					break;
				}
				case 2:
				{
					cellPos.z -= 1;
					break;
				}
				case 3:
				{
					cellPos.x += 1;
					break;
				}
				case 4:
				{
					cellPos.y += 1;
					break;
				}
				case 5:
				{
					cellPos.z += 1;
					break;
				}
				default:
					assert(false && "The ray didn't intersect with the cell. This shouldn't be possible. Verify the math here is correct!");
					break;
			}
		}

		return nullptr;
	}

	void DynamicEntitySpatialHashTable::DebugDrawCells(DebugLinePass* lines) const
	{
		const Vector3f cellColor(0.0f, 1.0f, 0.0f);
		const Vector3f entityColor(1.0f, 0.0f, 0.0f);

		for (const auto it : m_map)
		{
			Vector3i cellPos = it.first;

			Vector3f cellOrigin(float(cellPos.x), float(cellPos.y), float(cellPos.z));
			cellOrigin *= m_cellDimensions;

			Vector3f cellExtent = m_cellDimensions;

			lines->DrawCube(cellOrigin, cellExtent, cellColor);

			const Cell& cellEntities = it.second;
			for (Entity* entity : cellEntities)
			{
				const Bounds& entityBounds = entity->GetComponent<DynamicEntityTracker>()->GetEntityWorldBounds();
				lines->DrawCube(entityBounds.m_origin, entityBounds.m_extents, entityColor);
			}
		}
	}
}