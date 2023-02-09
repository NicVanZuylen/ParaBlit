#pragma once
#include "Entity/Entity.h"
#include "Utility/BoundingVolumeHierarchy.h"

namespace Eng
{
	class EntityBoundingVolumeHierarchy : public BoundingVolumeHierarchy
	{
	public:

		struct ObjectData : public BoundingVolumeHierarchy::ObjectData
		{
			Eng::Entity* m_entity = nullptr;
		};

		EntityBoundingVolumeHierarchy() = default;
		EntityBoundingVolumeHierarchy(CLib::Allocator* allocator, const CreateDesc& desc);
		~EntityBoundingVolumeHierarchy() = default;

		void Init(CLib::Allocator* allocator, const CreateDesc& desc);

	private:

		struct NodeData
		{

		};

		BoundingVolumeHierarchy::NodeData* AllocateNodeData() override;

		void FreeNodeData(BoundingVolumeHierarchy::NodeData* data) override;
	};
}