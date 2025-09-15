#pragma once
#include "IResourcePool.h"
#include "ParaBlitApi.h"
#include "ParaBlitDebug.h"
#include "PoolAllocator.h"

namespace PB
{
	class Device;

	class ResourcePool : public IResourcePool
	{
	public:

		ResourcePool() = default;

		~ResourcePool();

		void Create(Device* device, const ResourcePoolDesc& desc);

		void PlaceBuffer(IBufferObject* buffer, u32 offset) override;

		void PlaceTexture(ITexture* texture, u32 offset) override;

	private:

		Device* m_device = nullptr;
		PoolAllocator::PoolAllocation m_poolAllocation{};
		PB::EMemoryType m_memoryType = PB::EMemoryType::END_RANGE;
	};
}
