#pragma once
#include <mutex>
#include <map>
#include <unordered_map>
#include <fstream>

#include "AssetEncoder/AssetDatabaseReader.h"
#include "CLib/ExternalAllocator.h"
#include "CLib/Vector.h"

namespace AssetEncoder
{
	class AssetBinaryDatabaseWriter
	{
	public:

		ASSET_ENCODER_API AssetBinaryDatabaseWriter(const char* databasePath);

		ASSET_ENCODER_API void* AllocateAsset(const char* assetName, size_t userDataSize, size_t binarySize, size_t date, char** outUserData = nullptr, AssetMeta* outMeta = nullptr);

		ASSET_ENCODER_API void WriteDatabase();

	private:

		static constexpr size_t StringCachePageSize = 1024;
		static constexpr size_t AssetCachePageSize = 64 * 1024 * 1024;

		static void* StringPageAlloc(void* context, uint32_t requestedMinSize, uint32_t& outSize)
		{
			outSize = StringCachePageSize;
			void* newPage = malloc(outSize);
			AssetBinaryDatabaseWriter* db = reinterpret_cast<AssetBinaryDatabaseWriter*>(context);
			db->m_stringPageHandles.PushBack(newPage);
			return newPage;
		}

		static void* AssetPageAlloc(void* context, uint32_t requestedMinSize, uint32_t& outSize)
		{
			outSize = AssetCachePageSize;
			void* newPage = malloc(outSize);
			AssetBinaryDatabaseWriter* db = reinterpret_cast<AssetBinaryDatabaseWriter*>(context);
			db->m_assetPageHandles.PushBack(newPage);
			return newPage;
		}

		static void PageFree(void* context, void* ptr)
		{
			return free(ptr);
		}

		using PageHandles = CLib::Vector<void*, 8>;

		size_t* m_lastStringHeaderStride = nullptr;
		size_t m_lastStringHeaderLocation = 0;
		PageHandles m_stringPageHandles;
		uint32_t m_stringCount = 0;
		CLib::ExternalAllocator m_stringAllocator{ this, StringPageAlloc, PageFree };
		std::unordered_map<size_t, size_t> m_assetLocationStringMap; // Key=AssetMeta location in asset cache // Val=String location in string cache

		PageHandles m_assetPageHandles;
		CLib::ExternalAllocator m_assetAllocator{ this, AssetPageAlloc, PageFree };

		std::string m_databasePath;
		std::mutex m_mutex;
	};
}