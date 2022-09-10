#pragma once
#include "AssetEncoder/AssetEncoderLib.h"
#include <unordered_map>
#include <fstream>

namespace AssetEncoder
{
	struct AssetDatabaseIndex
	{
		size_t m_totalSize;
		size_t m_stringBegin;
		size_t m_stringBlockSize;
		size_t m_stringCount;
		size_t m_assetBegin;
		size_t m_assetBlockSize;
		uint64_t m_buildVersion;
	};

	struct AssetMeta
	{
		size_t m_location;
		size_t m_assetSize;
		size_t m_binarySize;
		uint64_t m_dateModified;	 // Date modifed in OS time. Used to check if a file has changed to skip redundant builds.
		uint64_t m_dateBuilt;		 // Date asset was built in OS time. Used to check if a file has changed to skip redundant builds.
		size_t m_userDataSize;
		size_t m_userDataLocation;
	};

	struct DatabaseStringHeader
	{
		size_t m_stringSize;
		size_t m_nextLocation;		// Location of next string header. UINT_MAX if this is the final string.
		AssetMeta m_asset;
	};

	class AssetBinaryDatabaseReader
	{
	public:

		ASSET_ENCODER_API AssetBinaryDatabaseReader() = default;

		ASSET_ENCODER_API AssetBinaryDatabaseReader(const char* databasePath);

		ASSET_ENCODER_API ~AssetBinaryDatabaseReader();

		ASSET_ENCODER_API void OpenFile(const char* databasePath);

		ASSET_ENCODER_API bool HasOpenFile() const;

		ASSET_ENCODER_API AssetMeta GetAssetInfo(const char* name) const;

		ASSET_ENCODER_API void GetAssetUserData(const AssetMeta& asset, void* outData) const;

		ASSET_ENCODER_API void GetAssetBinary(const char* assetName, void* storage);

		ASSET_ENCODER_API void GetAssetBinaryRange(const char* assetName, void* storage, size_t beginBytes, size_t endBytes);

	private:

		std::unordered_map<std::string, DatabaseStringHeader> m_stringMap;
		char* m_stringCache = nullptr;
		AssetDatabaseIndex m_index{};
		std::streampos m_startPos{};
		std::ifstream m_dbFile;
	};
}