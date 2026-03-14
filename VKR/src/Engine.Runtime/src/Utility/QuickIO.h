#pragma once
#include <cstdint>

namespace CLib
{
	class Allocator;
}

namespace QIO
{
	bool Load(const char* path, char** data, uint64_t* fileSize);

	bool LoadAlloc(const char* path, char** data, uint64_t* fileSize, CLib::Allocator* allocator);
}

