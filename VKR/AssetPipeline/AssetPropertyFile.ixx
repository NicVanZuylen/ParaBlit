module;

#include "pugixml.hpp"
#include <fstream>

export module AssetPropertyFile;

export namespace AssetPipeline
{
	class AssetPropertyFile
	{
	public:

		AssetPropertyFile(const char* fileName, const char* rootNodeName)
		{
			m_fileName = fileName;
			m_rootNodeName = rootNodeName;
			m_rootNode = m_document.append_child(rootNodeName);
		}

		bool Load()
		{
			std::ifstream inStream(m_fileName.c_str(), std::ios::in);
			if (inStream.good())
			{
				m_document.reset();
				m_document.load(inStream);
				inStream.close();

				m_rootNode = m_document.first_child();
				return true;
			}

			return false;
		}

		bool Save()
		{
			std::ofstream outStream(m_fileName.c_str(), std::ios::beg);
			if (outStream.good())
			{
				m_document.save(outStream);
				outStream.close();
				return true;
			}

			return false;
		}

		void SetIntegerProperty(const char* label, int64_t value)
		{
			auto node = m_rootNode.child(label);
			if (node.type() == pugi::node_null)
			{
				m_rootNode.append_child(label).append_attribute("value").set_value(value);
				return;
			}
			node.first_attribute().set_value(value);
		}

		void SetBooleanProperty(const char* label, bool value)
		{
			auto node = m_rootNode.child(label);
			if (node.type() == pugi::node_null)
			{
				m_rootNode.append_child(label).append_attribute("value").set_value(value);
				return;
			}
			node.first_attribute().set_value(value);
		}

		void SetStringProperty(const char* label, const char* value)
		{
			auto node = m_rootNode.child(label);
			if (node.type() == pugi::node_null)
			{
				m_rootNode.append_child(label).append_attribute("value").set_value(value);
				return;
			}
			node.first_attribute().set_value(value);
		}

		int64_t GetIntegerProperty(const char* label, int64_t def) const
		{
			auto node = m_rootNode.child(label);
			if (node.type() != pugi::node_null)
				return node.first_attribute().as_llong();
			return def;
		}

		bool GetBooleanProperty(const char* label, bool def) const
		{
			auto node = m_rootNode.child(label);
			if (node.type() != pugi::node_null)
				return node.first_attribute().as_bool();
			return def;
		}

		const char* GetStringProperty(const char* label, const char* def) const
		{
			auto node = m_rootNode.child(label);
			if (node.type() != pugi::node_null)
				return node.first_attribute().as_string();
			return def;
		}

		void RemoveProperty(const char* label)
		{
			m_rootNode.remove_child(label);
		}

		void Clear()
		{
			m_document.reset();
			m_rootNode = m_document.append_child(m_rootNodeName.c_str());
		}

	private:

		std::string m_fileName;
		std::string m_rootNodeName;
		pugi::xml_node m_rootNode;
		pugi::xml_document m_document;
	};
}