#include "AssetEncoder/AssetBinaryDatabase.h"

#include <cassert>
#include <exception>
#include <iostream>
#include <filesystem>

namespace AssetEncoder
{
	AssetBinaryDatabaseWriter::AssetBinaryDatabaseWriter(const char* databasePath)
	{
		m_databasePath = databasePath;
	}

	void* AssetBinaryDatabaseWriter::AllocateAsset(const char* assetName, size_t binarySize, size_t date)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		size_t stringLength = strlen(assetName) + 1; // Include null terminator.
		char* strMem = reinterpret_cast<char*>(m_stringAllocator.Alloc(uint32_t(stringLength) + sizeof(DatabaseStringHeader), 16));
		memcpy(&strMem[sizeof(DatabaseStringHeader)], assetName, stringLength);
		++m_stringCount;

		void* assetPtr = m_assetAllocator.Alloc(uint32_t(binarySize), 128);

		size_t stringHeaderOffset = reinterpret_cast<size_t>(strMem) - reinterpret_cast<size_t>(m_stringPageHandles.Back());
		size_t stringHeaderLocation = ((m_stringPageHandles.Count() - 1) * StringCachePageSize) + stringHeaderOffset;
		size_t assetOffset = reinterpret_cast<size_t>(assetPtr) - reinterpret_cast<size_t>(m_assetPageHandles.Back());
		
		DatabaseStringHeader* stringHeader = reinterpret_cast<DatabaseStringHeader*>(strMem);
		stringHeader->m_stringSize = uint32_t(stringLength);
		stringHeader->m_stride = 0;
		stringHeader->m_asset.m_binarySize = binarySize;
		stringHeader->m_asset.m_location = ((m_assetPageHandles.Count() - 1) * AssetCachePageSize) + assetOffset;
		stringHeader->m_asset.m_assetSize = binarySize;
		stringHeader->m_asset.m_dateModified = date;

		if (m_lastStringHeaderStride)
		{
			*m_lastStringHeaderStride = stringHeaderLocation - m_lastStringHeaderLocation;
		}
		m_lastStringHeaderStride = &stringHeader->m_stride;
		m_lastStringHeaderLocation = stringHeaderLocation;

		return assetPtr;
	}

	void AssetBinaryDatabaseWriter::WriteDatabase()
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		// Create database path if it doesn't exist.
		std::filesystem::path path = m_databasePath;
		path.remove_filename();
		if (!std::filesystem::exists(path))
			std::filesystem::create_directory(path);

		std::ofstream dbFile(m_databasePath, std::ios::binary | std::ios::out | std::ios::beg);
		auto fileExceptions = dbFile.exceptions();

		assert(dbFile.good() && !fileExceptions);
		if (dbFile.good() && !fileExceptions)
		{
			std::streampos beginPos = dbFile.tellp();
			std::streampos currentPos = beginPos;

			AssetDatabaseIndex index;
			index.m_stringBegin = size_t(currentPos) + sizeof(AssetDatabaseIndex);
			index.m_stringCount = m_stringCount;
			index.m_buildVersion = ASSET_ENCODER_VERSION;

			size_t currentSize = 0;

			// Write strings...
			size_t stringCount = 0;
			currentPos = index.m_stringBegin;
			for (auto& stringPage : m_stringPageHandles)
			{
				++stringCount;

				dbFile.seekp(currentPos);
				dbFile.write(reinterpret_cast<const char*>(stringPage), StringCachePageSize);
				currentPos += StringCachePageSize;
				currentSize += StringCachePageSize;
			}
			index.m_stringBlockSize = currentSize;
			currentSize = 0;

			// Write asset binaries...
			index.m_assetBegin = currentPos;
			for (auto& assetPage : m_assetPageHandles)
			{
				dbFile.seekp(currentPos);
				dbFile.write(reinterpret_cast<const char*>(assetPage), AssetCachePageSize);
				currentPos += AssetCachePageSize;
				currentSize += AssetCachePageSize;
			}
			index.m_assetBlockSize = currentSize;

			index.m_totalSize = currentPos;

			// Write header...
			dbFile.seekp(beginPos);
			dbFile.write(reinterpret_cast<const char*>(&index), sizeof(AssetDatabaseIndex));
		}

		dbFile.close();
	}
}
