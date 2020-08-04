#pragma once
#include "Vector.h"
#include <stdint.h>

namespace CLib
{
	// A fast and simple fixed-sized memory block allocator by Nicholas Van Zuylen. Good for making lots of small allocations with efficiency (95% faster than 'new').
	class FixedBlockAllocator
	{
	public:

		FixedBlockAllocator(uint32_t blockSize, uint32_t pageSize = 4096);

		~FixedBlockAllocator();

		// Allocate a raw block of memory. The memory won't be initialized.
		void* Alloc();

		// Free a raw block of memory. Don't use this to free initialized class/struct object memory!
		void Free(void* ptr);

		// Allocate a new class/struct object and call it's constructor using the provided arguments. (Equivalent to using "new T(args...)")
		// Attempting to allocate an object larger than this allocator's block size will return nullptr.
		template<typename T, typename... Args>
		inline T* Alloc(Args&&... args)
		{
			if (sizeof(T) > m_blockSize)
				return nullptr;

			void* block = InternalAlloc();
			return new(block) T(args...);
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
		inline void* InternalAlloc();

		// Internal free function. All free functions will use this to free memory in the allocator.
		inline void InternalFree(void* ptr);

		// Allocate an additional master block/page of memory to allow for more allocations.
		inline void AllocatePage();

		struct Page
		{
			void* m_block;
			uint32_t m_remainingBlocks;
		};
		
		// Effectively a node in the free list. Pointing to the previous node on the free list.
		struct BlockNode
		{
			BlockNode* m_prev;
		};

		BlockNode* m_freeList;
		uint32_t m_blockSize;
		uint32_t m_blockCount;
		uint32_t m_pageSize;
		Vector<Page, 32> m_pages;
	};
}

