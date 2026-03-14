#include "VectorTests.h"
#include "SubAllocatorTests.h"
#include "AllocatorTests.h"
#include "ReflectionTests.h"

#include "CLib/String.h"

using namespace CLib;

static CLib::Allocator vectorAllocator;

static void* VectorAlloc(unsigned long long size)
{
	return vectorAllocator.Alloc((unsigned int)size);
}

static void VectorFree(void* ptr)
{
	vectorAllocator.Free(ptr);
}

int main() 
{
#if CLIB_CLIENT_WINDOWS
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	CLibTest::ReflectionTest();
	CLibTest::VectorTest();
	CLibTest::SubAllocatorTest();
	CLibTest::AllocatorTest();	
	CLibTest::FixedBlockAllocatorTest();

	CLib::vectorAllocFunc = VectorAlloc;
	CLib::vectorFreeFunc = VectorFree;

	Vector<int> testVec;
	testVec.Reserve(10);

	String testString = "Noice";
	String testString2 = ", Noicer";

	String testString3 = testString;
	testString3 += testString2;

	std::cout << testString3 << std::endl;

	return 0;
}