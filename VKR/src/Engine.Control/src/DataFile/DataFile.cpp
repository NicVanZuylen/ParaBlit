#include "DataFile.h"
#include "Engine.Control/GUID.h"
#include <fstream>
#include <cassert>
#include <filesystem>

namespace Ctrl
{
	DataNode::DataNode(const char* type)
	{
		m_type = type != nullptr ? type : "";
	}

	DataNode::DataNode(const pugi::xml_node& node)
	{
		InitializeWithNode(node);
	}

	DataNode::~DataNode()
	{
		for (auto& attribute : m_attributes)
		{
			FreeAttributeData(attribute.second);
		}
	}

	void DataNode::InitializeWithNode(const pugi::xml_node& node)
	{
		for (auto& attrib : node.attributes())
		{
			if (strcmp(attrib.name(), "TYPE") == 0)
			{
				m_type = attrib.value();
			}
			else if(strcmp(attrib.name(), "GUID") == 0)
			{
				SetSelfGUID(attrib.value());
			}
		}

		for (auto& child : node.children())
		{
			ReadChildren(child);
		}
	}

	void DataNode::WriteData(pugi::xml_node& parent, const char* name, bool includeSelf)
	{
		pugi::xml_node thisNode = includeSelf ? parent.append_child(name) : parent;
		if (m_type.empty() == false)
		{
			thisNode.append_attribute("TYPE").set_value(m_type.c_str());
		}
		if (m_guid.IsValid() == true)
		{
			thisNode.append_attribute("GUID").set_value(m_guid.AsCString());
		}

		for (auto& a : m_attributes)
		{
			std::stringstream valueStr;

			Attribute& attribute = a.second;
			switch (attribute.m_type)
			{
				case EAttributeType::INT:
				{
					pugi::xml_node newNode = thisNode.append_child(a.first.c_str());
					newNode.append_attribute("TYPE").set_value("int");

					StringifyArray<int>(valueStr, attribute.m_data.Data(), attribute.m_data.Count());
					newNode.append_attribute("VALUE").set_value(valueStr.str().c_str());
					break;
				}
				case EAttributeType::FLOAT:
				{
					pugi::xml_node newNode = thisNode.append_child(a.first.c_str());
					newNode.append_attribute("TYPE").set_value("float");

					StringifyArray<float>(valueStr, attribute.m_data.Data(), attribute.m_data.Count());
					newNode.append_attribute("VALUE").set_value(valueStr.str().c_str());
					break;
				}
				case EAttributeType::DOUBLE:
				{
					pugi::xml_node newNode = thisNode.append_child(a.first.c_str());
					newNode.append_attribute("TYPE").set_value("double");

					StringifyArray<double>(valueStr, attribute.m_data.Data(), attribute.m_data.Count());
					newNode.append_attribute("VALUE").set_value(valueStr.str().c_str());
					break;
				}
				case EAttributeType::BOOL:
				{
					pugi::xml_node newNode = thisNode.append_child(a.first.c_str());
					newNode.append_attribute("TYPE").set_value("bool");

					StringifyArray<bool>(valueStr, attribute.m_data.Data(), attribute.m_data.Count());
					newNode.append_attribute("VALUE").set_value(valueStr.str().c_str());
					break;
				}
				case EAttributeType::STRING:
				{
					pugi::xml_node newNode = thisNode.append_child(a.first.c_str());
					newNode.append_attribute("TYPE").set_value("string");

					CLib::Vector<const char*> stringVec;
					GetString(a.first.c_str(), stringVec);
					StringifyStringArray(valueStr, stringVec);
					newNode.append_attribute("VALUE").set_value(valueStr.str().c_str());
					break;
				}
				case EAttributeType::GUID:
				{
					pugi::xml_node newNode = thisNode.append_child(a.first.c_str());
					newNode.append_attribute("TYPE").set_value("GUID");

					uint32_t guidCount = 0;
					const GUID* guids = GetGUID(a.first.c_str(), guidCount);
					StringifyGUIDArray(valueStr, guids, guidCount);
					newNode.append_attribute("VALUE").set_value(valueStr.str().c_str());
					break;
				}
				case EAttributeType::DATA_NODE:
				{
					uint32_t dataNodeCount = attribute.m_data.Count() / sizeof(DataNode*);
					for (uint32_t i = 0; i < dataNodeCount; ++i)
					{
						DataNode** n = reinterpret_cast<DataNode**>(attribute.m_data.Data()) + i;
						(*n)->WriteData(thisNode, a.first.c_str());
					}
					break;
				}
				default:
					break;
			}
		}
	}

	void DataNode::PeekAttributes(std::function<void(const std::string&, const Attribute&)> peekFunc) const
	{
		for (auto& attrib : m_attributes)
			peekFunc(attrib.first, attrib.second);
	}

	const int* DataNode::GetInteger(const char* name, uint32_t& outCount) const
	{
		auto it = m_attributes.find(name);
		if (it != m_attributes.end())
		{
			const Attribute& attrib = it->second;
			assert(attrib.m_type == EAttributeType::INT);
			outCount = attrib.m_data.Count() / sizeof(int);
			return reinterpret_cast<const int*>(it->second.m_data.Data());
		}

		return nullptr;
	}

	int DataNode::GetInteger(const char* name, int fallbackValue) const
	{
		uint32_t count = 0;
		const int* val = GetInteger(name, count);
		return val ? *val : fallbackValue;
	}

	const float* DataNode::GetFloat(const char* name, uint32_t& outCount) const
	{
		auto it = m_attributes.find(name);
		if (it != m_attributes.end())
		{
			const Attribute& attrib = it->second;
			assert(attrib.m_type == EAttributeType::FLOAT);
			outCount = attrib.m_data.Count() / sizeof(float);
			return reinterpret_cast<const float*>(it->second.m_data.Data());
		}
		
		return nullptr;
	}

	float DataNode::GetFloat(const char* name, float fallbackValue) const
	{
		uint32_t count = 0;
		const float *val = GetFloat(name, count);
		return val ? *val : fallbackValue;
	}

	const double* DataNode::GetDouble(const char* name, uint32_t& outCount) const
	{
		auto it = m_attributes.find(name);
		if (it != m_attributes.end())
		{
			const Attribute& attrib = it->second;
			assert(attrib.m_type == EAttributeType::DOUBLE);
			outCount = attrib.m_data.Count() / sizeof(double);
			return reinterpret_cast<const double*>(it->second.m_data.Data());
		}

		return nullptr;
	}

	double DataNode::GetDouble(const char* name, double fallbackValue) const
	{
		uint32_t count = 0;
		const double* val = GetDouble(name, count);
		return val ? *val : fallbackValue;
	}

	const bool* DataNode::GetBool(const char* name, uint32_t& outCount) const
	{
		auto it = m_attributes.find(name);
		if (it != m_attributes.end())
		{
			const Attribute& attrib = it->second;
			assert(attrib.m_type == EAttributeType::BOOL);
			outCount = attrib.m_data.Count() / sizeof(bool);
			return reinterpret_cast<const bool*>(it->second.m_data.Data());
		}

		return nullptr;
	}

	bool DataNode::GetBool(const char* name, bool fallbackValue) const
	{
		uint32_t count = 0;
		const bool* val = GetBool(name, count);
		return val ? *val : fallbackValue;
	}

	void DataNode::GetString(const char* name, CLib::Vector<const char*>& outStr) const
	{
		auto it = m_attributes.find(name);
		if (it != m_attributes.end())
		{
			const Attribute& attrib = it->second;
			assert(attrib.m_type == EAttributeType::STRING);
			const char* currentStr = reinterpret_cast<const char*>(attrib.m_data.Data());
			uint32_t length = strlen(currentStr);
			while (*currentStr != '\0')
			{
				length = strlen(currentStr);
				if (length > 0)
				{
					outStr.PushBack(currentStr);
				}
				
				currentStr += length + 1;
			}
		}
	}

	const char* DataNode::GetString(const char* name, const char* fallbackValue) const
	{
		CLib::Vector<const char*> val;
		GetString(name, val);
		return val.Front() ? val.Front() : fallbackValue;
	}

	IDataNode** DataNode::GetDataNode(const char* name, uint32_t& outCount)
	{
		auto it = m_attributes.find(name);
		if (it != m_attributes.end())
		{
			Attribute& attrib = it->second;
			assert(attrib.m_type == EAttributeType::DATA_NODE);
			
			outCount = attrib.m_data.Count() / sizeof(DataNode*);
			return reinterpret_cast<IDataNode**>(attrib.m_data.Data());
		}

		return nullptr;
	}

	const IDataNode* const* DataNode::GetDataNode(const char* name, uint32_t& outCount) const
	{
		auto it = m_attributes.find(name);
		if (it != m_attributes.end())
		{
			const Attribute& attrib = it->second;
			assert(attrib.m_type == EAttributeType::DATA_NODE);

			outCount = attrib.m_data.Count() / sizeof(DataNode*);
			return reinterpret_cast<const IDataNode* const*>(attrib.m_data.Data());
		}

		return nullptr;
	}

	IDataNode* DataNode::GetDataNode(const char* name)
	{
		auto it = m_attributes.find(name);
		if (it != m_attributes.end())
		{
			assert(it->second.m_type == EAttributeType::DATA_NODE);
			return *reinterpret_cast<IDataNode**>(it->second.m_data.Data());
		}

		return nullptr;
	}

	const IDataNode* DataNode::GetDataNode(const char* name) const
	{
		return GetDataNode(name);
	}

	const GUID* DataNode::GetGUID(const char* name, uint32_t& outCount) const
	{
		auto it = m_attributes.find(name);
		if (it != m_attributes.end())
		{
			const Attribute& attrib = it->second;
			assert(attrib.m_type == EAttributeType::GUID);
			outCount = attrib.m_data.Count() / sizeof(GUID);
			return reinterpret_cast<const GUID*>(it->second.m_data.Data());
		}

		return nullptr;
	}

	const GUID* DataNode::GetGUID(const char* name) const
	{
		uint32_t count = 0;
		return GetGUID(name, count);
	}

	void DataNode::SetInteger(const char* name, int* value, uint32_t count)
	{
		std::string nameStr = name;

		auto it = m_attributes.find(nameStr);
		if (it != m_attributes.end())
		{
			Attribute& attrib = it->second;
			assert(attrib.m_type == EAttributeType::INT);
			attrib.m_data.SetCount(sizeof(int) * count);
			std::memcpy(attrib.m_data.Data(), value, sizeof(int) * count);
		}

		Attribute& newAttribute = m_attributes.emplace(std::pair<std::string, Attribute>(nameStr, {})).first->second;
		newAttribute.m_type = EAttributeType::INT;
		newAttribute.m_data.SetCount(sizeof(int) * count);

		std::memcpy(newAttribute.m_data.Data(), value, sizeof(int) * count);
	}

	void DataNode::SetInteger(const char* name, int value)
	{
		SetInteger(name, &value);
	}

	void DataNode::SetFloat(const char* name, float* value, uint32_t count)
	{
		std::string nameStr = name;

		auto it = m_attributes.find(nameStr);
		if (it != m_attributes.end())
		{
			Attribute& attrib = it->second;
			assert(attrib.m_type == EAttributeType::FLOAT);
			attrib.m_data.SetCount(sizeof(float) * count);
			std::memcpy(attrib.m_data.Data(), value, sizeof(float) * count);
		}

		Attribute& newAttribute = m_attributes.emplace(std::pair<std::string, Attribute>(nameStr, {})).first->second;
		newAttribute.m_type = EAttributeType::FLOAT;
		newAttribute.m_data.SetCount(sizeof(float) * count);

		std::memcpy(newAttribute.m_data.Data(), value, sizeof(float) * count);
	}

	void DataNode::SetFloat(const char* name, float value)
	{
		SetFloat(name, &value);
	}

	void DataNode::SetDouble(const char* name, double* value, uint32_t count)
	{
		std::string nameStr = name;

		auto it = m_attributes.find(nameStr);
		if (it != m_attributes.end())
		{
			Attribute& attrib = it->second;
			assert(attrib.m_type == EAttributeType::DOUBLE);
			attrib.m_data.SetCount(sizeof(double) * count);
			std::memcpy(attrib.m_data.Data(), value, sizeof(double) * count);
		}

		Attribute& newAttribute = m_attributes.emplace(std::pair<std::string, Attribute>(nameStr, {})).first->second;
		newAttribute.m_type = EAttributeType::DOUBLE;
		newAttribute.m_data.SetCount(sizeof(double) * count);

		std::memcpy(newAttribute.m_data.Data(), value, sizeof(double) * count);
	}

	void DataNode::SetDouble(const char* name, double value)
	{
		SetDouble(name, &value);
	}

	void DataNode::SetBool(const char* name, bool* value, uint32_t count)
	{
		std::string nameStr = name;

		auto it = m_attributes.find(nameStr);
		if (it != m_attributes.end())
		{
			Attribute& attrib = it->second;
			assert(attrib.m_type == EAttributeType::BOOL);
			attrib.m_data.SetCount(sizeof(bool) * count);
			std::memcpy(attrib.m_data.Data(), value, sizeof(bool) * count);
		}

		Attribute& newAttribute = m_attributes.emplace(std::pair<std::string, Attribute>(nameStr, {})).first->second;
		newAttribute.m_type = EAttributeType::BOOL;
		newAttribute.m_data.SetCount(sizeof(bool) * count);

		std::memcpy(newAttribute.m_data.Data(), value, sizeof(bool) * count);
	}

	void DataNode::SetBool(const char* name, bool value)
	{
		SetBool(name, &value);
	}

	void DataNode::SetString(const char* name, const char** value, uint32_t count)
	{
		std::string nameStr = name;

		auto it = m_attributes.find(nameStr);
		if (it != m_attributes.end())
		{
			Attribute& attrib = it->second;
			assert(attrib.m_type == EAttributeType::STRING);
			
			uint32_t totalCharCount = 0;
			for (uint32_t i = 0; i < count; ++i)
			{
				uint32_t charCount = strlen(value[i]) + 1;
				uint32_t pos = totalCharCount;
				totalCharCount += charCount;

				attrib.m_data.SetCount(totalCharCount);
				std::memcpy(attrib.m_data.Data() + pos, value[i], charCount);
			}
			attrib.m_data.PushBack(uint8_t('\0')); // Push another null terminator to terminate array with a double null terminator.
		}

		Attribute& newAttribute = m_attributes.emplace(std::pair<std::string, Attribute>(nameStr, {})).first->second;
		newAttribute.m_type = EAttributeType::STRING;
		
		uint32_t totalCharCount = 0;
		for (uint32_t i = 0; i < count; ++i)
		{
			uint32_t charCount = strlen(value[i]) + 1;
			uint32_t pos = totalCharCount;
			totalCharCount += charCount;

			newAttribute.m_data.SetCount(totalCharCount);
			std::memcpy(newAttribute.m_data.Data() + pos, value[i], charCount);
		}
		newAttribute.m_data.PushBack(uint8_t('\0')); // Push another null terminator to terminate array with a double null terminator.
	}

	void DataNode::SetString(const char* name, const char* value)
	{
		SetString(name, &value);
	}

	void DataNode::SetGUID(const char* name, const GUID* value, uint32_t count)
	{
		std::string nameStr = name;

		auto it = m_attributes.find(nameStr);
		if (it != m_attributes.end())
		{
			Attribute& attrib = it->second;
			assert(attrib.m_type == EAttributeType::GUID);
			attrib.m_data.SetCount(count * sizeof(GUID));

			for (uint32_t i = 0; i < count; ++i)
			{
				uint32_t pos = i * sizeof(GUID);
				GUID* val = reinterpret_cast<GUID*>(attrib.m_data.Data() + pos);
				*val = value[i];
			}
		}

		Attribute& newAttribute = m_attributes.emplace(std::pair<std::string, Attribute>(nameStr, {})).first->second;
		newAttribute.m_type = EAttributeType::GUID;
		newAttribute.m_data.SetCount(count * sizeof(GUID));

		for (uint32_t i = 0; i < count; ++i)
		{
			uint32_t pos = i * sizeof(GUID);
			GUID* val = reinterpret_cast<GUID*>(newAttribute.m_data.Data() + pos);
			*val = value[i];
		}
	}

	void DataNode::SetGUID(const char* name, const GUID& value)
	{
		SetGUID(name, &value);
	}

	void DataNode::RemoveAttributeOrNode(const char* name, uint32_t index)
	{
		auto it = m_attributes.find(name);
		if (it != m_attributes.end())
		{
			Attribute& attribute = it->second;
			FreeAttributeData(attribute, index);
			if (attribute.m_data.Count() == 0)
			{
				m_attributes.erase(it);
			}
		}
	}

	void DataNode::SetSelfGUID(const GUID& value)
	{
		m_guid = value;
	}

	const GUID& DataNode::GetSelfGUID() const
	{
		return m_guid;
	}

	const char* DataNode::GetTypeName() const
	{
		return m_type.empty() ? nullptr : m_type.c_str();
	}

	IDataNode* DataNode::AddDataNode(const char* name, const pugi::xml_node* nodeData, const char* type)
	{
		std::string nameStr = name;
		auto it = m_attributes.find(nameStr);
		if (it != m_attributes.end())
		{
			Attribute& attrib = it->second;
			uint32_t pos = attrib.m_data.Count();
			attrib.m_data.SetCount(attrib.m_data.Count() + sizeof(DataNode*));
			assert(attrib.m_type == EAttributeType::DATA_NODE);

			uint8_t* data = attrib.m_data.Data();
			DataNode* newNode = nullptr;
			if (nodeData != nullptr)
			{
				newNode = *reinterpret_cast<DataNode**>(&data[pos]) = s_dataAllocator.Alloc<DataNode>(*nodeData);
			}
			else
			{
				newNode = *reinterpret_cast<DataNode**>(&data[pos]) = s_dataAllocator.Alloc<DataNode>(type);
			}
			return newNode;
		}

		Attribute& newAttribute = m_attributes.emplace(std::pair<std::string, Attribute>(nameStr, {})).first->second;
		newAttribute.m_type = EAttributeType::DATA_NODE;
		newAttribute.m_data.SetCount(sizeof(DataNode*));

		DataNode*& nodePtr = *reinterpret_cast<DataNode**>(newAttribute.m_data.Data());
		if (nodeData != nullptr)
		{
			nodePtr = s_dataAllocator.Alloc<DataNode>(*nodeData);
		}
		else
		{
			nodePtr = s_dataAllocator.Alloc<DataNode>(type);
		}

		return nodePtr;
	}

	IDataNode* DataNode::AddDataNode(const char* name, const char* type)
	{
		return AddDataNode(name, nullptr, type);
	}

	IDataNode* DataNode::GetOrAddDataNode(const char* name, const char* type)
	{
		std::string nameStr = name;
		auto it = m_attributes.find(nameStr);
		if (it != m_attributes.end())
		{
			Attribute& attrib = it->second;
			assert(attrib.m_type == EAttributeType::DATA_NODE);
			
			return *reinterpret_cast<DataNode**>(attrib.m_data.Data());
		}

		return AddDataNode(name, type);
	}

	void DataNode::ReadChildren(pugi::xml_node& node)
	{
		if (node.attributes().empty()) // Node is a typeless data node.
		{
			AddDataNode(node.name(), &node);
			return;
		}
		std::string nameStr = node.name();

		pugi::xml_attribute typeAttrib;
		bool validType = false;
		pugi::xml_attribute valueAttrib;
		bool validValue = false;
		for (auto& attrib : node.attributes())
		{
			if (strcmp(attrib.name(), "TYPE") == 0)
			{
				typeAttrib = attrib;
				validType = true;
			}
			else if (strcmp(attrib.name(), "VALUE") == 0)
			{
				valueAttrib = attrib;
				validValue = true;
			}
		}

		if (validType && validValue)
		{
			if (strcmp(typeAttrib.value(), "int") == 0)
			{
				CLib::Vector<int> valueArr;
				std::stringstream ss;
				ss << valueAttrib.value();

				for (int i; ss >> i;)
				{
					valueArr.PushBack(i);
					if (ss.peek() == ',' || ss.peek() == ' ')
					{
						ss.ignore();
					}
				}
				SetInteger(node.name(), valueArr.Data(), valueArr.Count());
			}
			else if (strcmp(typeAttrib.value(), "float") == 0)
			{
				CLib::Vector<float> valueArr;
				std::stringstream ss;
				ss << valueAttrib.value();

				for (float f; ss >> f;)
				{
					valueArr.PushBack(f);
					if (ss.peek() == ',' || ss.peek() == ' ')
					{
						ss.ignore();
					}
				}

				SetFloat(node.name(), valueArr.Data(), valueArr.Count());
			}
			else if (strcmp(typeAttrib.value(), "double") == 0)
			{
				CLib::Vector<double> valueArr;
				std::stringstream ss;
				ss << valueAttrib.value();

				for (double f; ss >> f;)
				{
					valueArr.PushBack(f);
					if (ss.peek() == ',' || ss.peek() == ' ')
					{
						ss.ignore();
					}
				}

				SetDouble(node.name(), valueArr.Data(), valueArr.Count());
			}
			else if (strcmp(typeAttrib.value(), "bool") == 0)
			{
				CLib::Vector<bool> valueArr;
				std::stringstream ss;
				ss << valueAttrib.value();

				for (bool b; ss >> b;)
				{
					valueArr.PushBack(b);
					if (ss.peek() == ',' || ss.peek() == ' ')
					{
						ss.ignore();
					}
				}

				SetBool(node.name(), valueArr.Data(), valueArr.Count());
			}
			else if (strcmp(typeAttrib.value(), "string") == 0)
			{
				std::vector<std::string> stringVec;
				std::string valueStr = valueAttrib.value();
				size_t pos = 0;
				while (pos != std::string::npos)
				{
					size_t comma = valueStr.find_first_of(',', pos);
					size_t lastSpace = valueStr.find_first_not_of(' ', comma + 1);
					size_t end = lastSpace != std::string::npos ? std::max<size_t>(comma, lastSpace) : comma;

					stringVec.push_back(valueStr.substr(pos, comma - pos));

					pos = end;
				}

				CLib::Vector<const char*> cStringArr;
				for (std::string& s : stringVec)
					cStringArr.PushBack(s.c_str());

				SetString(node.name(), cStringArr.Data(), cStringArr.Count());
			}
			else if (strcmp(typeAttrib.value(), "GUID") == 0)
			{
				CLib::Vector<GUID> guidArr;
				std::string valueStr = valueAttrib.value();
				size_t pos = 0;
				while (pos != std::string::npos)
				{
					const char* substr = valueStr.data() + pos;
					guidArr.PushBack(substr);
					size_t comma = valueStr.find_first_of(',', pos);
					size_t lastSpace = valueStr.find_first_not_of(' ', comma + 1);
					size_t end = lastSpace != std::string::npos ? std::max<size_t>(comma, lastSpace) : comma;
					pos = end;
				}

				SetGUID(node.name(), guidArr.Data(), guidArr.Count());
			}
		}
		else // Unrecognized types are assumed to be classes/structs and will be read as a data node.
		{
			AddDataNode(node.name(), &node);
			return;
		}
	}

	void DataNode::FreeAttributeData(Attribute& attribute, uint32_t index)
	{
		switch (attribute.m_type)
		{
			case EAttributeType::DATA_NODE:
			{
				uint32_t count = attribute.m_data.Count() / sizeof(DataNode*);
				DataNode** dataNodes = reinterpret_cast<DataNode**>(attribute.m_data.Data());
				if (index < count)
				{
					// Remove a single node at the index.
					s_dataAllocator.Free(dataNodes[index]);
					if (index < count - 1)
					{
						size_t copyCount = count - (index + 1);
						std::memcpy(&dataNodes[index], &dataNodes[index + 1], sizeof(DataNode*) * copyCount);
					}
					attribute.m_data.SetCount(attribute.m_data.Count() - sizeof(DataNode*));
				}
				else
				{
					// Delete all data of this attribute.
					for (uint32_t i = 0; i < count; ++i)
					{
						s_dataAllocator.Free(dataNodes[i]);
					}
					attribute.m_data.Clear();
				}
				break;
			}
			default:
				assert(index == ~uint32_t(0) && "NOT IMPLEMENTED!");
				break; // Other attribute types do not dynamically allocate data (aside from the attribute's data vector which frees its memory upon destruction).
		}
	}

	void DataNode::WriteIntArray(std::stringstream& outStr, int* data, uint32_t count)
	{
		for (uint32_t i = 0; i < count; ++i)
		{
			if (i != 0)
			{
				outStr << ", ";
			}
			outStr << data[i];
		}
	}

	void DataNode::StringifyStringArray(std::stringstream& outStr, CLib::Vector<const char*>& strArray)
	{
		for (uint32_t i = 0; i < strArray.Count(); ++i)
		{
			if (i != 0)
			{
				outStr << ", ";
			}
			outStr << strArray[i];
		}
	}

	void DataNode::StringifyGUIDArray(std::stringstream& outStr, const GUID* guidArray, uint32_t count)
	{
		for (uint32_t i = 0; i < count; ++i)
		{
			if (i != 0)
			{
				outStr << ", ";
			}
			outStr.write(guidArray[i], GUIDLength);
			outStr.seekp(0, std::ios::end);
		}
	}

	DataFile::~DataFile()
	{
		Close();
	}

	IDataFile::EFileStatus DataFile::Open(const char* fileName, EOpenMode openMode, bool allowFail)
	{
		std::ios::openmode openFlags;
		switch (openMode)
		{
		case EOpenMode::OPEN_READ_ONLY:
			openFlags = std::ios::in | std::ios::_Nocreate;
			break;
		case EOpenMode::OPEN_READ_WRITE:
			openFlags = std::ios::in | std::ios::out;
			break;
		default:
			openFlags = std::ios::in;
			break;
		}

		std::fstream fStream(fileName, openFlags);
		if (fStream.good())
		{
			m_status = EFileStatus::OPENED_SUCCESSFULLY;

			m_document.reset();
			m_document.load(fStream);
			fStream.close();
		}
		assert(allowFail || m_status == EFileStatus::OPENED_SUCCESSFULLY);

		m_fileName = fileName;
		ParseData();

		return m_status;
	}

	void DataFile::ParseData()
	{
		if (m_base == nullptr)
		{
			pugi::xml_node root = m_document.root();
			m_base = s_dataAllocator.Alloc<DataNode>(root);
		}
	}

	bool DataFile::WriteData()
	{
		if (m_base == nullptr)
			return false;

		m_document.reset();
		pugi::xml_node root = m_document.root();

		m_base->WriteData(root, "Base", false);

		std::filesystem::path directory = m_fileName;
		directory.remove_filename();

		if (directory.empty() == false)
		{
			std::filesystem::create_directory(directory);
		}
		std::ofstream outStream(m_fileName, std::ios::beg | std::ios::out);
		if (outStream.good())
		{
			m_status = EFileStatus::OPENED_SUCCESSFULLY;

			m_document.save(outStream);
			outStream.close();

			return true;
		}

		return false;
	}

	void DataFile::Close()
	{
		if (m_base)
		{
			s_dataAllocator.Free(m_base);
			m_base = nullptr;
		}
		m_document.reset();
	}

	IDataFile* IDataFile::Create()
	{
		return new DataFile;
	}

	void IDataFile::Destroy(IDataFile* file)
	{
		delete reinterpret_cast<DataFile*>(file);
	}
}