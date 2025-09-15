#pragma once
#include "Engine.AssetEncoder/EncoderBase.h"
#include "Engine.Control/IDataFile.h"

namespace Reflectron
{
	class ReflectionGenerator : public AssetEncoder::EncoderBase
	{
	public:

		ReflectionGenerator(const char* name, const char* dbName, const char* sourceDirectory);

		~ReflectionGenerator();

	private:

		uint64_t GetTimestamp(std::string& src);
		bool FileNeedsReflection(const std::string& src);
		void GenerateReflectionCode(std::string& outfileStr, const std::string& src, const std::string& fileName, const std::string& generatedFilename, uint64_t timestamp);
		void InsertGeneratedHeader(std::string& src, const std::string& generatedFilename);
		void GetReflectionFieldArgs(std::string& src, size_t pos, std::vector<std::string>& outArgs);
	};
}
