#include "Entity/EntityBoundingVolumeHierarchy.h"

namespace Eng
{
	EntityBoundingVolumeHierarchy::EntityBoundingVolumeHierarchy(const CreateDesc& desc)
		: BoundingVolumeHierarchy(desc)
	{

	}

	void EntityBoundingVolumeHierarchy::Init(const CreateDesc& desc)
	{
		BoundingVolumeHierarchy::Init(desc);
	}

	void EntityBoundingVolumeHierarchy::AllocateNodeData(BuildNode* node)
	{
		new (node + 1) NodeData;
	}

	void EntityBoundingVolumeHierarchy::FreeNodeData(BuildNode* node)
	{
		reinterpret_cast<NodeData*>(node + 1)->~NodeData();
	};
}