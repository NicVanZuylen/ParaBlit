#pragma once
#include "Clib/FixedBlockAllocator.h"

#include <unordered_map>
#include <functional>

namespace CLib
{
	class ExternalAllocator
	{
	public:

		using AddPageFunc = std::function<void*(void* context, uint32_t requestedMinSize, uint32_t& outSize)>;
		using RemovePageFunc = std::function<void(void* context, void* ptr)>;

		CLIB_API ExternalAllocator(void* context = nullptr, AddPageFunc addPageFunc = nullptr, RemovePageFunc removePageFunc = nullptr);

		CLIB_API ExternalAllocator(const uint32_t* segmentSizes, uint32_t segmentCount, void* context, AddPageFunc addPageFunc, RemovePageFunc removePageFunc);

		CLIB_API ~ExternalAllocator();

		CLIB_API void Init(const uint32_t* segmentSizes, uint32_t segmentCount, void* context, AddPageFunc addPageFunc, RemovePageFunc removePageFunc);

		// Allocate a raw block of memory. The memory won't be initialized.
		CLIB_API void* Alloc(uint32_t size, uint32_t alignment = 0);

		// Free a raw block of memory. Don't use this to free initialized class/struct object memory!
		CLIB_API void Free(void* ptr);

		CLIB_API size_t GetAllocationOffsetInPage(void* ptr) const;

		CLIB_API void* GetAllocationPageAddress(void* ptr) const;

		CLIB_API void DumpMemoryLeaks();

	private:

		// Effectively a node in the free list. Pointing to the previous node on the free list.
		struct BlockNode
		{
			BlockNode* m_prevNode;
			void* m_ptr;
			uint32_t m_size;
			uint32_t m_pageIndex;
		};

		struct Page
		{
			void* m_context = nullptr;
			void* m_start = nullptr;
			uint32_t m_allocated = 0; // Amount of memory from this page currently in use.
			uint32_t m_size = 0;
			uint32_t m_allocatedBlockCount = 0;
			uint32_t m_freeBlockNodeCount = 0;
			CLib::FixedBlockAllocator m_blockNodeStorage{ sizeof(BlockNode), sizeof(BlockNode) * 32 };
		};

		CLIB_API void AddPage(uint32_t requiredSize);

		CLIB_API void* FinalizeAllocation(BlockNode* node, uint32_t alignment);

		// Internal allocation function, all allocation functions will use this as a base for thier allocations.
		CLIB_API void* InternalAlloc(uint32_t size, uint32_t alignment = 0, const char* typeName = "void*");

		// Internal free function. All free functions will use this to free memory in the allocator.
		CLIB_API void InternalFree(void* ptr);

		// Get the index of the first free list who's segment size is larger than the provided size.
		CLIB_API uint32_t GetLargerFreeListIdx(const uint32_t& size);

		// Get the index of the first free list who's segment size is smaller than the provided size.
		CLIB_API uint32_t GetSmallerFreeListIdx(const uint32_t& size);

		inline bool IsPageFull(const Page& page, const uint32_t& allocationSize)
		{
			return page.m_allocated >= page.m_size || (page.m_size - page.m_allocated) < allocationSize;
		};

		static constexpr const uint32_t DefaultSegmentCount = 10;
		Vector<uint32_t, DefaultSegmentCount> m_segSizes =
		{ 
			0, 
			32, 
			64, 
			128, 
			256, 
			512, 
			1024, 
			2048, 
			4096, 
			0 
		};

		void* m_context = nullptr;
		uint32_t m_currentPageIndex = 0;
		AddPageFunc m_addPageFunc;
		RemovePageFunc m_removePageFunc;
		std::unordered_map<void*, BlockNode*> m_liveBlockMap;
		Vector<BlockNode*, DefaultSegmentCount> m_freeLists{ DefaultSegmentCount };
		CLib::FixedBlockAllocator m_pageStorage{ sizeof(Page), sizeof(Page) * 32 };
		Vector<Page*, 32, 32> m_pages{};
		Vector<uint32_t, 32, 32> m_freePageIndices{};
	};
}

