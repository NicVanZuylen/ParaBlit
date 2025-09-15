#include "Engine.AssetEncoder/AssetDatabaseReader.h"

#include <cassert>

namespace AssetEncoder
{
	AssetID AssetHandle::GetID(const AssetBinaryDatabaseReader* reader)
	{
		if (m_id == ~AssetID(0))
		{
			m_id = reader->GetAssetID(m_assetName);
		}

		return m_id;
	}

	AssetBinaryDatabaseReader::AssetBinaryDatabaseReader(const char* databasePath)
	{
		OpenFile(databasePath);
	}

	AssetBinaryDatabaseReader::~AssetBinaryDatabaseReader()
	{
		if(m_stringCache != nullptr)
			free(m_stringCache);
		m_dbFile.close();
	}

	void AssetBinaryDatabaseReader::OpenFile(const char* databasePath)
	{
		m_dbFile = std::ifstream(databasePath, std::ios::binary | std::ios::beg);
		auto fileExceptions = m_dbFile.exceptions();

		if (!m_dbFile.good() || fileExceptions)
			return;

		m_startPos = m_dbFile.tellg();

		// Read header...
		m_dbFile.read(reinterpret_cast<char*>(&m_index), sizeof(AssetDatabaseIndex));

		// Read strings and build map of strings to assets...
		m_dbFile.seekg(m_index.m_stringBegin);

		m_stringCache = reinterpret_cast<char*>(malloc(m_index.m_stringBlockSize));

		if (m_stringCache != nullptr)
		{
			m_dbFile.read(m_stringCache, m_index.m_stringBlockSize);

			size_t currentPos = 0;
			for (size_t currentString = 0; currentString < m_index.m_stringCount; ++currentString)
			{
				DatabaseStringHeader* header = reinterpret_cast<DatabaseStringHeader*>(&m_stringCache[currentPos]);
				size_t stringOffset = currentPos + sizeof(DatabaseStringHeader) + header->m_asset.m_userDataSize;

				uint64_t id = m_currentFreeId++;

				const char* str = &m_stringCache[stringOffset];
				m_assetMap.insert({ id, *header });
				m_idToStringMap.insert({ AssetString(str), id});
				m_stringToIDMap.insert({ id, AssetString(str) });

				currentPos = header->m_nextLocation;
			}
		}
	}

	bool AssetBinaryDatabaseReader::HasOpenFile() const
	{
		return m_dbFile.is_open();
	}

	AssetID AssetBinaryDatabaseReader::GetAssetID(const char* assetName) const
	{
		AssetString str(assetName);
		auto it = m_idToStringMap.find(str);
		if (it == m_idToStringMap.end())
			return 0;

		return it->second;
	}

	AssetID AssetBinaryDatabaseReader::GetAssetID(AssetHandle& handle) const
	{
		return handle.GetID(this);
	}

	AssetMeta AssetBinaryDatabaseReader::GetAssetInfo(AssetID id) const
	{
		auto it = m_assetMap.find(id);
		if (it == m_assetMap.end())
			return {};

		return it->second.m_asset;
	}

	const char* AssetBinaryDatabaseReader::GetAssetName(AssetID id) const
	{
		auto it = m_stringToIDMap.find(id);
		if (it != m_stringToIDMap.end())
		{
			return it->second.m_string.c_str();
		}

		return nullptr;
	}

	AssetMeta AssetBinaryDatabaseReader::GetAssetInfo(AssetHandle& handle) const
	{
		return GetAssetInfo(handle.GetID(this));
	}

	void AssetBinaryDatabaseReader::GetAssetUserData(const AssetMeta& asset, void* outData) const
	{
		memcpy(outData, &m_stringCache[asset.m_userDataLocation], asset.m_userDataSize);
	}

	void AssetBinaryDatabaseReader::GetAssetBinary(AssetID id, void* storage)
	{
		auto it = m_assetMap.find(id);
		if (it == m_assetMap.end())
			return;

		DatabaseStringHeader header = it->second;
		m_dbFile.seekg(m_index.m_assetBegin + header.m_asset.m_location);
		m_dbFile.read(reinterpret_cast<char*>(storage), header.m_asset.m_binarySize);
	}

	void AssetBinaryDatabaseReader::GetAssetBinary(AssetHandle& handle, void* storage)
	{
		GetAssetBinary(handle.GetID(this), storage);
	}

	void AssetBinaryDatabaseReader::GetAssetBinaryRange(AssetID id, void* storage, size_t beginBytes, size_t endBytes)
	{
		auto it = m_assetMap.find(id);
		if (it == m_assetMap.end())
			return;

		DatabaseStringHeader header = it->second;

		assert(beginBytes < header.m_asset.m_binarySize);
		assert(endBytes <= header.m_asset.m_binarySize);
		assert(beginBytes < endBytes);

		m_dbFile.seekg(m_index.m_assetBegin + header.m_asset.m_location + beginBytes);
		m_dbFile.read(reinterpret_cast<char*>(storage), endBytes - beginBytes);
	}

	void AssetBinaryDatabaseReader::GetAssetBinaryRange(AssetHandle& handle, void* storage, size_t beginBytes, size_t endBytes)
	{
		GetAssetBinaryRange(handle.GetID(this), storage, beginBytes, endBytes);
	}

	size_t AssetBinaryDatabaseReader::AssetStringHasher::operator() (const AssetString& string) const
	{
		std::hash<std::string> hasher;
		return hasher(string.m_string);
	}
}