#pragma once
#include "CLib/CLibAPI.h"
#include "Vector.h"
#include <cstdint>

namespace CLib
{
	class SubAllocator
	{
	public:

		CLIB_API SubAllocator();

		CLIB_API ~SubAllocator();

		CLIB_API bool DumpLeaks();

		CLIB_API void* Alloc(uint32_t size, uint32_t alignment = 0);

		CLIB_API void Free(void* ptr);

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

		CLIB_API void* InternalAlloc(uint32_t size, uint32_t alignment = 4);
		CLIB_API void InternalFree(void* ptr);
		CLIB_API void CreatePage();

		Vector<Page, 32> m_pages;
	};
}

