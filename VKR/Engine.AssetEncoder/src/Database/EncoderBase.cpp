#include "Engine.AssetEncoder/EncoderBase.h"
#include <Engine.Control/ISettingsParsers.h>

namespace AssetEncoder
{
	EncoderBase::EncoderBase(const char* name, const char* dbName, const char* assetDirectory)
	{
		m_name = name;
		m_dbName = name;
		m_forceBuild = Ctrl::ISettingsHub::GetOrCreate()->GetBooleanValue("Build.ForceBuildAll");

		std::string databasePath = assetDirectory;
		databasePath += "build/";
		databasePath += dbName;

		m_dbReader = new AssetEncoder::AssetBinaryDatabaseReader(databasePath.c_str());
		m_dbWriter = new AssetEncoder::AssetBinaryDatabaseWriter(databasePath.c_str());
	}

	EncoderBase::~EncoderBase()
	{
		if(m_changesDetected)
			m_dbWriter->WriteDatabase();
		else
			printf("%s: Database is already up-to-date.\n", m_name.c_str());

		delete m_dbReader;
		delete m_dbWriter;
	}

	std::string EncoderBase::ConvertPathToDBFormat(const char* databaseName, std::string path)
	{
		// Remove extension.
		path.erase(path.find_last_of("."), path.size());

		// Trim path before database name.
		size_t dbNameLocation = path.find(databaseName);
		path.erase(0, dbNameLocation);

		// Replace back slashes with forward slashes.
		for (auto& c : path)
		{
			if (c == '\\')
				c = '/';
		}

		return path;
	}

	void EncoderBase::RecursiveSearchDirectoryForExtension(const std::string& localDir, std::vector<FileInfo>& outFilenames, const std::string& desiredExtension)
	{
		std::filesystem::path workingDir = std::filesystem::current_path().parent_path();
		std::filesystem::path searchDir = workingDir;
		searchDir.append(localDir);

		if (std::filesystem::is_directory(searchDir))
		{
			for (const auto& entry : std::filesystem::recursive_directory_iterator(searchDir))
			{
				bool validExtension = desiredExtension.empty() || (desiredExtension == entry.path().extension());
				if (entry.is_regular_file() && validExtension)
				{
					FileInfo& fileInfo = outFilenames.emplace_back();
					fileInfo.m_fileName = entry.path().string();
					fileInfo.m_lastModifiedTime = std::chrono::duration_cast<std::chrono::seconds>(entry.last_write_time().time_since_epoch()).count();
				}
			}
		}
		else
		{
			printf("%ws - Directory not found.\n", searchDir.c_str());
		}
	}

	void EncoderBase::GetAssetStatus(const char* rootFolderName, const std::vector<FileInfo>& fileInfos, std::vector<AssetStatus>& outStatus)
	{
		for (auto it = fileInfos.begin(); it != fileInfos.end(); ++it)
		{
			AssetStatus& status = outStatus.emplace_back();

			// Assets may have an associated XML file with build properties inside. If this properties file is modified the asset needs to be rebuilt.
			std::filesystem::path propertyFilePath = it->m_fileName;
			propertyFilePath.replace_extension("xml");

			std::filesystem::directory_entry propertyFileEntry(propertyFilePath);
			size_t propertyLastModifiedTime = 0;
			if (propertyFileEntry.exists())
			{
				propertyLastModifiedTime = std::chrono::duration_cast<std::chrono::seconds>(propertyFileEntry.last_write_time().time_since_epoch()).count();
			}
			size_t mostRecentModifiedTime = std::max(propertyLastModifiedTime, it->m_lastModifiedTime);


			status.m_fullPath = it->m_fileName;
			status.m_propertyFilePath = propertyFilePath.string();
			status.m_dbPath = ConvertPathToDBFormat(rootFolderName, it->m_fileName);
			status.m_extension = it->m_fileName.substr(it->m_fileName.find_last_of('.'));
			AssetHandle handle(status.m_dbPath.c_str());
			status.m_info = m_dbReader->GetAssetInfo(handle);
			status.m_outdated = status.m_info.m_binarySize == 0 || status.m_info.m_dateModified < mostRecentModifiedTime || m_forceBuild;
			status.m_hasPropertyFile = propertyFileEntry.exists();
			status.m_lastModifiedTime = mostRecentModifiedTime;
		}
	}

	void EncoderBase::WriteUnmodifiedAsset(const AssetStatus& status)
	{
		char* userData = nullptr;
		void* storage = m_dbWriter->AllocateAsset(status.m_dbPath.c_str(), status.m_info.m_userDataSize, status.m_info.m_binarySize, status.m_info.m_dateModified, &userData);
		m_dbReader->GetAssetUserData(status.m_info, userData);

		AssetHandle handle(status.m_dbPath.c_str());
		m_dbReader->GetAssetBinary(handle, storage);
	}

	void EncoderBase::WriteUnmodifiedAsset(const char* assetDBName)
	{
		AssetHandle handle(assetDBName);
		auto meta = m_dbReader->GetAssetInfo(handle);

		char* userData = nullptr;
		void* storage = m_dbWriter->AllocateAsset(assetDBName, meta.m_userDataSize, meta.m_binarySize, meta.m_dateModified, &userData);
		m_dbReader->GetAssetUserData(meta, userData);
		m_dbReader->GetAssetBinary(handle , storage);
	}

	void EncoderBase::FlagAsModified()
	{
		m_changesDetected = true;
	}
}