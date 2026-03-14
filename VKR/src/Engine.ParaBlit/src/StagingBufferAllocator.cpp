#include "StagingBufferAllocator.h"
#include "Device.h"
#include "ParaBlitDebug.h"

namespace PB
{
	void TempBufferAllocator::ResetFrame(u32 frameIndex)
	{
		std::lock_guard lock(m_mutex);
		for (u32 i = 0; i < MemoryTypeCount; ++i)
		{
			m_freeLists[i].AppendList(m_pendingFreeLists[frameIndex][i]);
			m_pendingFreeLists[frameIndex][i].AppendList(m_frameLists[frameIndex][i]);
		}
	}

	void TempBufferAllocator::Create(Device* device)
	{
		m_device = device;
	}

	void TempBufferAllocator::Destroy()
	{
		VkDevice deviceHandle = m_device->GetHandle();

		// Free all frame pages.
		for (u32 i = 0; i < MemoryTypeCount; ++i)
		{
			for (u32 j = 0; j < PB_FRAME_IN_FLIGHT_COUNT + 1; ++j)
			{
				m_freeLists[i].AppendList(m_pendingFreeLists[j][i]);
				m_freeLists[i].AppendList(m_frameLists[j][i]);
			}

			PageListNode* currentNode = m_freeLists[i].m_start;
			while (currentNode)
			{
				PageListNode* nodeToFree = currentNode;
				currentNode = currentNode->m_next;
				nodeToFree->Unlink();

				vkDestroyBuffer(deviceHandle, nodeToFree->m_buffer, nullptr);
				vkFreeMemory(deviceHandle, nodeToFree->m_memory, nullptr);
				m_pageAllocator.Free(nodeToFree);
			}
		}
	}

	TempBuffer TempBufferAllocator::NewTempBuffer(u32 size, u32 frameIndex, EMemoryType memoryType, u32 alignment)
	{
		std::lock_guard lock(m_mutex);

		u32 memoryTypeIdx = static_cast<u32>(memoryType);
		u32 requiredSize = size + alignment;
		PageList& frameList = m_frameLists[frameIndex][memoryTypeIdx];

		// Check if the last node owned by this frame has enough space...
		PageListNode* lastOwnedNode = frameList.m_end;
		if (lastOwnedNode && lastOwnedNode->CanFit(requiredSize))
		{
			TempBuffer returnView;
			lastOwnedNode->BuildView(returnView, requiredSize, alignment);
			return returnView;
		}

		// Otherwise look at the free list.
		if (!m_freeLists[memoryTypeIdx].IsEmpty())
		{
			// Search for large enough node in the free list.
			PageListNode* currentNode = m_freeLists[memoryTypeIdx].m_end;
			while (currentNode)
			{
				// Check for size and disregard allocated bytes, since this memory won't be in use by anything.
				if (currentNode->m_size < requiredSize)
				{
					currentNode = currentNode->m_prev;
					continue;
				}

				// The current node's allocated byte count won't have been reset when added to the free list, so we do it here.
				currentNode->m_bytesAllocated = 0;

				m_freeLists[memoryTypeIdx].UnlinkNode(currentNode);
				frameList.AppendNode(currentNode);

				TempBuffer returnBuffer;
				currentNode->BuildView(returnBuffer, requiredSize, alignment);
				return returnBuffer;
			}
		}

		// Allocate a new page node, since no others fit the requested size.
		PageListNode* newNode = AllocatePageBuffer(memoryType, requiredSize);
		frameList.AppendNode(newNode);
		if (requiredSize > MinPageSize && frameList.m_start != frameList.m_end)
		{
			PB_LOG_FORMAT("Warning: Allocating massive temporary buffer of size: %u that exceeds the page size of %u. It is recommended that the maximum staged bytes per frame should be limited to a low multiple of %u.", size, MinPageSize, MinPageSize);

			// The new page is guaranteed to be full, so we'll add this node to the frame list behind the end node, to allow further suballocations from the end node.
			frameList.SwapNodes(newNode, newNode->m_prev);
			newNode = frameList.m_end->m_prev;
		}

		TempBuffer returnBuffer;
		newNode->BuildView(returnBuffer, requiredSize, alignment);
		return returnBuffer;
	}

	inline TempBufferAllocator::PageListNode* TempBufferAllocator::AllocatePageBuffer(EMemoryType memoryType, u32 desiredSize)
	{
		u32 requiredSize = MinPageSize >= desiredSize ? MinPageSize : desiredSize;

		PageListNode& newPage = *m_pageAllocator.Alloc<PageListNode>();
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
		newPage.m_size = requiredSize;

		VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr };
		allocInfo.allocationSize = bufferMemRequirements.size;
		allocInfo.memoryTypeIndex = m_device->FindMemoryTypeIndex(bufferMemRequirements.memoryTypeBits, memoryType);
		PB_ERROR_CHECK(vkAllocateMemory(m_device->GetHandle(), &allocInfo, nullptr, &newPage.m_memory));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(newPage.m_memory);

		PB_ERROR_CHECK(vkBindBufferMemory(m_device->GetHandle(), newPage.m_buffer, newPage.m_memory, 0));
		PB_BREAK_ON_ERROR;

		PB_ERROR_CHECK(vkMapMemory(m_device->GetHandle(), newPage.m_memory, 0, desiredSize, 0, reinterpret_cast<void**>(&newPage.m_mappedMemory)));
		PB_BREAK_ON_ERROR;

		return &newPage;
	}

	void TempBufferAllocator::PageList::AppendNode(TempBufferAllocator::PageListNode* node)
	{
		node->Unlink();

		PB_ASSERT(m_start != nullptr || m_end == nullptr);
		PB_ASSERT(m_end == nullptr || m_end->m_next == nullptr);
		if (!m_start)
		{
			m_start = node;
			m_end = node;
			return;
		}

		// Link end node and input node.
		m_end->m_next = node;
		node->m_prev = m_end;

		// Update end node.
		m_end = node;
	}

	void TempBufferAllocator::PageList::AppendList(PageList& list)
	{
		PB_ASSERT(m_start != nullptr || m_end == nullptr);

		if (list.IsEmpty())
			return;

		PB_ASSERT(list.m_end->m_next == nullptr);

		if (!m_start)
		{
			m_start = list.m_start;
			m_end = list.m_end;

			// Invalidate input list. It now longer owns it's nodes.
			list.Invalidate();
			return;
		}

		// Link end of this list with the start of the input list.
		m_end->m_next = list.m_start;
		list.m_start->m_prev = m_end;

		// Update this list's end node.
		m_end = list.m_end;

		// Invalidate input list. It now longer owns it's nodes.
		list.Invalidate();
	}

	void TempBufferAllocator::PageList::SwapNodes(PageListNode* first, PageListNode* second)
	{
		if (first == nullptr || second == nullptr)
			return;

		PageListNode cacheFirst = *first;
		PageListNode* firstNext = first->m_next;
		PageListNode* secondNext = second->m_next;
		PageListNode* firstPrev = first->m_prev;
		PageListNode* secondPrev = second->m_prev;

		*first = *second;
		*second = cacheFirst;

		first->m_next = firstNext;
		first->m_prev = firstPrev;

		second->m_next = secondNext;
		second->m_prev = secondPrev;
	}

	void TempBufferAllocator::PageList::UnlinkNode(PageListNode* node)
	{
		if (m_start == node)
		{
			Invalidate();
			node->Unlink();
			return;
		}

		node->m_prev->m_next = node->m_next;
		if (node == m_end)
			m_end = m_end->m_prev;

		node->Unlink();
	}

	TempBufferAllocator::PageListNode* TempBufferAllocator::PageList::UnlinkEnd()
	{
		PageListNode* returnNode = m_end;
		m_end = m_end->m_prev;
		m_end->m_next = nullptr;

		returnNode->Unlink();
		return returnNode;
	}

	void TempBufferAllocator::PageListNode::Unlink()
	{
		m_prev = nullptr;
		m_next = nullptr;
	}

	bool TempBufferAllocator::PageListNode::CanFit(u32 size)
	{
		return m_size - m_bytesAllocated >= size;
	}

	void TempBufferAllocator::PageListNode::BuildView(TempBuffer& view, u32 size, u32 alignment)
	{
		view.m_buffer = m_buffer;
		view.m_mappedPtr = m_mappedMemory;

		u32 alignPad = alignment > 0 ? alignment - (m_bytesAllocated % alignment) : 0;
		view.m_offset = m_bytesAllocated + alignPad;
		view.m_alignment = alignment;

		m_bytesAllocated += size;
		view.m_size = size;
	}
}
