#pragma once
#include "Engine.AssetEncoder/AssetEncoderLib.h"
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

	class AssetBinaryDatabaseReader;

	using AssetID = uint64_t;
	class AssetHandle
	{
	public:

		AssetHandle(const char* assetName)
			: m_assetName(assetName)
		{

		}

		ASSET_ENCODER_API AssetID GetID(const AssetBinaryDatabaseReader* reader);

	private:

		const char* m_assetName;
		AssetID m_id = ~AssetID(0);
	};

	class AssetBinaryDatabaseReader
	{
	public:


		ASSET_ENCODER_API AssetBinaryDatabaseReader() = default;

		ASSET_ENCODER_API AssetBinaryDatabaseReader(const char* databasePath);

		ASSET_ENCODER_API ~AssetBinaryDatabaseReader();

		ASSET_ENCODER_API void OpenFile(const char* databasePath);

		ASSET_ENCODER_API bool HasOpenFile() const;

		ASSET_ENCODER_API AssetID GetAssetID(const char* assetName) const;
		ASSET_ENCODER_API AssetID GetAssetID(AssetHandle& handle) const;

		ASSET_ENCODER_API AssetMeta GetAssetInfo(AssetHandle& handle) const;
		ASSET_ENCODER_API AssetMeta GetAssetInfo(AssetID id) const;

		ASSET_ENCODER_API void GetAssetUserData(const AssetMeta& asset, void* outData) const;

		ASSET_ENCODER_API void GetAssetBinary(AssetHandle& handle, void* storage);

		ASSET_ENCODER_API void GetAssetBinaryRange(AssetID id, void* storage, size_t beginBytes, size_t endBytes);
		ASSET_ENCODER_API void GetAssetBinaryRange(AssetHandle& handle, void* storage, size_t beginBytes, size_t endBytes);

	private:

		struct AssetString
		{
			bool operator == (const AssetString& other) const
			{
				return strcmp(m_string.c_str(), other.m_string.c_str()) == 0;
			}

			std::string m_string;
		};

		struct AssetStringHasher
		{
			size_t operator () (const AssetString& string) const;
		};

		std::unordered_map<AssetID, DatabaseStringHeader> m_assetMap;
		AssetID m_currentFreeId = 1;
		std::unordered_map<AssetString, AssetID, AssetStringHasher> m_stringMap;
		char* m_stringCache = nullptr;
		AssetDatabaseIndex m_index{};
		std::streampos m_startPos{};
		std::ifstream m_dbFile;
	};
}