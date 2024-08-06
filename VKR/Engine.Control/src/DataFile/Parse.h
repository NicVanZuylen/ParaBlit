#pragma once
#include "Engine.Control/ISettingsParsers.h"
#include "../src/pugixml/pugixml.hpp"
#include <unordered_map>

namespace Ctrl
{
	class DataContainer : public IDataContainer
	{
	public:

		DataContainer() = default;

		~DataContainer() = default;

		CONTROL_API const char* GetStringValue(const char* token) const override;
		CONTROL_API int GetIntegerValue(const char* token) const override;
		CONTROL_API uint32_t GetUIntegerValue(const char* token) const override;
		CONTROL_API bool GetBooleanValue(const char* token) const override;
		CONTROL_API bool HasToken(const char* token) const override;
		CONTROL_API bool HasValue(const char* token) const override;
		CONTROL_API const char* GetRawInputString() const override;

		CONTROL_API void SetStringValue(const char* token, const char* value) override;
		CONTROL_API void SetIntegerValue(const char* token, int value) override;
		CONTROL_API void SetUIntegerValue(const char* token, uint32_t value) override;
		CONTROL_API void SetBooleanValue(const char* token, bool value) override;

	protected:

		friend class SettingsHub;

		std::string m_rawString;
		std::unordered_map<std::string, std::string> m_tokens;
	};

	class CommandLineParser : public ICommandLine, public DataContainer
	{
	public:

		CONTROL_API CommandLineParser(int argc, char** argv);

		CONTROL_API ~CommandLineParser() = default;

		CONTROL_API const IDataContainer* GetData() const override { return this; };
	};

	class ConfigFile : public IConfigFile, public DataContainer
	{
	public:

		CONTROL_API ConfigFile(const char* filePath, EOpenMode openMode);

		CONTROL_API ~ConfigFile() = default;

		CONTROL_API EFileStatus GetStatus() override { return m_status; }
		CONTROL_API IDataContainer* GetData() override { return this; };
		CONTROL_API const IDataContainer* GetData() const override { return this; };

		CONTROL_API bool WriteData() override;

	private:

		CONTROL_API void ReadNode(const pugi::xml_node& node, std::string name);
		CONTROL_API void WriteToken(const std::string& name, const std::string& value, pugi::xml_node& parent, std::unordered_map<std::string, pugi::xml_node>& namespaceNodeMap);

		EFileStatus m_status = EFileStatus::CANT_OPEN;
		std::string m_filePath;
		pugi::xml_node m_rootNode;
		pugi::xml_document m_document;
	};

	class SettingsHub : public ISettingsHub
	{
	public:

		CONTROL_API static SettingsHub* GetOrCreate();
		CONTROL_API static void Destroy();

		CONTROL_API void AddSettings(const IDataContainer* parser, EMergeBehavior mergeBehavior) override;
		CONTROL_API const char* GetStringValue(const char* token) override;
		CONTROL_API int GetIntegerValue(const char* token) override;
		CONTROL_API uint32_t GetUIntegerValue(const char* token) override;
		CONTROL_API bool GetBooleanValue(const char* token) override;
		CONTROL_API bool HasToken(const char* token) override;
		CONTROL_API bool HasValue(const char* token) override;

	private:

		CONTROL_API SettingsHub() = default;

		CONTROL_API ~SettingsHub() = default;

		static SettingsHub* s_instance;
		std::unordered_map<std::string, std::string> m_tokens;
	};
}