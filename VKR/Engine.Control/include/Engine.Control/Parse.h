#pragma once
#include "ControlLib.h"
#include <unordered_map>

#include "../pugixml/pugixml.hpp"

namespace Ctrl
{
	class Parser
	{
	public:

		CONTROL_API Parser() = default;

		CONTROL_API ~Parser() = default;

		CONTROL_API const char* GetStringValue(const char* token);
		CONTROL_API int GetIntegerValue(const char* token);
		CONTROL_API uint32_t GetUIntegerValue(const char* token);
		CONTROL_API bool GetBooleanValue(const char* token);
		CONTROL_API bool HasToken(const char* token);
		CONTROL_API bool HasValue(const char* token);

		const char* GetRawString() { return m_rawString.c_str(); }

	protected:

		friend class SettingsHub;

		std::string m_rawString;
		std::unordered_map<std::string, std::string> m_tokens;
	};

	class CommandLineParser : public Parser
	{
		CONTROL_API CommandLineParser(int argc, char** argv);

		CONTROL_API ~CommandLineParser() = default;
	};

	class ConfigFile : public Parser
	{
	public:

		CONTROL_API ConfigFile(const char* filePath);

		CONTROL_API ~ConfigFile() = default;

	private:

		pugi::xml_node m_rootNode;
		pugi::xml_document m_document;
	};

	class SettingsHub
	{
	public:

		enum class EMergeBehavior
		{
			OVERWRITE,
			IGNORE_DUPLICATES
		};

		CONTROL_API static SettingsHub& GetOrCreate();
		CONTROL_API static void Destroy();

		CONTROL_API void AddSettings(const Parser& parser, EMergeBehavior mergeBehavior = EMergeBehavior::OVERWRITE);

		CONTROL_API const char* GetStringValue(const char* token);
		CONTROL_API int GetIntegerValue(const char* token);
		CONTROL_API uint32_t GetUIntegerValue(const char* token);
		CONTROL_API bool GetBooleanValue(const char* token);
		CONTROL_API bool HasToken(const char* token);
		CONTROL_API bool HasValue(const char* token);

	private:

		SettingsHub() = default;

		~SettingsHub() = default;

		static SettingsHub* s_instance;
		std::unordered_map<std::string, std::string> m_tokens;
	};
}