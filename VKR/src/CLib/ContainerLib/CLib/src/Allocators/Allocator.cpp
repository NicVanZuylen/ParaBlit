#include "Clib/Allocator.h"
#include <cassert>

#if CLIB_ALLOCATOR_DEBUG
#define DEBUG_STORE_TYPE_NAME(blockNode, name) blockNode->m_typeName = name
#define DEBUG_TRACK_BLOCK(blockNode) m_allocatedNodes.insert(blockNode)
#define DEBUG_UNTRACK_BLOCK(blockNode) m_allocatedNodes.erase(blockNode)
#else
#define DEBUG_STORE_TYPE_NAME(blockNode, name)
#define DEBUG_TRACK_BLOCK(blockNode)
#define DEBUG_UNTRACK_BLOCK(blockNode)
#endif

// Useful for diagnosing if there is an issue with allocation logic.
// If enabling this fixes an issue under investigation, then the problem is most likely within this logic.

#if CLIB_ALLOCATOR_DEBUG
#define CLIB_ALLOCATOR_USE_MALLOC false
#else
#define CLIB_ALLOCATOR_USE_MALLOC true // Use malloc on release builds to help diagnose heap corruption issues which debug builds do not report.
#endif

namespace CLib
{
	uint32_t Allocator::m_segSizes[10] = { 0, 32, 64, 128, 256, 512, 1024, 2048, 4096, 0 };

	Allocator::Allocator(uint32_t pageSize, bool threadSafe)
	{
		m_pageSize = pageSize;
		m_threadSafe = threadSafe;
		memset(m_freeLists, 0, sizeof(m_freeLists));

		// Allocate first memory page.
		AllocatePage();
	}

	Allocator::~Allocator()
	{
#if CLIB_ALLOCATOR_DUMP_ON_DESTRUCTION
		DumpMemoryLeaks();
#endif

		// Free memory pages. Users should free any objects that are initialized by this allocator. Especially if they allocate memory on another allocator, otherwise it will be leaked.
		for (auto& page : m_pages)
		{
			free(page.m_block);
			page.m_block = nullptr;
			page.m_allocated = 0;
		}
		m_pages.Clear();
	}

	void* Allocator::Alloc(uint32_t size, uint32_t alignment)
	{
		if (m_threadSafe)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return InternalAlloc(size, alignment);
		}
		return InternalAlloc(size, alignment);
	}

	void Allocator::Free(void* ptr)
	{
		if (m_threadSafe)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			InternalFree(ptr);
			return;
		}
		InternalFree(ptr);
	}

	const char* Allocator::GetBlockName(void* ptr)
	{
#if CLIB_ALLOCATOR_DEBUG
		BlockNode* node = reinterpret_cast<BlockNode*>(reinterpret_cast<size_t>(ptr) - sizeof(BlockNode));
		return node->m_typeName;
#else
		return "void*";
#endif
	}

	void Allocator::DumpMemoryLeaks()
	{
#if CLIB_ALLOCATOR_DEBUG
		if (m_allocatedNodes.empty())
			return;

		printf("---------------------------------------------------------------------------------------\n");
		printf("CLib::Allocator: Detected memory leaks! Beginning dump...\n");
		printf("---------------------------------------------------------------------------------------\n");

		for (auto& node : m_allocatedNodes)
			printf("Block: %p - Size: %u - Type Name: %s\n", reinterpret_cast<void*>(reinterpret_cast<uint64_t>(node) + sizeof(BlockNode)), node->m_size, node->m_typeName);

		printf("---------------------------------------------------------------------------------------\n");
		printf("CLib::Allocator: End dump.\n");

		assert(false && "CLib::Allocator detected memory leaks. Check console output for leak dump.");
#endif
	}

	void* Allocator::InternalAlloc(uint32_t size, uint32_t alignment, const char* typeName)
	{
		if constexpr (CLIB_ALLOCATOR_USE_MALLOC)
		{
			return malloc(size);
		}

		// TODO: Consider falling back to malloc for blocks too large to fit on any page. Track if a block was allocated with malloc so we can free it with 'free'.
		uint32_t alignmentPad = alignment > 0 ? alignment - ((size + sizeof(BlockNode)) % alignment) : 0;
		uint32_t requiredSize = size + alignmentPad;

		auto freeListIdx = GetUpperFreeListIdx(requiredSize);
		BlockNode*& freeList = m_freeLists[freeListIdx];
		if (freeList)
		{
			if (freeListIdx == 0 || freeListIdx == _countof(m_segSizes) - 1)
			{
				BlockNode** block = &freeList;

				// Blocks at these free list indices aren't guarenteed to be large enough.
				while (*block)
				{
					BlockNode*& blockRef = *block;
					if (blockRef->m_size >= requiredSize)
					{
						BlockNode* returnNode = blockRef;
						blockRef = blockRef->m_prevNode;

						DEBUG_STORE_TYPE_NAME(returnNode, typeName);
						DEBUG_TRACK_BLOCK(returnNode);

						return reinterpret_cast<void*>(reinterpret_cast<size_t>(returnNode) + sizeof(BlockNode));
					}

					freeList = blockRef->m_prevNode;
				}
			}
			else // Other free lists are guaranteed to have large enough blocks.
			{
				auto* block = freeList;
				freeList = freeList->m_prevNode;
				DEBUG_STORE_TYPE_NAME(block, typeName);
				DEBUG_TRACK_BLOCK(block);
				return reinterpret_cast<void*>(reinterpret_cast<size_t>(block) + sizeof(BlockNode));
			}
		}

		uint32_t sizeIncludingBlockNode = sizeof(BlockNode) + requiredSize;
		if (IsPageFull(m_pages.Back(), sizeIncludingBlockNode)) // Allocate a new page if no untouched blocks remain.
			AllocatePage();
		auto& currentPage = m_pages.Back();

		// Allocate an new block from untouched page memory.
		BlockNode* newNode = reinterpret_cast<BlockNode*>(reinterpret_cast<size_t>(currentPage.m_block) + currentPage.m_allocated);
		newNode->m_size = requiredSize;
		DEBUG_STORE_TYPE_NAME(newNode, typeName);
		DEBUG_TRACK_BLOCK(newNode);

		currentPage.m_allocated += sizeIncludingBlockNode;

		return reinterpret_cast<void*>(reinterpret_cast<size_t>(newNode) + sizeof(BlockNode));
	}

	void Allocator::InternalFree(void* ptr)
	{
		if constexpr (CLIB_ALLOCATOR_USE_MALLOC)
		{
			free(ptr);
			return;
		}

		BlockNode* node = reinterpret_cast<BlockNode*>(reinterpret_cast<size_t>(ptr) - sizeof(BlockNode));
		DEBUG_UNTRACK_BLOCK(node);

		// Push the block back onto the free list.
		BlockNode*& freeList = m_freeLists[GetLowerFreeListIdx(node->m_size)];
		node->m_prevNode = freeList;
		freeList = node;
	}

	void Allocator::AllocatePage()
	{
		// Allocate first page.
		m_pages.PushBack({ malloc(m_pageSize)});
		m_pages.Back().m_allocated = 0;
	}

	uint32_t Allocator::GetUpperFreeListIdx(const uint32_t& size)
	{
		constexpr uint32_t arraySize = _countof(m_segSizes);
		constexpr uint32_t arrayEnd = arraySize - 1;
		for (uint32_t i = 0; i < arrayEnd; ++i)
		{
			if (size <= m_segSizes[i + 1])
			{
				return i == 0 ? 0 : i + 1;
			}
		}
		return arrayEnd; // Use the index of the last free list if we didn't find a large enough size class.
	}

	uint32_t Allocator::GetLowerFreeListIdx(const uint32_t& size)
	{
		constexpr uint32_t arraySize = _countof(m_segSizes);
		constexpr uint32_t arrayEnd = arraySize - 1;
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