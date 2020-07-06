#include "DeviceAllocator.h"
#include "ParaBlitDebug.h"
#include "Device.h"

namespace PB
{
	DeviceAllocator::DeviceAllocator()
	{
	}

	DeviceAllocator::~DeviceAllocator()
	{
		
	}

	void DeviceAllocator::Init(Device* device)
	{
		m_device = device;
	}

	void DeviceAllocator::Destroy()
	{
		Reset();
	}

	DeviceAllocator::PageView DeviceAllocator::Alloc(const VkMemoryRequirements& requirements, const EMemoryType& memType)
	{
		std::lock_guard<std::mutex> lock(m_allocatorLock);

		PB_ASSERT(requirements.size + requirements.alignment <= ~(0U), "Requested memory block is too large.");
		u32 requiredSize = static_cast<u32>(requirements.size + requirements.alignment);
		// TODO: Ensure alignment requirements are met when setting the start parameter of page views.
		auto sliceIt = m_availableBlocks[memType].lower_bound(requiredSize);
		if (sliceIt != m_availableBlocks[memType].end())
		{
			auto sliceNode = m_availableBlocks[memType].extract(sliceIt);
			auto& slice = sliceNode.mapped();
			
			// If this if statement is not entered we have allocated a perfect slice.
			if (slice.m_size > requiredSize)
			{
				// Split the slice into another which can be allocated separately.
				std::pair<u32, PageView> pair(requiredSize, {});
				PageView& newSlice = pair.second;
				newSlice.m_memory = slice.m_memory;
				newSlice.m_memoryType = slice.m_memoryType;
				newSlice.m_start = slice.m_start + requiredSize;
				newSlice.m_size = slice.m_size - requiredSize;
				slice.m_size = requiredSize;
				m_availableBlocks[memType].insert(pair);
			}
			
			// Calculate offset from start to meet memory alignment requirements.
			slice.m_alignmentOffset = requirements.alignment + (slice.m_start % requirements.alignment);

			return slice;
		}

		// TODO: Find reasonable sizes for pages to be allocated with rather than just "what is large enough".
		// Allocate a new page.
		auto& newPage = m_pages.PushBack();
		newPage.m_size = requiredSize;

		VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr };
		allocInfo.allocationSize = requiredSize;
		allocInfo.memoryTypeIndex = m_device->FindMemoryTypeIndex(requirements.memoryTypeBits, memType);
		
		PB_ERROR_CHECK(vkAllocateMemory(m_device->GetHandle(), &allocInfo, nullptr, &newPage.m_memory));
		PB_ASSERT(newPage.m_memory);

		// Return a new view of the entire page for use.
		PageView view;
		view.m_memory = newPage.m_memory;
		view.m_start = 0;
		view.m_size = requiredSize;
		view.m_memoryType = memType;
		view.m_alignmentOffset = 0;
		return view;
	}

	void DeviceAllocator::Free(PageView& pageView)
	{
		std::lock_guard<std::mutex> lock(m_allocatorLock);

		// Insert the slice back into the memory type's associate map.
		PageView newSlice = pageView;
		std::pair<u32, PageView> newPair(newSlice.m_size, newSlice);
		m_availableBlocks[newSlice.m_memoryType].insert(newPair);

		// Invalidate the provided page view.
		pageView.m_memory = VK_NULL_HANDLE;
		pageView.m_memoryType = PB_MEMORY_TYPE_END_RANGE;
		pageView.m_start = 0;
		pageView.m_size = 0;
	}

	void DeviceAllocator::Reset()
	{
		m_availableBlocks->clear();
		for (auto& page : m_pages)
		{
			vkFreeMemory(m_device->GetHandle(), page.m_memory, nullptr);
		}
		m_pages.Clear();
	}
}