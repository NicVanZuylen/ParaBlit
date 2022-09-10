#include "AssetEncoder/AssetBinaryDatabase.h"

#include <cassert>
#include <exception>
#include <iostream>
#include <filesystem>
#include <cmath>

namespace AssetEncoder
{
	AssetBinaryDatabaseWriter::AssetBinaryDatabaseWriter(const char* databasePath)
	{
		m_databasePath = databasePath;
	}

	void* AssetBinaryDatabaseWriter::AllocateAsset(const char* assetName, size_t userDataSize, size_t binarySize, size_t date, char** outUserData, AssetMeta* outMeta)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		// User data is expected to be small, and can reside in string memory.
		size_t stringOffset = sizeof(DatabaseStringHeader) + userDataSize;
		size_t userDataOffset = sizeof(DatabaseStringHeader);

		size_t stringLength = strlen(assetName) + 1; // Include null terminator.
		char* strMem = reinterpret_cast<char*>(m_stringAllocator.Alloc(uint32_t(stringLength + stringOffset), 16));
		if(outUserData != nullptr)
			*outUserData = &strMem[userDataOffset];
		memcpy(&strMem[stringOffset], assetName, stringLength); // Copy string
		++m_stringCount;

		void* stringPageAddress = m_stringAllocator.GetAllocationPageAddress(strMem);
		size_t stringHeaderLocalOffset = m_stringAllocator.GetAllocationOffsetInPage(strMem);
		size_t stringPageOffset = m_stringPageHandleMap.at(stringPageAddress).second.m_offset;
		size_t stringHeaderLocation = stringPageOffset + stringHeaderLocalOffset;

		void* assetPtr = m_assetAllocator.Alloc(uint32_t(binarySize), 128);
		void* pageAddress = m_assetAllocator.GetAllocationPageAddress(assetPtr);
		size_t assetLocalOffset = reinterpret_cast<uint8_t*>(assetPtr) - reinterpret_cast<uint8_t*>(pageAddress);
		size_t pageOffset = m_assetPageHandleMap.at(pageAddress).second.m_offset;

		DatabaseStringHeader* stringHeader = reinterpret_cast<DatabaseStringHeader*>(strMem);
		stringHeader->m_stringSize = uint32_t(stringLength);
		stringHeader->m_nextLocation = ~uint32_t(0);
		stringHeader->m_asset.m_binarySize = binarySize;
		stringHeader->m_asset.m_location = pageOffset + assetLocalOffset;
		stringHeader->m_asset.m_assetSize = binarySize;
		stringHeader->m_asset.m_dateModified = date;
		stringHeader->m_asset.m_dateBuilt = std::chrono::duration_cast<std::chrono::seconds>(std::filesystem::_File_time_clock::now().time_since_epoch()).count();;
		stringHeader->m_asset.m_userDataSize = userDataSize;
		stringHeader->m_asset.m_userDataLocation = stringHeaderLocation + userDataOffset;

		if (outMeta != nullptr)
			*outMeta = stringHeader->m_asset;

		if (m_lastStringHeader)
		{
			m_lastStringHeader->m_nextLocation = stringHeaderLocation;
		}
		m_lastStringHeader = stringHeader;

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
			for (auto& [page, pageData] : m_stringPageHandleVector)
			{
				++stringCount;

				dbFile.seekp(currentPos);
				dbFile.write(reinterpret_cast<const char*>(page), pageData.m_size);
				currentPos += pageData.m_size;
				currentSize += pageData.m_size;
			}
			index.m_stringBlockSize = currentSize;
			currentSize = 0;

			// Write asset binaries...
			index.m_assetBegin = currentPos;
			for (auto& [page, pageData] : m_assetPageHandleVector)
			{
				assert(pageData.m_offset == size_t(currentPos) - index.m_assetBegin);

				dbFile.seekp(currentPos);
				dbFile.write(reinterpret_cast<const char*>(page), pageData.m_size);
				currentPos += pageData.m_size;
				currentSize += pageData.m_size;
			}
			index.m_assetBlockSize = currentSize;

			index.m_totalSize = currentPos;

			// Write header...
			dbFile.seekp(beginPos);
			dbFile.write(reinterpret_cast<const char*>(&index), sizeof(AssetDatabaseIndex));
		}

		dbFile.close();
	}

	void* AssetBinaryDatabaseWriter::StringPageAlloc(void* context, uint32_t requestedMinSize, uint32_t& outSize)
	{
		outSize = StringCachePageSize;
		void* newPage = malloc(outSize);
		AssetBinaryDatabaseWriter* db = reinterpret_cast<AssetBinaryDatabaseWriter*>(context);

		PageData data{ outSize, db->m_currentStringPageOffset };
		PageHandle newHandle{ newPage, data };

		db->m_stringPageHandleMap.insert({ newPage, newHandle });
		db->m_stringPageHandleVector.PushBack(newHandle);
		db->m_currentStringPageOffset += outSize;
		return newPage;
	}

	void* AssetBinaryDatabaseWriter::AssetPageAlloc(void* context, uint32_t requestedMinSize, uint32_t& outSize)
	{
		if (requestedMinSize > AssetCachePageSize && requestedMinSize % AssetCachePageSize > 0)
		{
			outSize = ((requestedMinSize / AssetCachePageSize) + 1) * AssetCachePageSize;
		}
		else
		{
			outSize = std::max<uint32_t>(requestedMinSize, uint32_t(AssetCachePageSize));
		}

		void* newPage = malloc(outSize);
		AssetBinaryDatabaseWriter* db = reinterpret_cast<AssetBinaryDatabaseWriter*>(context);

		PageData data{ outSize, db->m_currentAssetPageOffset };
		PageHandle newHandle{ newPage, data };
		db->m_assetPageHandleMap.insert({ newPage, newHandle });
		db->m_assetPageHandleVector.PushBack(newHandle);
		db->m_currentAssetPageOffset += outSize;
		
		return newPage;
	}
}
