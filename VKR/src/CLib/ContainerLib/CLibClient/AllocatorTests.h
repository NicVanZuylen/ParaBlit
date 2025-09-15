#pragma once
#include <chrono>
#include "FixedBlockAllocatorTests.h"
#include "CLib/Allocator.h"

namespace CLibTest
{
	void AllocatorTest()
	{
		std::cout << "------------------------ Allocator Tests ------------------------\n\n";

		{
			CLib::Allocator allocator;

			static constexpr const uint32_t AllocationCount = 1500;
			static constexpr const uint32_t RawBlockSize = 8;

			std::cout << "Allocating " << AllocationCount << " blocks of size " << RawBlockSize << " and " << AllocationCount << " instances of TestClass (size: " << sizeof(TestClass) << "), using CLib::Allocator and 'new/delete'.\n";

			CLib::Vector<void*, AllocationCount> ptrs;
			auto startTime = std::chrono::high_resolution_clock::now();
			for (int i = 0; i < AllocationCount; ++i)
				ptrs.PushBack(allocator.Alloc(RawBlockSize, 8));

			for (auto& ptr : ptrs)
			{
				allocator.Free(ptr);
				ptr = nullptr;
			}
			ptrs.Clear();

			for (int i = 0; i < AllocationCount; ++i)
				ptrs.PushBack(allocator.Alloc<TestClass>(i));

			for (auto& ptr : ptrs)
			{
				allocator.Free((TestClass*)ptr);
				ptr = nullptr;
			}
			ptrs.Clear();

			auto endTime = std::chrono::high_resolution_clock::now();

			auto timeDiff = endTime - startTime;
			auto time = std::chrono::duration_cast<std::chrono::microseconds>(timeDiff).count();

			std::cout << "Allocations using 'Allocator' took " << time << "us\n";

			startTime = std::chrono::high_resolution_clock::now();
			for (int i = 0; i < AllocationCount; ++i)
				ptrs.PushBack(new unsigned char[RawBlockSize]);

			for (auto& ptr : ptrs)
			{
				delete ptr;
				ptr = nullptr;
			}
			ptrs.Clear();

			for (int i = 0; i < AllocationCount; ++i)
				ptrs.PushBack(new TestClass(i));

			for (auto& ptr : ptrs)
			{
				delete ptr;
				ptr = nullptr;
			}
			ptrs.Clear();

			endTime = std::chrono::high_resolution_clock::now();

			timeDiff = endTime - startTime;
			time = std::chrono::duration_cast<std::chrono::microseconds>(timeDiff).count();

			std::cout << "Allocations using 'new' took " << time << "us\n";

			std::cout << "Memory Leak Testing...\n";

			// Memory leak testing.
			allocator.Alloc<TestClass>(0);
			allocator.Alloc(sizeof(TestClass));
			allocator.Alloc(16);
			allocator.Alloc(64);
			allocator.Alloc(128);

			allocator.DumpMemoryLeaks();
		}

		std::cout << "Stressing Allocator stability...\n";

		{
			CLib::Allocator allocator;

			// This test should check the highest and lowest free lists for available blocks. But the second allocations will be too small. The allocator needs to see this and make a new block instead.

			auto* ptr = allocator.Alloc(8);
			allocator.Free(ptr);

			ptr = allocator.Alloc(16);
			allocator.Free(ptr);

			ptr = allocator.Alloc(4096 * 2);
			allocator.Free(ptr);

			ptr = allocator.Alloc(4096 * 4);
			allocator.Free(ptr);
		}

		std::cout << "------------------------ Allocator Tests Complete ------------------------\n";
	}
}