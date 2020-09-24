#include "StagingBufferAllocator.h"
#include "Device.h"
#include "ParaBlitDebug.h"

namespace PB
{
	u8* TempBuffer::Map(VkDevice device)
	{
		u8* uptr;
		vkMapMemory(device, m_parentMemory, m_offset, m_size, 0, reinterpret_cast<void**>(&uptr));
		return uptr;
	}

	void TempBuffer::Unmap(VkDevice device)
	{
		vkUnmapMemory(device, m_parentMemory);
	}

	void TempBufferAllocator::Create(Device* device)
	{
		m_device = device;
	}

	void TempBufferAllocator::Destroy()
	{
		for (auto& pageArray : m_bufferPages)
		{
			for (auto& page : pageArray)
			{
				vkDestroyBuffer(m_device->GetHandle(), page.m_buffer, nullptr);
				vkFreeMemory(m_device->GetHandle(), page.m_memory, nullptr);
				page.m_buffer = VK_NULL_HANDLE;
				page.m_memory = VK_NULL_HANDLE;
				page.m_size = 0;
				page.m_lastUsedFrame = 0;
			}
			pageArray.Clear();
		}
	}

	TempBuffer TempBufferAllocator::NewTempBuffer(u32 size, u64 currentFrame, EMemoryType memoryType)
	{
		u32 memoryTypeIdx = static_cast<u32>(memoryType);

		// Attempt to sub-allocate from an existing page first before otherwise allocating a new buffer entirely.
		for (auto& page : m_bufferPages[memoryTypeIdx])
		{
			if (page.m_lastUsedFrame < currentFrame)
			{
				// Clear this page if there are no in-flight allocations using it.
				page.m_allocated = 0;
				// Swap this page to the beginning of the array to avoid iteration.
				std::swap<InternalBuffer>(page, m_bufferPages[memoryTypeIdx][0]);
			}

			if (page.m_size - page.m_allocated < size)
				continue;

			TempBuffer view;
			view.m_parentBuffer = page.m_buffer;
			view.m_parentMemory = page.m_memory;
			view.m_offset = page.m_allocated;
			view.m_size = size;

			page.m_allocated += size;
			page.m_lastUsedFrame = currentFrame + (PB_FRAME_IN_FLIGHT_COUNT - 1); // All in-flight frames using this page need to finish before it can be cleared.
			return view;
		}

		// Out of page memory, allocate another page.
		AllocatePageBuffer(memoryType, size);
		auto& newPage = m_bufferPages[memoryTypeIdx].Back();
		TempBuffer newView;
		newView.m_parentBuffer = newPage.m_buffer;
		newView.m_parentMemory = newPage.m_memory;
		newView.m_offset = 0;
		newView.m_size = size;

		newPage.m_allocated += size;
		newPage.m_lastUsedFrame = currentFrame + (PB_FRAME_IN_FLIGHT_COUNT - 1); // All in-flight frames using this page need to finish before it can be cleared.

		return newView;
	}

	inline void TempBufferAllocator::AllocatePageBuffer(EMemoryType memoryType, u32 desiredSize)
	{
		constexpr u32 pageAlignment = 64;
		constexpr u32 pageSize = 512;

		u32 requiredSize = pageSize >= desiredSize ? pageSize : desiredSize;

		InternalBuffer& newPage = m_bufferPages[static_cast<u32>(memoryType)].PushBack();
		auto queueFamilyIndex = static_cast<u32>(m_device->GetGraphicsQueueFamilyIndex());

		VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr };
		bufferInfo.flags = 0;
		bufferInfo.queueFamilyIndexCount = 1;
		bufferInfo.pQueueFamilyIndices = &queueFamilyIndex;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferInfo.size = requiredSize;
		switch (memoryType)
		{
		case EMemoryType::HOST_VISIBLE:
			bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			break;
		case EMemoryType::DEVICE_LOCAL:
			bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			break;
		default:
			PB_NOT_IMPLEMENTED;
			break;
		}

		PB_ERROR_CHECK(vkCreateBuffer(m_device->GetHandle(), &bufferInfo, nullptr, &newPage.m_buffer));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(newPage.m_buffer);

		VkMemoryRequirements bufferMemRequirements;
		vkGetBufferMemoryRequirements(m_device->GetHandle(), newPage.m_buffer, &bufferMemRequirements);
		PB_ASSERT(bufferMemRequirements.size >= requiredSize);
		newPage.m_size = desiredSize;

		VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr };
		allocInfo.allocationSize = bufferMemRequirements.size;
		allocInfo.memoryTypeIndex = m_device->FindMemoryTypeIndex(bufferMemRequirements.memoryTypeBits, memoryType);
		PB_ERROR_CHECK(vkAllocateMemory(m_device->GetHandle(), &allocInfo, nullptr, &newPage.m_memory));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(newPage.m_memory);

		PB_ERROR_CHECK(vkBindBufferMemory(m_device->GetHandle(), newPage.m_buffer, newPage.m_memory, 0));
		PB_BREAK_ON_ERROR;
	}
}
