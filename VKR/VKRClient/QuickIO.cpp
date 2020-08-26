#include "QuickIO.h"
#include "CLib/Allocator.h"
#include <fstream>

namespace QIO
{
	bool Load(const char* path, char** data, unsigned long long* fileSize)
	{
		std::ifstream inStream(path, std::ios::binary | std::ios::ate); // Start at the end to get the size.
		auto fileExceptions = inStream.exceptions();

		if (inStream.good() && !fileExceptions)
		{
			*fileSize = static_cast<unsigned long long>(inStream.tellg()); // Get the filesize (which is the current stream position).
			inStream.seekg(0);

			char* buf = new char[*fileSize];
			inStream.read(buf, *fileSize);
			*data = buf;

			inStream.close();
			printf_s("Successfully read %i bytes of data from file at %s \n", *fileSize, path);
			return true;
		}
		else
		{
			printf_s("Could not read data from file %s \n", *fileSize, path);
			return false;
		}
	}

	bool LoadAlloc(const char* path, char** data, unsigned long long* fileSize, CLib::Allocator* allocator)
	{
		std::ifstream inStream(path, std::ios::binary | std::ios::ate); // Start at the end to get the size.
		auto fileExceptions = inStream.exceptions();

		if (inStream.good() && !fileExceptions)
		{
			*fileSize = static_cast<unsigned long long>(inStream.tellg()); // Get the filesize (which is the current stream position).
			inStream.seekg(0);

			char* buf = (char*)allocator->Alloc(static_cast<uint32_t>(*fileSize));
			inStream.read(buf, *fileSize);
			*data = buf;

			inStream.close();
			printf_s("Successfully read %i bytes of data from file at %s \n", *fileSize, path);
			return true;
		}
		else
		{
			printf_s("Could not read data from file %s \n", *fileSize, path);
			return false;
		}
	}
}
