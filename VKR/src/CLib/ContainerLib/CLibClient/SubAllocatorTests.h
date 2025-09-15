#pragma once
#include "CLib/SubAllocator.h"
#include <iostream>
#include <random>

namespace CLibTest
{
	void SubAllocatorTest()
	{
		std::cout << "------------------------ SubAllocator Tests ------------------------" << std::endl;

		CLib::SubAllocator testAlloc;

		// Make 1000 allocations and free them in order of allocation.
		CLib::Vector<void*, 1000> ptrs;
		for (int i = 0; i < 1000; ++i)
			ptrs.PushBack(testAlloc.Alloc(16));

		for (int i = 999; i > -1; --i)
			testAlloc.Free(ptrs[i]);
		ptrs.Clear();

		// Allocate a class that must be constructed and destructed (Vector in this case)
		auto* vec = testAlloc.Alloc<CLib::Vector<int, 1000>>();
		testAlloc.Free(vec);

		// Make 100 allocations and free them in random order.
		constexpr int allocCount = 100;
		for (int i = 0; i < allocCount; ++i)
		{
			if(i == 49) // To mix things up a little, allocate a vector at i == 49. It will be freed last.
				vec = testAlloc.Alloc<CLib::Vector<int, 1000>>();
			ptrs.PushBack(testAlloc.Alloc(16));
		}

		std::default_random_engine rng;
		uint32_t ptrCount = allocCount;
		while (ptrCount)
		{
			std::uniform_int_distribution<uint32_t> dst(0, allocCount - 1);
			uint32_t rand = dst(rng);

			auto& localPtr = ptrs[rand];
			if (localPtr)
			{
				testAlloc.Free(localPtr);

				--ptrCount;
				localPtr = nullptr;
			}
		}
		ptrs.Clear();

		testAlloc.Free(vec);

		// Allocate random-sized blocks with an alignment of 4.
		std::uniform_int_distribution<uint32_t> dst(1, 16);
		for(int i = 0; i < 10; ++i)
			ptrs.PushBack(testAlloc.Alloc(dst(rng), 4));

		// Dump currently allocated blocks...
		testAlloc.DumpLeaks();

		for (uint32_t i = 0; i < ptrs.Count(); ++i)
			testAlloc.Free(ptrs[i]);
		ptrs.Clear();

		// Overflow the page size to test page allocation, whilst also testing increasing allocation block sizes.
		uint32_t allocSize = 512;
		while (allocSize <= 8192)
		{
			for (int i = 0, c = (1024 * 1024 * 2) / allocSize; i < c; ++i)
				ptrs.PushBack(testAlloc.Alloc(allocSize));

			for (uint32_t i = 0; i < ptrs.Count(); ++i)
				testAlloc.Free(ptrs[i]);
			ptrs.Clear();

			allocSize *= 2;
		}

		// Finish with a single allocation when no other allocations are live. 
		// It is useful to step into the allocator code with this allocation to ensure the memory state tracking is correct by this point.
		auto* ptr = testAlloc.Alloc(4096);
		testAlloc.Free(ptr);

		std::cout << "------------------------ SubAllocator Tests Complete ------------------------" << std::endl;
	}
}