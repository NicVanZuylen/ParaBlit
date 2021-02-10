#pragma once
#include "ParaBlitDefs.h"
#include "ParaBlitDebug.h"
#include "CLib/Vector.h"

namespace PB
{
	class Device;

	struct TempBuffer
	{
		u8* Map(VkDevice device, u32 mapOffset = 0);
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
		TempBuffer NewTempBuffer(u32 size, u64 currentFrame, EMemoryType memoryType = EMemoryType::HOST_VISIBLE);

	private:

		struct InternalBuffer
		{
			VkBuffer m_buffer = VK_NULL_HANDLE;
			VkDeviceMemory m_memory = VK_NULL_HANDLE;
			u32 m_size = 0;
			u32 m_allocated = 0;
			u64 m_lastUsedFrame = 0;
		};

		inline void AllocatePageBuffer(EMemoryType memoryType, u32 desiredSize);

		Device* m_device = nullptr;
		CLib::Vector<InternalBuffer, 32> m_bufferPages[static_cast<u32>(EMemoryType::END_RANGE)];
	};
}