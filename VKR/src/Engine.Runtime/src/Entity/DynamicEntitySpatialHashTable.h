#pragma once
#include "CLib/Allocator.h"
#include "Entity/Component/DynamicEntityTracker.h"
#include "Engine.Math/Scalar.h"
#include "Engine.Math/Vector3.h"

#include <unordered_map>

namespace Eng
{
	class DynamicEntitySpatialHashTable
	{
	public:

		DynamicEntitySpatialHashTable() = default;
		~DynamicEntitySpatialHashTable();

		void Init(CLib::Allocator* allocator);
		void Destroy();

		void Reset();

		inline Vector3i ToCellPosition(const Vector3f& position)
		{
			return Vector3i
			(
				Math::FloorToInt(position.x / m_cellDimensions),
				Math::FloorToInt(position.y / m_cellDimensions),
				Math::FloorToInt(position.z / m_cellDimensions)
			);
		}

		inline void Insert(Entity* entity, const DynamicEntityTracker* tracker)
		{
			const Bounds& bounds = tracker->GetEntityWorldBounds();

			Vector3i originCellPos = ToCellPosition(bounds.m_origin);
			Vector3i extentCellpos = ToCellPosition(bounds.m_origin + bounds.m_extents);

			Vector3i iPos;
			for (iPos.x = originCellPos.x; iPos.x <= extentCellpos.x; ++iPos.x)
			{
				for (iPos.y = originCellPos.y; iPos.y <= extentCellpos.y; ++iPos.y)
				{
					for (iPos.z = originCellPos.z; iPos.z <= extentCellpos.z; ++iPos.z)
					{
						m_map[iPos].PushBack(entity);
					}
				}
			}

			m_averageMaxDimension += Math::Max(Math::Max(bounds.m_extents.x, bounds.m_extents.y), bounds.m_extents.z);
			++m_entityCount;
		}

		Entity* RaycastGetEntity(const Vector3f& rayOrigin, const Vector3f& rayDirection, float rayRange = 10.0f);

		void DebugDrawCells(class DebugLinePass* lines) const;

	private:

		struct CellHasher
		{
			inline size_t operator()(const Vector3i& cellPos) const
			{
				size_t seed = cellPos.x * (2 * cellPos.y) * (4 * cellPos.z);

				for (auto x : cellPos.data) {
					x = ((x >> 16) ^ x) * 0x45d9f3b;
					x = ((x >> 16) ^ x) * 0x45d9f3b;
					x = (x >> 16) ^ x;
					seed ^= x + 0x9e3779b9 + (seed << 6) + (seed >> 2);
				}
				return seed;
			}
		};

		CLib::Allocator* m_allocator = nullptr;

		static constexpr uint32_t CellExpectedEntityLimit = 4;
		static constexpr uint32_t CellExpandRate = 16;

		using Cell = CLib::Vector<Entity*, CellExpectedEntityLimit, CellExpandRate>;

		float m_cellDimensions = 10.0f;
		float m_averageMaxDimension = 1.0f;
		uint32_t m_entityCount = 0;
		std::unordered_map<Math::Vector3i, Cell, CellHasher> m_map;
	};
}