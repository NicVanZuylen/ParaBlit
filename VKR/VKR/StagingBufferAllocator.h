#pragma once
#include "ParaBlitDefs.h"
#include "DynamicArray.h"

#include "vulkan/vulkan.h"

namespace PB
{
	class Device;

	struct StagingBuffer
	{
		u8* Map(VkDevice device);
		void Unmap(VkDevice device);

		VkBuffer m_parentBuffer;
		VkDeviceMemory m_parentMemory;
		u32 m_offset;
		u32 m_size;
	};

	class StagingBufferAllocator
	{
	public:

		void Create(Device* device);

		void Destroy();

		// Allocate memory for staging purposes, sub-allocate if possible. Lifetime is for as long as the buffer's frame it was allocated for is in flight.
		StagingBuffer NewTempStagingBuffer(u32 size, u64 currentFrame);

	private:

		struct InternalBuffer
		{
			VkBuffer m_buffer = VK_NULL_HANDLE;
			VkDeviceMemory m_memory = VK_NULL_HANDLE;
			u32 m_size = 0;
			u32 m_allocated = 0;
			u64 m_lastUsedFrame = 0;
		};

		inline InternalBuffer AllocatePageBuffer(u32 size);

		Device* m_device = nullptr;
		DynamicArray<InternalBuffer, 32> m_bufferPages;
	};
}