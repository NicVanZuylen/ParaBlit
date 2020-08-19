#pragma once
#include "ParaBlitDefs.h"
#include "CLib/Vector.h"

#include "vulkan/vulkan.h"

namespace PB
{
	class Device;

	struct TempBuffer
	{
		u8* Map(VkDevice device);
		void Unmap(VkDevice device);

		VkBuffer m_parentBuffer;
		VkDeviceMemory m_parentMemory;
		u32 m_offset;
		u32 m_size;
	};

	class TempBufferAllocator
	{
	public:

		void Create(Device* device);

		void Destroy();

		// Allocate temporary buffer. Lifetime is for as long as the buffer's frame it was allocated for is in flight.
		TempBuffer NewTempBuffer(u32 size, u64 currentFrame, EMemoryType memoryType = PB_MEMORY_TYPE_HOST_VISIBLE);

	private:

		struct InternalBuffer
		{
			VkBuffer m_buffer = VK_NULL_HANDLE;
			VkDeviceMemory m_memory = VK_NULL_HANDLE;
			u32 m_size = 0;
			u32 m_allocated = 0;
			u64 m_lastUsedFrame = 0;
		};

		inline InternalBuffer AllocatePageBuffer(EMemoryType memoryType);

		Device* m_device = nullptr;
		CLib::Vector<InternalBuffer, 32> m_bufferPages[PB_MEMORY_TYPE_END_RANGE];
	};
}