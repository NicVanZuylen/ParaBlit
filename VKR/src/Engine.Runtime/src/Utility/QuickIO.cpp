#include "QuickIO.h"
#include "CLib/Allocator.h"
#include <fstream>

namespace QIO
{
	bool Load(const char* path, char** data, uint64_t* fileSize)
	{
		std::ifstream inStream(path, std::ifstream::binary | std::ifstream::ate); // Start at the end to get the size.
		auto fileExceptions = inStream.exceptions();

		if (inStream.good() && !fileExceptions)
		{
			*fileSize = static_cast<uint64_t>(inStream.tellg()); // Get the filesize (which is the current stream position).
			inStream.seekg(0);

			char* buf = new char[*fileSize];
			inStream.read(buf, *fileSize);
			*data = buf;

			inStream.close();
			printf("Successfully read %u bytes of data from file at %s \n", (unsigned int)*fileSize, path);
			return true;
		}
		else
		{
			printf("Could not read data from file %s \n", path);
			return false;
		}
	}

	bool LoadAlloc(const char* path, char** data, uint64_t* fileSize, CLib::Allocator* allocator)
	{
		std::ifstream inStream(path, std::ifstream::binary | std::ifstream::ate); // Start at the end to get the size.
		auto fileExceptions = inStream.exceptions();

		if (inStream.good() && !fileExceptions)
		{
			*fileSize = static_cast<uint64_t>(inStream.tellg()); // Get the filesize (which is the current stream position).
			inStream.seekg(0);

			char* buf = (char*)allocator->Alloc(static_cast<uint32_t>(*fileSize));
			inStream.read(buf, *fileSize);
			*data = buf;

			inStream.close();
			printf("Successfully read %u bytes of data from file at %s \n", (unsigned int)*fileSize, path);
			return true;
		}
		else
		{
			printf("Could not read data from file %s \n", path);
			return false;
		}
	}
}
