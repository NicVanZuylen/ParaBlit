#include "ResourcePool.h"
#include "Device.h"
#include "BufferObject.h"

namespace PB
{
	void ResourcePool::Create(Device* device, const ResourcePoolDesc& desc)
	{
		m_device = device;
		m_memoryType = desc.m_memoryType;

		m_device->GetBufferAllocator(desc.m_memoryType).Alloc(desc.m_size, 0, m_poolAllocation);
	}

	ResourcePool::~ResourcePool()
	{
		if (m_poolAllocation.m_memoryHandle)
		{
			m_device->GetBufferAllocator(m_memoryType).Free(m_poolAllocation);
			m_poolAllocation.m_memoryHandle = VK_NULL_HANDLE;
		}
	}

	void ResourcePool::PlaceBuffer(IBufferObject* buffer, u32 offset)
	{
		PB_ASSERT(buffer != nullptr);

		BufferObject* internalBuffer = reinterpret_cast<BufferObject*>(buffer);
		VkBuffer bufferHandle = internalBuffer->GetHandle();

		// Ensure resource pool's memory is suitable for the buffer object to use.
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(m_device->GetHandle(), bufferHandle, &memRequirements);
		PB_ASSERT_MSG(m_device->FindMemoryTypeIndex(memRequirements.memoryTypeBits, m_memoryType) > 0, "Memory of ResourcePool not suitable for provided buffer object.");

		// Bind region of this pool's memory to the buffer.
		vkBindBufferMemory(m_device->GetHandle(), bufferHandle, m_poolAllocation.m_memoryHandle, m_poolAllocation.m_offset + offset);
	}

	void ResourcePool::PlaceTexture(ITexture* texture, u32 offset)
	{
		PB_NOT_IMPLEMENTED;
	}
}