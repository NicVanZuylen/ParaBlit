#pragma once
#include "Vector.h"
#include <cstdint>

namespace CLib
{
	
	class Allocator
	{
	public:

		Allocator(uint32_t pageSize = 1024 * 1024);

		~Allocator();

		// Allocate a raw block of memory. The memory won't be initialized.
		void* Alloc(uint32_t size, uint32_t alignment = 0);

		// Free a raw block of memory. Don't use this to free initialized class/struct object memory!
		void Free(void* ptr);

		// Allocate a new class/struct object and call it's constructor using the provided arguments. (Equivalent to using "new T(args...)")
		template<typename T, typename... Args>
		inline T* Alloc(Args&&... args)
		{
			return new(InternalAlloc(sizeof(T), std::alignment_of<T>().value)) T(args...);
		}

		// Free an allocated class/struct object and call it's destructor.
		template<typename T>
		inline void Free(T* ptr)
		{
			ptr->~T();
			InternalFree(ptr);
		}

	private:

		// Internal allocation function, all allocation functions will use this as a base for thier allocations.
		inline void* InternalAlloc(uint32_t size, uint32_t alignment = 0);

		// Internal free function. All free functions will use this to free memory in the allocator.
		inline void InternalFree(void* ptr);

		// Allocate an additional master block/page of memory to allow for more allocations.
		inline void AllocatePage();

		// Gets the free list a block of the provided size is less than in block size.
		inline uint32_t GetUpperFreeListIdx(const uint32_t& size);

		// Gets the free list a block of the provided size is greater than in block size.
		inline uint32_t GetLowerFreeListIdx(const uint32_t& size);

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

			BlockNode* m_prevNode;
			uint32_t m_size;
		};

		static uint32_t m_segSizes[10];

		uint32_t m_pageSize;
		BlockNode* m_freeLists[_countof(m_segSizes)];
		Vector<Page, 32> m_pages;
	};
}

