#include "FixedBlockAllocator.h"

namespace CLib
{
	FixedBlockAllocator::FixedBlockAllocator(uint32_t blockSize, uint32_t pageSize)
	{
		m_blockSize = blockSize;
		m_pageSize = pageSize;
		m_freeList = nullptr;

		uint32_t realBlockSize = m_blockSize + sizeof(BlockNode);
		m_blockCount = m_pageSize / realBlockSize;

		// Allocate first memory page.
		AllocatePage();
	}

	FixedBlockAllocator::~FixedBlockAllocator()
	{
		// Free memory pages. Users should free any objects that are initialized by this allocator. Especially if they allocate memory on another allocator, otherwise it will be leaked.
		for (auto& page : m_pages)
		{
			_aligned_free(page.m_block);
			page.m_block = nullptr;
			page.m_remainingBlocks = 0;
		}
		m_pages.Clear();
	}

	void* FixedBlockAllocator::Alloc()
	{
		return InternalAlloc();
	}

	void FixedBlockAllocator::Free(void* ptr)
	{
		InternalFree(ptr);
	}

	void* FixedBlockAllocator::InternalAlloc()
	{
		// Attempt to pop a block off of the free list.
		if (m_freeList != nullptr)
		{
			void* block = reinterpret_cast<void*>(m_freeList);
			m_freeList = m_freeList->m_prev;

			return reinterpret_cast<void*>(reinterpret_cast<size_t>(block) + sizeof(BlockNode));
		}

		if (m_pages.Back().m_remainingBlocks == 0) // Allocate a new page if no untouched blocks remain.
			AllocatePage();
		auto& currentPage = m_pages.Back();

		// Allocate an new block from untouched page memory.
		size_t usedBlockCount = m_blockCount - currentPage.m_remainingBlocks--;
		size_t realBlockSize = m_blockSize + sizeof(BlockNode);
		return reinterpret_cast<void*>(reinterpret_cast<size_t>(currentPage.m_block) + (usedBlockCount * realBlockSize) + sizeof(BlockNode));
	}

	void FixedBlockAllocator::InternalFree(void* ptr)
	{
		// Push the block back onto the free list.
		BlockNode* node = reinterpret_cast<BlockNode*>(reinterpret_cast<size_t>(ptr) - sizeof(BlockNode));
		node->m_prev = m_freeList;
		m_freeList = node;
	}

	inline void FixedBlockAllocator::AllocatePage()
	{
		// Allocate first page.
		m_pages.PushBack({ _aligned_malloc(m_pageSize, 16) });
		m_pages.Back().m_remainingBlocks = m_blockCount;
	}
}