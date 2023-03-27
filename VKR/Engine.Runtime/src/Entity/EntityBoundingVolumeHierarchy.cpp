#include "Entity/EntityBoundingVolumeHierarchy.h"

namespace Eng
{
	EntityBoundingVolumeHierarchy::EntityBoundingVolumeHierarchy(CLib::Allocator* allocator, const CreateDesc& desc)
		: BoundingVolumeHierarchy(allocator, desc)
	{

	}

	void EntityBoundingVolumeHierarchy::Init(CLib::Allocator* allocator, const CreateDesc& desc)
	{
		BoundingVolumeHierarchy::Init(allocator, desc);
	}

	BoundingVolumeHierarchy::NodeData* EntityBoundingVolumeHierarchy::AllocateNodeData()
	{
		return m_allocator->Alloc<NodeData>();
	}

	void EntityBoundingVolumeHierarchy::FreeNodeData(BoundingVolumeHierarchy::NodeData* data)
	{
		m_allocator->Free(reinterpret_cast<NodeData*>(data));
	};
}