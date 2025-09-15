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
			Entity* m_entity = nullptr;
		};

		EntityBoundingVolumeHierarchy() = default;
		EntityBoundingVolumeHierarchy(const CreateDesc& desc);
		~EntityBoundingVolumeHierarchy() = default;

		void Init(const CreateDesc& desc);

	private:

		struct NodeData : public BoundingVolumeHierarchy::NodeData
		{

		};

		void AllocateNodeData(BuildNode* node) override;

		void FreeNodeData(BuildNode* node) override;
	};
}