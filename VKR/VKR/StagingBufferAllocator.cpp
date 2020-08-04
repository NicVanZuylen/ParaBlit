#include "StagingBufferAllocator.h"
#include "Device.h"
#include "ParaBlitDebug.h"

namespace PB
{
	u8* StagingBuffer::Map(VkDevice device)
	{
		u8* uptr;
		vkMapMemory(device, m_parentMemory, m_offset, m_size, 0, reinterpret_cast<void**>(&uptr));
		return uptr;
	}

	void StagingBuffer::Unmap(VkDevice device)
	{
		vkUnmapMemory(device, m_parentMemory);
	}

	void StagingBufferAllocator::Create(Device* device)
	{
		m_device = device;
	}

	void StagingBufferAllocator::Destroy()
	{
		for (auto& page : m_bufferPages)
		{
			vkDestroyBuffer(m_device->GetHandle(), page.m_buffer, nullptr);
			vkFreeMemory(m_device->GetHandle(), page.m_memory, nullptr);
			page.m_buffer = VK_NULL_HANDLE;
			page.m_memory = VK_NULL_HANDLE;
			page.m_size = 0;
			page.m_lastUsedFrame = 0;
		}
		m_bufferPages.Clear();
	}

	StagingBuffer StagingBufferAllocator::NewTempStagingBuffer(u32 size, u64 currentFrame)
	{
		// Attempt to sub-allocate from an existing page first before otherwise allocating a new buffer entirely.
		for (auto& page : m_bufferPages)
		{
			if (page.m_lastUsedFrame < currentFrame)
			{
				// Clear this page if there are no in-flight allocations using it.
				page.m_allocated = 0;
				// Swap this page to the beginning of the array to avoid iteration.
				std::swap<InternalBuffer>(page, m_bufferPages[0]);
			}

			if (page.m_size - page.m_allocated < size)
				continue;

			StagingBuffer view;
			view.m_parentBuffer = page.m_buffer;
			view.m_parentMemory = page.m_memory;
			view.m_offset = page.m_allocated;
			view.m_size = size;

			page.m_allocated += size;
			page.m_lastUsedFrame = currentFrame + (PB_FRAME_IN_FLIGHT_COUNT - 1); // All in-flight frames using this page need to finish before it can be cleared.
			return view;
		}

		// Out of page memory, allocate another page.
		auto newPage = AllocatePageBuffer(size);
		StagingBuffer newView;
		newView.m_parentBuffer = newPage.m_buffer;
		newView.m_parentMemory = newPage.m_memory;
		newView.m_offset = 0;
		newView.m_size = size;

		newPage.m_allocated += size;
		newPage.m_lastUsedFrame = currentFrame + (PB_FRAME_IN_FLIGHT_COUNT - 1); // All in-flight frames using this page need to finish before it can be cleared.

		return newView;
	}

	inline StagingBufferAllocator::InternalBuffer StagingBufferAllocator::AllocatePageBuffer(u32 size)
	{
		constexpr u32 pageAlignment = 64;

		InternalBuffer newBuffer;
		auto queueFamilyIndex = static_cast<u32>(m_device->GetGraphicsQueueFamilyIndex());
		u32 alignedSize = size + (size % pageAlignment);

		VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr };
		bufferInfo.flags = 0;
		bufferInfo.queueFamilyIndexCount = 1;
		bufferInfo.pQueueFamilyIndices = &queueFamilyIndex;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferInfo.size = alignedSize;
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		PB_ERROR_CHECK(vkCreateBuffer(m_device->GetHandle(), &bufferInfo, nullptr, &newBuffer.m_buffer));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(newBuffer.m_buffer);

		VkMemoryRequirements bufferMemRequirements;
		vkGetBufferMemoryRequirements(m_device->GetHandle(), newBuffer.m_buffer, &bufferMemRequirements);
		newBuffer.m_size = static_cast<u32>(bufferMemRequirements.size);

		VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr };
		allocInfo.allocationSize = bufferMemRequirements.size;
		allocInfo.memoryTypeIndex = m_device->FindMemoryTypeIndex(bufferMemRequirements.memoryTypeBits, PB_MEMORY_TYPE_HOST_VISIBLE);
		PB_ERROR_CHECK(vkAllocateMemory(m_device->GetHandle(), &allocInfo, nullptr, &newBuffer.m_memory));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(newBuffer.m_memory);

		return newBuffer;
	}
}
