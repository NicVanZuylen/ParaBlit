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

		const char* GetStringValue(const char* token) const override;
		int GetIntegerValue(const char* token) const override;
		uint32_t GetUIntegerValue(const char* token) const override;
		bool GetBooleanValue(const char* token) const override;
		bool HasToken(const char* token) const override;
		bool HasValue(const char* token) const override;
		const char* GetRawInputString() const override;

		void SetStringValue(const char* token, const char* value) override;
		void SetIntegerValue(const char* token, int value) override;
		void SetUIntegerValue(const char* token, uint32_t value) override;
		void SetBooleanValue(const char* token, bool value) override;

	protected:

		friend class SettingsHub;

		std::string m_rawString;
		std::unordered_map<std::string, std::string> m_tokens;
	};

	class CommandLineParser : public ICommandLine, public DataContainer
	{
	public:

		CommandLineParser(int argc, char** argv);

		~CommandLineParser() = default;

		const IDataContainer* GetData() const override { return this; };
	};

	class ConfigFile : public IConfigFile, public DataContainer
	{
	public:

		ConfigFile(const char* filePath, EOpenMode openMode);

		~ConfigFile() = default;

		EFileStatus GetStatus() override { return m_status; }
		IDataContainer* GetData() override { return this; };
		const IDataContainer* GetData() const override { return this; };

		bool WriteData() override;

	private:

		void ReadNode(const pugi::xml_node& node, std::string name);
		void WriteToken(const std::string& name, const std::string& value, pugi::xml_node& parent, std::unordered_map<std::string, pugi::xml_node>& namespaceNodeMap);

		EFileStatus m_status = EFileStatus::CANT_OPEN;
		std::string m_filePath;
		pugi::xml_node m_rootNode;
		pugi::xml_document m_document;
	};

	class SettingsHub : public ISettingsHub
	{
	public:

		static SettingsHub* GetOrCreate();
		static void Destroy();

		void AddSettings(const IDataContainer* parser, EMergeBehavior mergeBehavior) override;
		const char* GetStringValue(const char* token) override;
		int GetIntegerValue(const char* token) override;
		uint32_t GetUIntegerValue(const char* token) override;
		bool GetBooleanValue(const char* token) override;
		bool HasToken(const char* token) override;
		bool HasValue(const char* token) override;

	private:

		SettingsHub() = default;

		~SettingsHub() = default;

		static SettingsHub* s_instance;
		std::unordered_map<std::string, std::string> m_tokens;
	};
}