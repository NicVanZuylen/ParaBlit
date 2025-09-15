#pragma once
#include "CLib/FixedBlockAllocator.h"

#include <chrono>
#include <iostream>

namespace CLibTest
{
	class TestClass
	{
	public:

		TestClass(int foo)
		{
			m_foo = foo;
		}

		~TestClass()
		{
			m_foo = 0;
		}

	private:

		size_t m_foo;
	};

	void FixedBlockAllocatorTest()
	{
		std::cout << "------------------------ FixedBlockAllocator Tests ------------------------\n\n";
		CLib::FixedBlockAllocator allocator(sizeof(TestClass), 1024 * 1024);

		CLib::Vector<void*, 500> ptrs;
		std::cout << "For both tests: Allocating 500 blocks of raw memory of size: " << sizeof(TestClass) << ", then 500 instances of TestClass (size: " << sizeof(TestClass) << ").\n";
		auto startTime = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < 500; ++i)
			ptrs.PushBack(allocator.Alloc());

		for (int i = ptrs.Count() - 1; i > -1; --i)
		{
			allocator.Free(ptrs[i]);
			ptrs[i] = nullptr;
		}
		ptrs.Clear();

		for (int i = 0; i < 500; ++i)
		{
			ptrs.PushBack(allocator.Alloc<TestClass>(i));
		}

		for (auto& ptr : ptrs)
		{
			allocator.Free((TestClass*)ptr);
			ptr = nullptr;
		}
		ptrs.Clear();

		auto endTime = std::chrono::high_resolution_clock::now();

		auto timeDiff = endTime - startTime;
		auto time = std::chrono::duration_cast<std::chrono::microseconds>(timeDiff).count();

		std::cout << "Allocations using 'FixedBlockAllocator' took " << time << "us\n";

		startTime = std::chrono::high_resolution_clock::now();

		for (int i = 0; i < 500; ++i)
			ptrs.PushBack(new unsigned char[sizeof(TestClass)]);

		for (int i = ptrs.Count() - 1; i > -1; --i)
		{
			delete[] ptrs[i];
			ptrs[i] = nullptr;
		}
		ptrs.Clear();

		for (int i = 0; i < 500; ++i)
		{
			ptrs.PushBack(new TestClass(i));
		}

		for (auto& ptr : ptrs)
		{
			delete ptr;
			ptr = nullptr;
		}
		ptrs.Clear();

		endTime = std::chrono::high_resolution_clock::now();

		timeDiff = endTime - startTime;
		time = std::chrono::duration_cast<std::chrono::microseconds>(timeDiff).count();

		std::cout << "Allocations using 'new' took " << time << "us\n\n";
		std::cout << "------------------------ FixedBlockAllocator Tests Complete ------------------------\n";
	}
};