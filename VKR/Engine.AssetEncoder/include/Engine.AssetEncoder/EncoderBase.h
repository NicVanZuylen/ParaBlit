#pragma once
#include <string>
#include <filesystem>

#include "Engine.AssetEncoder/AssetEncoderLib.h"
#include "CLib/Vector.h"
#include "Engine.AssetEncoder/AssetBinaryDatabase.h"

namespace AssetEncoder
{
	struct FileInfo
	{
		size_t m_lastModifiedTime = 0;
		std::string m_fileName;
	};

	class EncoderBase
	{
	public:

		ASSET_ENCODER_API EncoderBase(const char* name, const char* dbName, const char* assetDirectory);

		ASSET_ENCODER_API ~EncoderBase();

	protected:

		struct AssetStatus
		{
			AssetMeta m_info{};
			std::string m_fullPath;
			std::string m_propertyFilePath;
			std::string m_dbPath;
			std::string m_extension;
			size_t m_lastModifiedTime;
			bool m_hasPropertyFile = false;
			bool m_outdated = false;
		};

		ASSET_ENCODER_API static std::string ConvertPathToDBFormat(const char* databaseName, std::string path);

		ASSET_ENCODER_API static void RecursiveSearchDirectoryForExtension(const std::string& localDir, std::vector<FileInfo>& outFilenames, const std::string& desiredExtension);

		ASSET_ENCODER_API void GetAssetStatus(const char* rootFolderName, const std::vector<FileInfo>& fileInfos, std::vector<AssetStatus>& outStatus);

		ASSET_ENCODER_API void WriteUnmodifiedAsset(const AssetStatus& status);

		ASSET_ENCODER_API void WriteUnmodifiedAsset(const char* assetDBName);

		ASSET_ENCODER_API void FlagAsModified();

		std::string m_name;
		std::string m_dbName;
		AssetEncoder::AssetBinaryDatabaseReader* m_dbReader = nullptr;
		AssetEncoder::AssetBinaryDatabaseWriter* m_dbWriter = nullptr;
		bool m_changesDetected = false;
	};
}