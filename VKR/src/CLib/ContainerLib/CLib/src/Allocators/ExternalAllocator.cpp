#include "CLib/ExternalAllocator.h"

#include <cassert>

namespace CLib
{
	ExternalAllocator::ExternalAllocator(void* context, AddPageFunc addPageFunc, RemovePageFunc removePageFunc)
	{
		if(context && addPageFunc && removePageFunc)
			Init(nullptr, 0, context, addPageFunc, removePageFunc);
	}

	ExternalAllocator::ExternalAllocator(const uint32_t* segmentSizes, uint32_t segmentCount, void* context, AddPageFunc addPageFunc, RemovePageFunc removePageFunc)
	{
		Init(segmentSizes, segmentCount, context, addPageFunc, removePageFunc);
	}

	void ExternalAllocator::Init(const uint32_t* segmentSizes, uint32_t segmentCount, void* context, AddPageFunc addPageFunc, RemovePageFunc removePageFunc)
	{
		m_context = context;
		m_addPageFunc = addPageFunc;
		m_removePageFunc = removePageFunc;

		if(segmentSizes != nullptr && segmentCount > 0)
		{
			m_segSizes.SetCount(segmentCount);
			for (uint32_t i = 0; i < segmentCount; ++i)
				m_segSizes[i] = segmentSizes[i];

			m_freeLists.SetCount(segmentCount);
		}

		memset(m_freeLists.Data(), 0, sizeof(BlockNode*) * m_freeLists.Count());
	}

	ExternalAllocator::~ExternalAllocator()
	{
		// Free memory pages. Users should free any objects that are initialized by this allocator. Especially if they allocate memory on another allocator, otherwise it will be leaked.
		for (auto& page : m_pages)
		{
			assert(page->m_start == nullptr && "ExternalAllocator: Page was not freed due to memory leaks.");
			m_pageStorage.Free(page);
		}
		m_pages.Clear();
	}

	void* ExternalAllocator::Alloc(uint32_t size, uint32_t alignment)
	{
		return InternalAlloc(size, alignment);
	}

	void ExternalAllocator::Free(void* ptr)
	{
		InternalFree(ptr);
	}

	size_t ExternalAllocator::GetAllocationOffsetInPage(void* ptr) const
	{
		auto it = m_liveBlockMap.find(ptr);
		if (it == m_liveBlockMap.end())
			return 0;

		const Page* page = m_pages[it->second->m_pageIndex];
		return uintptr_t(ptr) - uintptr_t(page->m_start);
	}

	void* ExternalAllocator::GetAllocationPageAddress(void* ptr) const
	{
		auto it = m_liveBlockMap.find(ptr);
		if (it == m_liveBlockMap.end())
			return nullptr;

		const Page* page = m_pages[it->second->m_pageIndex];
		return page->m_start;
	}

	void ExternalAllocator::DumpMemoryLeaks()
	{

	}

	void ExternalAllocator::AddPage(uint32_t requiredSize)
	{
		if (m_freePageIndices.Count() > 0)
		{
			m_currentPageIndex = m_freePageIndices.PopBack();
		}
		else
		{
			m_pages.PushBack(m_pageStorage.Alloc<Page>());
			m_currentPageIndex = m_pages.Count() - 1;
		}
		Page& newPage = *m_pages[m_currentPageIndex];

		newPage.m_context = m_context;
		newPage.m_start = m_addPageFunc(m_context, requiredSize, newPage.m_size);
		newPage.m_allocatedBlockCount = 0;
		newPage.m_allocated = 0;
		newPage.m_freeBlockNodeCount = 0;
	}

	void* ExternalAllocator::FinalizeAllocation(BlockNode* node, uint32_t alignment)
	{
		Page& page = *m_pages[node->m_pageIndex];
		++page.m_allocatedBlockCount;

		size_t blockOffsetInPage = reinterpret_cast<size_t>(node->m_ptr) - reinterpret_cast<size_t>(page.m_start);

		// Padding to be added at the start of the block to ensure correct alignment.
		size_t alignmentPad = blockOffsetInPage % alignment;
		if (alignmentPad > 0)
			alignmentPad = alignment - alignmentPad;

		void* finalPtr = reinterpret_cast<void*>(reinterpret_cast<size_t>(node->m_ptr) + alignmentPad);
		m_liveBlockMap.insert({ finalPtr, node });
		return finalPtr;
	}

	void* ExternalAllocator::InternalAlloc(uint32_t size, uint32_t alignment, const char* typeName)
	{
		uint32_t requiredSize = size + alignment;

		auto freeListIdx = GetLargerFreeListIdx(requiredSize);
		BlockNode*& freeList = m_freeLists[freeListIdx];
		if (freeList)
		{
			if (freeListIdx == 0 || freeListIdx == m_segSizes.Count() - 1)
			{
				BlockNode** block = &freeList;

				// Blocks at these free list indices aren't guarenteed to be large enough.
				while (block && *block)
				{
					auto& blockRef = *block;

					if (blockRef->m_size >= requiredSize)
					{
						BlockNode* returnNode = blockRef;
						blockRef = blockRef->m_prevNode;

						return FinalizeAllocation(returnNode, alignment);
					}

					block = &blockRef->m_prevNode;
				}
			}
			else // Other free lists are guaranteed to have large enough blocks.
			{
				auto* block = freeList;

				if (block)
				{
					freeList = freeList->m_prevNode;
					return FinalizeAllocation(block, alignment);
				}
			}
		}

		// By this point the free lists will have been searched for free space in all pages and no appropriate blocks were found.
		if (m_pages.Count() == 0 || IsPageFull(*m_pages[m_currentPageIndex], requiredSize))
		{
			// Before adding a new page, add the remainder of the page to the free list for future allocations if it fits a minimum size requirement.
			if (m_pages.Count() > 0)
			{
				Page& currentPage = *m_pages[m_currentPageIndex];
				uint32_t remainingSpace = currentPage.m_size - currentPage.m_allocated;

				if (m_segSizes.Count() > 1 && remainingSpace >= m_segSizes[1])
				{
					BlockNode& newNode = *currentPage.m_blockNodeStorage.Alloc<BlockNode>();
					newNode.m_prevNode = nullptr;
					newNode.m_ptr = reinterpret_cast<void*>(reinterpret_cast<size_t>(currentPage.m_start) + currentPage.m_allocated);
					newNode.m_pageIndex = m_currentPageIndex;
					newNode.m_size = remainingSpace;

					BlockNode*& freeList = m_freeLists[GetSmallerFreeListIdx(remainingSpace)];
					newNode.m_prevNode = freeList;
					freeList = &newNode;

					currentPage.m_allocated = currentPage.m_size;
				}
			}

			AddPage(requiredSize);
		}

		Page& currentPage = *m_pages[m_currentPageIndex];

		// Allocate an new block from untouched page memory.
		BlockNode& newNode = *currentPage.m_blockNodeStorage.Alloc<BlockNode>();
		newNode.m_prevNode = nullptr;
		newNode.m_ptr = reinterpret_cast<void*>(reinterpret_cast<size_t>(currentPage.m_start) + currentPage.m_allocated);
		newNode.m_pageIndex = m_currentPageIndex;
		newNode.m_size = requiredSize;

		currentPage.m_allocated += requiredSize;

		return FinalizeAllocation(&newNode, alignment);
	}

	void ExternalAllocator::InternalFree(void* ptr)
	{
		auto it = m_liveBlockMap.find(ptr);
		if (it == m_liveBlockMap.end())
			return;

		BlockNode* node = it->second;

		Page& page = *m_pages[node->m_pageIndex];
		--page.m_allocatedBlockCount;

		if (page.m_allocatedBlockCount == 0)
		{
			//for (BlockNode& node : page.m_blockNodes)
			//	node.m_size = 0; // Zero size will invalidate the block.
			page.m_size = 0;
			page.m_allocated = 0;
			page.m_freeBlockNodeCount = 0;

			const uint32_t& pageIndex = node->m_pageIndex;
			if (pageIndex == m_currentPageIndex)
				m_currentPageIndex = 0; // Go back to first page for future allocations. It will most likely be already full, in which circumstance a new page will be allocated.

			// Remove free list blocks belonging to this page.
			for (auto& freeList : m_freeLists)
			{
				BlockNode** block = &freeList;
				while (*block)
				{
					BlockNode*& blockRef = *block;
					if (blockRef->m_pageIndex == pageIndex)
					{
						blockRef = blockRef->m_prevNode;
					}
					else
						block = &blockRef->m_prevNode;
				}
			}

			m_removePageFunc(m_context, page.m_start);
			page.m_start = nullptr;

			m_freePageIndices.PushBack(pageIndex);
		}
		else
		{
			// Push the block back onto the free list.
			BlockNode*& freeList = m_freeLists[GetSmallerFreeListIdx(node->m_size)];
			node->m_prevNode = freeList;
			freeList = node;
		}

		m_liveBlockMap.erase(it);
	}

	uint32_t ExternalAllocator::GetLargerFreeListIdx(const uint32_t& size)
	{
		uint32_t arraySize = m_segSizes.Count();
		uint32_t arrayEnd = arraySize - 1;
		for (uint32_t i = 0; i < arrayEnd; ++i)
		{
			if (size <= m_segSizes[i + 1])
			{
				return i == 0 ? 0 : i + 1;
			}
		}
		return arrayEnd; // Use the index of the last free list if we didn't find a large enough size class.
	}

	uint32_t ExternalAllocator::GetSmallerFreeListIdx(const uint32_t& size)
	{
		uint32_t arraySize = m_segSizes.Count();
		uint32_t arrayEnd = arraySize - 1;
		for (uint32_t i = 0; i < arrayEnd; ++i)
		{
			if (size < m_segSizes[i + 1])
			{
				return i;
			}
		}
		return arrayEnd; // Use the index of the last free list if we didn't find a large enough size class.
	}
}