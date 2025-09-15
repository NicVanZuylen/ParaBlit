#pragma once
#include "CLib/CLibAPI.h"
#include "Vector.h"
#include <cstdint>
#include <mutex>

#if _DEBUG
#define CLIB_ALLOCATOR_DEBUG 1
#else
#define CLIB_ALLOCATOR_DEBUG 0
#endif

#if CLIB_ALLOCATOR_DEBUG
#include <unordered_set>
#endif

namespace CLib
{
	class Allocator
	{
	public:

		CLIB_API Allocator(uint32_t pageSize = 1024 * 1024, bool threadSafe = false);

		CLIB_API ~Allocator();

		// Allocate a raw block of memory. The memory won't be initialized.
		CLIB_API void* Alloc(uint32_t size, uint32_t alignment = 0);

		// Free a raw block of memory. Don't use this to free initialized class/struct object memory!
		CLIB_API void Free(void* ptr);

		// Allocate a new class/struct object and call it's constructor using the provided arguments. (Equivalent to using "new T(args...)")
		template<typename T, typename... Args>
		inline T* Alloc(Args&&... args)
		{
			if (m_threadSafe)
			{
				void* ptr = nullptr;
				{
					std::lock_guard<std::mutex> lock(m_mutex);
					ptr = InternalAlloc(sizeof(T), std::alignment_of<T>().value, typeid(T).name());
				}

				return new(ptr) T(args...);
			}
			return new(InternalAlloc(sizeof(T), std::alignment_of<T>().value, typeid(T).name())) T(args...);
		}

		// Free an allocated class/struct object and call it's destructor.
		template<typename T>
		inline void Free(T* ptr)
		{
			ptr->~T();
			if (m_threadSafe)
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				InternalFree(ptr);
			}
			else
			{
				InternalFree(ptr);
			}
		}

		CLIB_API const char* GetBlockName(void* ptr);

		CLIB_API void DumpMemoryLeaks();

	private:

		// Internal allocation function, all allocation functions will use this as a base for thier allocations.
		CLIB_API void* InternalAlloc(uint32_t size, uint32_t alignment = 0, const char* typeName = "void*");

		// Internal free function. All free functions will use this to free memory in the allocator.
		CLIB_API void InternalFree(void* ptr);

		// Allocate an additional master block/page of memory to allow for more allocations.
		CLIB_API void AllocatePage();

		// Gets the free list a block of the provided size is less than in block size.
		CLIB_API uint32_t GetUpperFreeListIdx(const uint32_t& size);

		// Gets the free list a block of the provided size is greater than in block size.
		CLIB_API uint32_t GetLowerFreeListIdx(const uint32_t& size);

		struct Page
		{
			void* m_block;
			uint32_t m_allocated; // Amount of memory from this page currently in use.
		};
		inline bool IsPageFull(const Page& page, const uint32_t& size) { return page.m_allocated >= m_pageSize || (m_pageSize - page.m_allocated) < size; };

		// Effectively a node in the free list. Pointing to the previous node on the free list.
		struct BlockNode
		{
			inline bool IsFree() { return m_prevNode != nullptr; };

#if CLIB_ALLOCATOR_DEBUG
			const char* m_typeName;
#endif
			BlockNode* m_prevNode;
			uint32_t m_size;
		};

		static uint32_t m_segSizes[10];

#if CLIB_ALLOCATOR_DEBUG
		std::unordered_set<BlockNode*> m_allocatedNodes; // Allocation tracking
#endif

		uint32_t m_pageSize;
		std::mutex m_mutex;
		bool m_threadSafe;
		BlockNode* m_freeLists[_countof(m_segSizes)];
		Vector<Page, 32> m_pages;
	};
}

