#pragma once
#include <mutex>
#include <unordered_map>

#include "Engine.AssetEncoder/AssetBinaryDatabaseReader.h"
#include "CLib/ExternalAllocator.h"
#include "CLib/Vector.h"

namespace AssetEncoder
{
	class AssetBinaryDatabaseWriter
	{
	public:

		ASSET_ENCODER_API AssetBinaryDatabaseWriter(const char* databasePath);

		ASSET_ENCODER_API ~AssetBinaryDatabaseWriter();

		ASSET_ENCODER_API void* AllocateAsset(const char* assetName, const Ctrl::GUID& guid, size_t userDataSize, size_t binarySize, size_t date, char** outUserData = nullptr, AssetMeta* outMeta = nullptr);

		ASSET_ENCODER_API void WriteDatabase();

	private:

		static constexpr size_t StringCachePageSize = 1024;
		static constexpr size_t AssetCachePageSize = 64 * 1024 * 1024;

		static void* StringPageAlloc(void* context, uint32_t requestedMinSize, uint32_t& outSize);

		static void* AssetPageAlloc(void* context, uint32_t requestedMinSize, uint32_t& outSize);

		static void PageFree(void* context, void* ptr)
		{
			return free(ptr);
		}

		struct PageData
		{
			size_t m_size = 0;
			size_t m_offset = 0;
		};

		using PageHandle = std::pair<void*, PageData>;
		using PageHandleVector = CLib::Vector<PageHandle>;
		using PageHandleMap = std::unordered_map<void*, PageHandle>;

		DatabaseStringHeader* m_lastStringHeader = nullptr;
		PageHandleMap m_stringPageHandleMap;
		PageHandleVector m_stringPageHandleVector;
		size_t m_currentStringPageOffset = 0;
		uint32_t m_stringCount = 0;
		CLib::ExternalAllocator m_stringAllocator{ this, StringPageAlloc, PageFree };
		CLib::Vector<void*, 1, 256> m_stringAllocations;

		PageHandleMap m_assetPageHandleMap;
		PageHandleVector m_assetPageHandleVector;
		size_t m_currentAssetPageOffset = 0;
		CLib::ExternalAllocator m_assetAllocator{ this, AssetPageAlloc, PageFree };
		CLib::Vector<void*, 1, 256> m_assetAllocations;

		std::string m_databasePath;
		std::mutex m_mutex;
	};
}