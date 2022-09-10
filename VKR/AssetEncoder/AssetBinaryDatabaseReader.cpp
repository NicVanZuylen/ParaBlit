#include "AssetEncoder/AssetDatabaseReader.h"

#include <cassert>

namespace AssetEncoder
{
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

				const char* str = &m_stringCache[stringOffset];
				m_stringMap.insert({ str, *header });

				currentPos = header->m_nextLocation;
			}
		}
	}

	bool AssetBinaryDatabaseReader::HasOpenFile() const
	{
		return m_dbFile.is_open();
	}

	AssetMeta AssetBinaryDatabaseReader::GetAssetInfo(const char* assetName) const
	{
		auto it = m_stringMap.find(assetName);
		if (it == m_stringMap.end())
			return {};

		return it->second.m_asset;
	}

	void AssetBinaryDatabaseReader::GetAssetUserData(const AssetMeta& asset, void* outData) const
	{
		memcpy(outData, &m_stringCache[asset.m_userDataLocation], asset.m_userDataSize);
	}

	void AssetBinaryDatabaseReader::GetAssetBinary(const char* assetName, void* storage)
	{
		auto it = m_stringMap.find(assetName);
		if (it == m_stringMap.end())
			return;

		DatabaseStringHeader header = it->second;
		m_dbFile.seekg(m_index.m_assetBegin + header.m_asset.m_location);
		m_dbFile.read(reinterpret_cast<char*>(storage), header.m_asset.m_binarySize);
	}

	void AssetBinaryDatabaseReader::GetAssetBinaryRange(const char* assetName, void* storage, size_t beginBytes, size_t endBytes)
	{
		auto it = m_stringMap.find(assetName);
		if (it == m_stringMap.end())
			return;

		DatabaseStringHeader header = it->second;

		assert(beginBytes < header.m_asset.m_binarySize);
		assert(endBytes <= header.m_asset.m_binarySize);
		assert(beginBytes < endBytes);

		m_dbFile.seekg(m_index.m_assetBegin + header.m_asset.m_location + beginBytes);
		m_dbFile.read(reinterpret_cast<char*>(storage), endBytes - beginBytes);
	}
}