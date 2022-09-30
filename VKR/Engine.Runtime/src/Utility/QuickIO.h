#pragma once

namespace CLib
{
	class Allocator;
}

namespace QIO
{
	bool Load(const char* path, char** data, unsigned long long* fileSize);

	bool LoadAlloc(const char* path, char** data, unsigned long long* fileSize, CLib::Allocator* allocator);
}

