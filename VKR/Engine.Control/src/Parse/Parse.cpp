#include "Engine.Control/Parse.h"

#include <fstream>

namespace Ctrl
{
	const char* Parser::GetStringValue(const char* token)
	{
		return m_tokens[token].c_str();
	}

	int Parser::GetIntegerValue(const char* token)
	{
		return std::atoi(m_tokens[token].c_str());
	}

	uint32_t Parser::GetUIntegerValue(const char* token)
	{
		return std::atoi(m_tokens[token].c_str());
	}

	bool Parser::GetBooleanValue(const char* token)
	{
		return std::atoi(m_tokens[token].c_str()) > 0;
	}

	bool Parser::HasToken(const char* token)
	{
		return m_tokens.contains(token);
	}

	bool Parser::HasValue(const char* token)
	{
		return m_tokens[token].empty() == false;
	}

	CommandLineParser::CommandLineParser(int argc, char** argv)
	{
		std::string mostRecentToken;
		for (int i = 1; i < argc; ++i)
		{
			const char* str = argv[i];

			if (str[0] == '-') // String is a token.
			{
				mostRecentToken = &str[1];

				m_tokens.try_emplace(mostRecentToken);
			}
			else if (mostRecentToken.empty() == false) // String is assumed to be a value of most recent token.
			{
				m_tokens[mostRecentToken] = str;
			}
		}
	}

	ConfigFile::ConfigFile(const char* filePath)
	{
		std::ifstream inStream(filePath, std::ios::in);
		if (inStream.good())
		{
			m_document.reset();
			m_document.load(inStream);
			inStream.close();

			m_rootNode = m_document.first_child();
		}
	}

	SettingsHub* SettingsHub::s_instance = nullptr;

	SettingsHub& SettingsHub::GetOrCreate()
	{
		if (s_instance == nullptr)
		{
			s_instance = new SettingsHub();
		}
		return *s_instance;
	}

	void SettingsHub::Destroy()
	{
		if (s_instance != nullptr)
		{
			delete s_instance;
			s_instance = nullptr;
		}
	}

	void SettingsHub::AddSettings(const Parser& parser, EMergeBehavior mergeBehavior)
	{
		for (const auto& node : parser.m_tokens)
		{
			if (mergeBehavior == EMergeBehavior::OVERWRITE)
			{
				m_tokens.try_emplace(node.first);
				m_tokens[node.first] = node.second;
			}
			else if (mergeBehavior == EMergeBehavior::IGNORE_DUPLICATES)
			{
				if (m_tokens.contains(node.first) == false)
				{
					m_tokens.insert(node);
				}
			}
		}
	}

	const char* SettingsHub::GetStringValue(const char* token)
	{
		return m_tokens[token].c_str();
	}

	int SettingsHub::GetIntegerValue(const char* token)
	{
		return std::atoi(m_tokens[token].c_str());
	}

	uint32_t SettingsHub::GetUIntegerValue(const char* token)
	{
		return std::atoi(m_tokens[token].c_str());
	}

	bool SettingsHub::GetBooleanValue(const char* token)
	{
		return std::atoi(m_tokens[token].c_str()) > 0;
	}

	bool SettingsHub::HasToken(const char* token)
	{
		return m_tokens.contains(token);
	}

	bool SettingsHub::HasValue(const char* token)
	{
		return m_tokens[token].empty() == false;
	}
}
