#include "Parse.h"
#include <fstream>
#include <filesystem>

namespace Ctrl
{
	const char* DataContainer::GetStringValue(const char* token) const
	{
		return m_tokens.find(token)->second.c_str();
	}

	int DataContainer::GetIntegerValue(const char* token) const
	{
		return std::atoi(m_tokens.find(token)->second.c_str());
	}

	uint32_t DataContainer::GetUIntegerValue(const char* token) const
	{
		return std::atoi(m_tokens.find(token)->second.c_str());
	}

	bool DataContainer::GetBooleanValue(const char* token) const
	{
		if (strcmp(m_tokens.find(token)->second.c_str(), "True") == 0)
			return true;
		else if (strcmp(m_tokens.find(token)->second.c_str(), "true") == 0)
			return true;
		else if (std::atoi(m_tokens.find(token)->second.c_str()) > 0)
			return true;

		return false;
	}

	bool DataContainer::HasToken(const char* token) const
	{
		return m_tokens.contains(token);
	}

	bool DataContainer::HasValue(const char* token) const
	{
		return m_tokens.find(token)->second.empty() == false;
	}

	const char* DataContainer::GetRawInputString() const
	{
		return m_rawString.c_str();
	}

	void DataContainer::SetStringValue(const char* token, const char* value)
	{
		m_tokens[token] = value;
	}

	void DataContainer::SetIntegerValue(const char* token, int value)
	{
		m_tokens[token] = std::to_string(value);
	}

	void DataContainer::SetUIntegerValue(const char* token, uint32_t value)
	{
		m_tokens[token] = std::to_string(value);
	}

	void DataContainer::SetBooleanValue(const char* token, bool value)
	{
		m_tokens[token] = value ? "True" : "False";
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

	void ConfigFile::ReadNode(const pugi::xml_node& node, std::string name)
	{
		for (const auto& childNode : node.children())
		{
			name.append(childNode.name());
			if (childNode.children().empty())
			{
				// Node is a leaf/value.
				std::string leafName = childNode.name();
				leafName += ".";
				leafName += childNode.first_attribute().name();
				m_tokens[leafName] = childNode.first_attribute().as_string("NULL");
			}
			else
			{
				// Node is a namespace.
				ReadNode(childNode, name + ".");
			}
		}
	}

	ConfigFile::ConfigFile(const char* filePath, EOpenMode openMode)
	{
		std::ios::openmode openFlags;
		switch (openMode)
		{
		case EOpenMode::OPEN_READ_ONLY:
			openFlags = std::ios::in | std::ios::_Nocreate;
		case EOpenMode::OPEN_READ_WRITE:
			openFlags = std::ios::in | std::ios::out;
		default:
			openFlags = std::ios::in;
			break;
		}

		std::fstream fStream(filePath, openFlags);
		if (fStream.good())
		{
			m_status = EFileStatus::OPENED_SUCCESSFULLY;

			m_document.reset();
			m_document.load(fStream);
			fStream.close();

			m_rootNode = m_document.root();
		}
		m_filePath = filePath;

		// Traverse all nodes in the file. Leaf nodes are assumed to be token values.
		ReadNode(m_rootNode, {});
	}

	void ConfigFile::WriteToken(const std::string& name, const std::string& value, pugi::xml_node& parent, std::unordered_map<std::string, pugi::xml_node>& namespaceNodeMap)
	{
		std::string currentNamespace;
		auto firstSeparator = name.find_first_of('.');
		if (firstSeparator != std::string::npos)
		{
			currentNamespace.append(name.begin(), name.begin() + firstSeparator);
		}

		std::string otherNamespaces;
		firstSeparator = name.find_first_of('.');
		auto otherNamespacesBeg = firstSeparator == std::string::npos ? name.begin() : name.begin() + firstSeparator + 1;
		otherNamespaces.append(otherNamespacesBeg, name.end());

		if (currentNamespace.empty() == false)
		{
			namespaceNodeMap.try_emplace(currentNamespace);
			auto namespaceNodeIt = namespaceNodeMap.find(currentNamespace);
			namespaceNodeIt->second = parent.append_child(currentNamespace.c_str());

			WriteToken(otherNamespaces, value, namespaceNodeIt->second, namespaceNodeMap);
		}
		else
		{
			pugi::xml_attribute valueAttrib = parent.append_attribute(name.c_str());
			valueAttrib.set_value(value.c_str());
		}
	}

	bool ConfigFile::WriteData()
	{
		m_document.reset();
		pugi::xml_node newRootNode = m_document.root();

		std::unordered_map<std::string, pugi::xml_node> namespaceNodeMap;
		for (auto& token : m_tokens)
		{
			WriteToken(token.first, token.second, newRootNode, namespaceNodeMap);
		}

		std::filesystem::path directory = m_filePath;
		directory.remove_filename();

		std::filesystem::create_directory(directory);
		std::ofstream outStream(m_filePath, std::ios::beg | std::ios::out);
		if (outStream.good())
		{
			m_status = EFileStatus::OPENED_SUCCESSFULLY;

			m_document.save(outStream);
			outStream.close();

			m_rootNode = m_document.first_child();
			return true;
		}
		
		return false;
	}
	 
	SettingsHub* SettingsHub::s_instance = nullptr;

	SettingsHub* SettingsHub::GetOrCreate()
	{
		if (s_instance == nullptr)
		{
			s_instance = new SettingsHub();
		}
		return s_instance;
	}

	void SettingsHub::Destroy()
	{
		if (s_instance != nullptr)
		{
			delete s_instance;
			s_instance = nullptr;
		}
	}

	void SettingsHub::AddSettings(const IDataContainer* parser, EMergeBehavior mergeBehavior)
	{
		const DataContainer& p = *reinterpret_cast<const DataContainer*>(parser);

		for (const auto& node : p.m_tokens)
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
		if (strcmp(m_tokens[token].c_str(), "True") == 0)
			return true;
		else if (strcmp(m_tokens[token].c_str(), "true") == 0)
			return true;
		else if (std::atoi(m_tokens[token].c_str()) > 0)
			return true;

		return false;
	}

	bool SettingsHub::HasToken(const char* token)
	{
		return m_tokens.contains(token);
	}

	bool SettingsHub::HasValue(const char* token)
	{
		return m_tokens[token].empty() == false;
	}

	ICommandLine* ICommandLine::Create(int argc, char** argv)
	{
		return new CommandLineParser(argc, argv);
	}

	void ICommandLine::Destroy(ICommandLine* file)
	{
		delete reinterpret_cast<CommandLineParser*>(file);
	}

	IConfigFile* IConfigFile::Create(const char* filePath, EOpenMode openMode)
	{
		return new ConfigFile(filePath, openMode);
	}

	void IConfigFile::Destroy(IConfigFile* file)
	{
		delete reinterpret_cast<ConfigFile*>(file);
	}

	ISettingsHub* ISettingsHub::GetOrCreate()
	{
		return SettingsHub::GetOrCreate();
	}

	void ISettingsHub::Destroy()
	{
		SettingsHub::Destroy();
	}
}
