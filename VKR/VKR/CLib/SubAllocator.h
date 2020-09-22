#pragma once
#include "Vector.h"

namespace CLib
{
	class SubAllocator
	{
	public:

		SubAllocator();

		~SubAllocator();

		bool DumpLeaks();

		void* Alloc(uint32_t size, uint32_t alignment = 0);

		void Free(void* ptr);

		template<typename T>
		inline T* Alloc(uint32_t alignment = 0)
		{
			T* typedPtr = new(InternalAlloc(sizeof(T), alignment)) T();
			return typedPtr;
		}

		template<typename T>
		inline void Free(T* ptr)
		{
			ptr->~T();
			InternalFree(ptr);
		}

	private:

		using u8 = unsigned char;

		struct BlockMeta
		{
			static const int64_t MinSize;
			uint32_t m_size;
			uint32_t m_diffFromPrevBlock;
			uint32_t m_start;
			uint32_t m_pageIndex : 31;
			uint32_t m_allocated : 1;
		};

		struct Page
		{
			static const uint32_t PageSize = 1024 * 1024;
			u8* m_base = nullptr;
		};

		inline void* InternalAlloc(uint32_t size, uint32_t alignment = 4);
		inline void InternalFree(void* ptr);
		inline void CreatePage();

		Vector<Page, 32> m_pages;
	};
}

