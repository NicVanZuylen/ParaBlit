#include "Allocator.h"

namespace CLib
{
	uint32_t Allocator::m_segSizes[9] = { 16, 32, 64, 128, 256, 512, 1024, 2048, 4096 };

	Allocator::Allocator(uint32_t pageSize)
	{
		m_pageSize = pageSize;
		memset(m_freeLists, 0, sizeof(m_freeLists));

		// Allocate first memory page.
		AllocatePage();
	}

	Allocator::~Allocator()
	{
		// Free memory pages. Users should free any objects that are initialized by this allocator. Especially if they allocate memory on another allocator, otherwise it will be leaked.
		for (auto& page : m_pages)
		{
			_aligned_free(page.m_block);
			page.m_block = nullptr;
			page.m_allocated = 0;
		}
		m_pages.Clear();
	}

	void* Allocator::Alloc(uint32_t size, uint32_t alignment)
	{
		return InternalAlloc(size, alignment);
	}

	void Allocator::Free(void* ptr)
	{
		InternalFree(ptr);
	}

	void* Allocator::InternalAlloc(uint32_t size, uint32_t alignment)
	{
		// TODO: Consider falling back to malloc for blocks too large to fit on any page. Track if a block was allocated with malloc so we can free it with 'free'.
		uint32_t alignmentPad = alignment > 0 ? alignment - ((size + sizeof(BlockNode)) % alignment) : 0;
		uint32_t requiredSize = size + alignmentPad;

		BlockNode*& freeList = m_freeLists[GetFreeListIdx(requiredSize)];
		if (freeList)
		{
			auto* block = freeList;
			freeList = freeList->m_prevNode;
			return reinterpret_cast<void*>(reinterpret_cast<size_t>(block) + sizeof(BlockNode));
		}

		if (IsPageFull(m_pages.Back(), requiredSize)) // Allocate a new page if no untouched blocks remain.
			AllocatePage();
		auto& currentPage = m_pages.Back();

		// Allocate an new block from untouched page memory.
		BlockNode* newNode = reinterpret_cast<BlockNode*>(reinterpret_cast<size_t>(currentPage.m_block) + currentPage.m_allocated);
		newNode->m_size = requiredSize;

		currentPage.m_allocated += sizeof(BlockNode) + requiredSize;

		return reinterpret_cast<void*>(reinterpret_cast<size_t>(newNode) + sizeof(BlockNode));
	}

	void Allocator::InternalFree(void* ptr)
	{
		BlockNode* node = reinterpret_cast<BlockNode*>(reinterpret_cast<size_t>(ptr) - sizeof(BlockNode));

		// Push the block back onto the free list.
		BlockNode*& freeList = m_freeLists[GetFreeListIdx(node->m_size)];
		node->m_prevNode = freeList;
		freeList = node;
	}

	inline void Allocator::AllocatePage()
	{
		// Allocate first page.
		m_pages.PushBack({ _aligned_malloc(m_pageSize, 4) });
		m_pages.Back().m_allocated = 0;
	}

	inline uint32_t Allocator::GetFreeListIdx(const uint32_t& size)
	{
		constexpr uint32_t arraySize = _countof(m_segSizes);
		for (uint32_t i = 0; i < arraySize; ++i)
		{
			if (size < m_segSizes[i])
			{
				if (i + 1 == arraySize)
					return i - 1;
				return i; // Return the next index, as blocks in it's free list are guaranteed to be large enough.
			}
		}
		return arraySize - 1; // Use the index of the last free list if we didn't find a large enough size class.
	}
}