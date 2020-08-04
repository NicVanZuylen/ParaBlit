#pragma once
#include "ParaBlitApi.h"
#include "CLib/Vector.h"
#include "vulkan/vulkan.h"
#include "ParaBlitDefs.h"

#include <map>
#include <mutex>

namespace PB
{
	class Device;

	class DeviceAllocator
	{
	public:

		PARABLIT_API DeviceAllocator();

		PARABLIT_API ~DeviceAllocator();

		PARABLIT_API void Init(Device* device);

		PARABLIT_API void Destroy();

		// Represents a aligned view of a portion/slice of a page's memory.
		// This will be returned to the user for usage as they see fit.
		struct PageView
		{
			VkDeviceMemory m_memory = VK_NULL_HANDLE;
			u32 m_start = 0;
			u32 m_size = 0;
			EMemoryType m_memoryType = PB_MEMORY_TYPE_END_RANGE;
			u16 m_alignmentOffset = 0;

			inline VkDeviceSize AlignedOffset()
			{
				return static_cast<VkDeviceSize>(m_start) + m_alignmentOffset;
			}
		};

		PARABLIT_API PageView Alloc(const VkMemoryRequirements& requirements, const EMemoryType& memType);

		PARABLIT_API void Free(PageView& pageView);

	private:

		PARABLIT_API void Reset();

		struct Page
		{
			VkDeviceMemory m_memory = VK_NULL_HANDLE;
			u32 m_size = 0;
		};

		Device* m_device = nullptr;
		std::multimap<u32, PageView> m_availableBlocks[PB_MEMORY_TYPE_END_RANGE];
		CLib::Vector<Page, 64, 64> m_pages;
		std::mutex m_allocatorLock;
	};
}