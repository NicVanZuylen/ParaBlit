#include "DataFile.h"
#include "CLib/Vector.h"
#include "Engine.Control/GUID.h"
#include "Engine.Control/IDataFile.h"
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
		for (auto& field : m_fields)
		{
			FreeFieldData(field.second);
		}
	}

	void DataNode::InitializeWithNode(const pugi::xml_node& node)
	{
		for (auto& field : node.attributes())
		{
			if (strcmp(field.name(), "TYPE") == 0)
			{
				m_type = field.value();
			}
			else if(strcmp(field.name(), "GUID") == 0)
			{
				SetSelfGUID(field.value());
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

		for (auto& a : m_fields)
		{
			std::stringstream valueStr;

			Field& field = a.second;
			switch (field.m_type)
			{
				case EFieldType::INT:
				{
					pugi::xml_node newNode = thisNode.append_child(a.first.c_str());
					newNode.append_attribute("TYPE").set_value("int");

					StringifyArray<int>(valueStr, field.m_data.Data(), field.m_data.Count());
					newNode.append_attribute("VALUE").set_value(valueStr.str().c_str());
					break;
				}
				case EFieldType::FLOAT:
				{
					pugi::xml_node newNode = thisNode.append_child(a.first.c_str());
					newNode.append_attribute("TYPE").set_value("float");

					StringifyArray<float>(valueStr, field.m_data.Data(), field.m_data.Count());
					newNode.append_attribute("VALUE").set_value(valueStr.str().c_str());
					break;
				}
				case EFieldType::DOUBLE:
				{
					pugi::xml_node newNode = thisNode.append_child(a.first.c_str());
					newNode.append_attribute("TYPE").set_value("double");

					StringifyArray<double>(valueStr, field.m_data.Data(), field.m_data.Count());
					newNode.append_attribute("VALUE").set_value(valueStr.str().c_str());
					break;
				}
				case EFieldType::BOOL:
				{
					pugi::xml_node newNode = thisNode.append_child(a.first.c_str());
					newNode.append_attribute("TYPE").set_value("bool");

					StringifyArray<bool>(valueStr, field.m_data.Data(), field.m_data.Count());
					newNode.append_attribute("VALUE").set_value(valueStr.str().c_str());
					break;
				}
				case EFieldType::STRING:
				{
					pugi::xml_node newNode = thisNode.append_child(a.first.c_str());
					newNode.append_attribute("TYPE").set_value("string");

					CLib::Vector<const char*> stringVec;
					GetString(a.first.c_str(), stringVec);
					StringifyStringArray(valueStr, stringVec);
					newNode.append_attribute("VALUE").set_value(valueStr.str().c_str());
					break;
				}
				case EFieldType::GUID:
				{
					pugi::xml_node newNode = thisNode.append_child(a.first.c_str());
					newNode.append_attribute("TYPE").set_value("GUID");

					uint32_t guidCount = 0;
					const GUID* guids = GetGUID(a.first.c_str(), guidCount);
					StringifyGUIDArray(valueStr, guids, guidCount);
					newNode.append_attribute("VALUE").set_value(valueStr.str().c_str());
					break;
				}
				case EFieldType::DATA_NODE:
				{
					uint32_t dataNodeCount = field.m_data.Count() / sizeof(DataNode*);
					for (uint32_t i = 0; i < dataNodeCount; ++i)
					{
						DataNode** n = reinterpret_cast<DataNode**>(field.m_data.Data()) + i;
						(*n)->WriteData(thisNode, a.first.c_str());
					}
					break;
				}
				default:
					break;
			}
		}
	}

	void DataNode::PeekFields(std::function<void(const std::string&, const Field&)> peekFunc) const
	{
		for (auto& field : m_fields)
			peekFunc(field.first, field.second);
	}

	const int* DataNode::GetInteger(const char* name, uint32_t& outCount) const
	{
		auto it = m_fields.find(name);
		if (it != m_fields.end())
		{
			const Field& field = it->second;
			assert(field.m_type == EFieldType::INT);
			outCount = field.m_data.Count() / sizeof(int);
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
		auto it = m_fields.find(name);
		if (it != m_fields.end())
		{
			const Field& field = it->second;
			assert(field.m_type == EFieldType::FLOAT);
			outCount = field.m_data.Count() / sizeof(float);
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
		auto it = m_fields.find(name);
		if (it != m_fields.end())
		{
			const Field& field = it->second;
			assert(field.m_type == EFieldType::DOUBLE);
			outCount = field.m_data.Count() / sizeof(double);
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
		auto it = m_fields.find(name);
		if (it != m_fields.end())
		{
			const Field& field = it->second;
			assert(field.m_type == EFieldType::BOOL);
			outCount = field.m_data.Count() / sizeof(bool);
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
		auto it = m_fields.find(name);
		if (it != m_fields.end())
		{
			const Field& field = it->second;
			assert(field.m_type == EFieldType::STRING);
			const char* currentStr = reinterpret_cast<const char*>(field.m_data.Data());
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
		auto it = m_fields.find(name);
		if (it != m_fields.end())
		{
			Field& field = it->second;
			assert(field.m_type == EFieldType::DATA_NODE);
			
			outCount = field.m_data.Count() / sizeof(DataNode*);
			return reinterpret_cast<IDataNode**>(field.m_data.Data());
		}

		return nullptr;
	}

	const IDataNode* const* DataNode::GetDataNode(const char* name, uint32_t& outCount) const
	{
		auto it = m_fields.find(name);
		if (it != m_fields.end())
		{
			const Field& field = it->second;
			assert(field.m_type == EFieldType::DATA_NODE);

			outCount = field.m_data.Count() / sizeof(DataNode*);
			return reinterpret_cast<const IDataNode* const*>(field.m_data.Data());
		}

		return nullptr;
	}

	const IDataNode* const* DataNode::GetDataNode(const Field& field, uint32_t& outCount) const
	{
		outCount = field.m_data.Count() / sizeof(DataNode*);
		return reinterpret_cast<const IDataNode* const*>(field.m_data.Data());
	}

	IDataNode* DataNode::GetDataNode(const char* name)
	{
		auto it = m_fields.find(name);
		if (it != m_fields.end())
		{
			assert(it->second.m_type == EFieldType::DATA_NODE);
			return *reinterpret_cast<IDataNode**>(it->second.m_data.Data());
		}

		return nullptr;
	}

	const IDataNode* DataNode::GetDataNode(const char* name) const
	{
		return GetDataNode(name);
	}

	void DataNode::GetAllChildDataNodes(CLib::Vector<IDataNode*>& outNodes)
	{
		for(auto& it : m_fields)
		{
			const Field& field = it.second;
			if(field.m_type == EFieldType::DATA_NODE)
			{
				uint32_t count = field.m_data.Count() / sizeof(DataNode*);
				auto nodes = reinterpret_cast<IDataNode* const*>(field.m_data.Data());
				for(uint32_t i = 0; i < count; ++i)
				{
					outNodes.PushBack(nodes[i]);
				}
			}
		}
	};

	const GUID* DataNode::GetGUID(const char* name, uint32_t& outCount) const
	{
		auto it = m_fields.find(name);
		if (it != m_fields.end())
		{
			const Field& field = it->second;
			assert(field.m_type == EFieldType::GUID);
			outCount = field.m_data.Count() / sizeof(GUID);
			return reinterpret_cast<const GUID*>(it->second.m_data.Data());
		}

		return nullptr;
	}

	const GUID* DataNode::GetGUID(const Field& field, uint32_t& outCount) const
	{
		outCount = field.m_data.Count() / sizeof(GUID);
		return (const GUID*)field.m_data.Data();
	}

	const GUID* DataNode::GetGUID(const char* name) const
	{
		uint32_t count = 0;
		return GetGUID(name, count);
	}

	void DataNode::SetInteger(const char* name, int* value, uint32_t count)
	{
		std::string nameStr = name;

		auto it = m_fields.find(nameStr);
		if (it != m_fields.end())
		{
			Field& field = it->second;
			assert(field.m_type == EFieldType::INT);
			field.m_data.SetCount(sizeof(int) * count);
			std::memcpy(field.m_data.Data(), value, sizeof(int) * count);
		}

		Field& newField = m_fields.emplace(std::pair<std::string, Field>(nameStr, {})).first->second;
		newField.m_type = EFieldType::INT;
		newField.m_data.SetCount(sizeof(int) * count);

		std::memcpy(newField.m_data.Data(), value, sizeof(int) * count);
	}

	void DataNode::SetInteger(const char* name, int value)
	{
		SetInteger(name, &value);
	}

	void DataNode::SetFloat(const char* name, float* value, uint32_t count)
	{
		std::string nameStr = name;

		auto it = m_fields.find(nameStr);
		if (it != m_fields.end())
		{
			Field& field = it->second;
			assert(field.m_type == EFieldType::FLOAT);
			field.m_data.SetCount(sizeof(float) * count);
			std::memcpy(field.m_data.Data(), value, sizeof(float) * count);
		}

		Field& newField = m_fields.emplace(std::pair<std::string, Field>(nameStr, {})).first->second;
		newField.m_type = EFieldType::FLOAT;
		newField.m_data.SetCount(sizeof(float) * count);

		std::memcpy(newField.m_data.Data(), value, sizeof(float) * count);
	}

	void DataNode::SetFloat(const char* name, float value)
	{
		SetFloat(name, &value);
	}

	void DataNode::SetDouble(const char* name, double* value, uint32_t count)
	{
		std::string nameStr = name;

		auto it = m_fields.find(nameStr);
		if (it != m_fields.end())
		{
			Field& field = it->second;
			assert(field.m_type == EFieldType::DOUBLE);
			field.m_data.SetCount(sizeof(double) * count);
			std::memcpy(field.m_data.Data(), value, sizeof(double) * count);
		}

		Field& newField = m_fields.emplace(std::pair<std::string, Field>(nameStr, {})).first->second;
		newField.m_type = EFieldType::DOUBLE;
		newField.m_data.SetCount(sizeof(double) * count);

		std::memcpy(newField.m_data.Data(), value, sizeof(double) * count);
	}

	void DataNode::SetDouble(const char* name, double value)
	{
		SetDouble(name, &value);
	}

	void DataNode::SetBool(const char* name, bool* value, uint32_t count)
	{
		std::string nameStr = name;

		auto it = m_fields.find(nameStr);
		if (it != m_fields.end())
		{
			Field& field = it->second;
			assert(field.m_type == EFieldType::BOOL);
			field.m_data.SetCount(sizeof(bool) * count);
			std::memcpy(field.m_data.Data(), value, sizeof(bool) * count);
		}

		Field& newField = m_fields.emplace(std::pair<std::string, Field>(nameStr, {})).first->second;
		newField.m_type = EFieldType::BOOL;
		newField.m_data.SetCount(sizeof(bool) * count);

		std::memcpy(newField.m_data.Data(), value, sizeof(bool) * count);
	}

	void DataNode::SetBool(const char* name, bool value)
	{
		SetBool(name, &value);
	}

	void DataNode::SetString(const char* name, const char** value, uint32_t count)
	{
		std::string nameStr = name;

		auto it = m_fields.find(nameStr);
		if (it != m_fields.end())
		{
			Field& field = it->second;
			assert(field.m_type == EFieldType::STRING);
			
			uint32_t totalCharCount = 0;
			for (uint32_t i = 0; i < count; ++i)
			{
				uint32_t charCount = strlen(value[i]) + 1;
				uint32_t pos = totalCharCount;
				totalCharCount += charCount;

				field.m_data.SetCount(totalCharCount);
				std::memcpy(field.m_data.Data() + pos, value[i], charCount);
			}
			field.m_data.PushBack(uint8_t('\0')); // Push another null terminator to terminate array with a double null terminator.
		}

		Field& newField = m_fields.emplace(std::pair<std::string, Field>(nameStr, {})).first->second;
		newField.m_type = EFieldType::STRING;
		
		uint32_t totalCharCount = 0;
		for (uint32_t i = 0; i < count; ++i)
		{
			uint32_t charCount = strlen(value[i]) + 1;
			uint32_t pos = totalCharCount;
			totalCharCount += charCount;

			newField.m_data.SetCount(totalCharCount);
			std::memcpy(newField.m_data.Data() + pos, value[i], charCount);
		}
		newField.m_data.PushBack(uint8_t('\0')); // Push another null terminator to terminate array with a double null terminator.
	}

	void DataNode::SetString(const char* name, const char* value)
	{
		SetString(name, &value);
	}

	void DataNode::SetGUID(const char* name, const GUID* value, uint32_t count)
	{
		std::string nameStr = name;

		auto it = m_fields.find(nameStr);
		if (it != m_fields.end())
		{
			Field& field = it->second;
			assert(field.m_type == EFieldType::GUID);
			field.m_data.SetCount(count * sizeof(GUID));

			for (uint32_t i = 0; i < count; ++i)
			{
				uint32_t pos = i * sizeof(GUID);
				GUID* val = reinterpret_cast<GUID*>(field.m_data.Data() + pos);
				*val = value[i];
			}
		}

		Field& newField = m_fields.emplace(std::pair<std::string, Field>(nameStr, {})).first->second;
		newField.m_type = EFieldType::GUID;
		newField.m_data.SetCount(count * sizeof(GUID));

		for (uint32_t i = 0; i < count; ++i)
		{
			uint32_t pos = i * sizeof(GUID);
			GUID* val = reinterpret_cast<GUID*>(newField.m_data.Data() + pos);
			*val = value[i];
		}
	}

	void DataNode::SetGUID(const char* name, const GUID& value)
	{
		SetGUID(name, &value);
	}

	void DataNode::RemoveFieldOrNode(const char* name, uint32_t index)
	{
		auto it = m_fields.find(name);
		if (it != m_fields.end())
		{
			Field& field = it->second;
			FreeFieldData(field, index);
			if (field.m_data.Count() == 0)
			{
				m_fields.erase(it);
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
		auto it = m_fields.find(nameStr);
		if (it != m_fields.end())
		{
			Field& field = it->second;
			uint32_t pos = field.m_data.Count();
			field.m_data.SetCount(field.m_data.Count() + sizeof(DataNode*));
			assert(field.m_type == EFieldType::DATA_NODE);

			uint8_t* data = field.m_data.Data();
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

		Field& newField = m_fields.emplace(std::pair<std::string, Field>(nameStr, {})).first->second;
		newField.m_type = EFieldType::DATA_NODE;
		newField.m_data.SetCount(sizeof(DataNode*));

		DataNode*& nodePtr = *reinterpret_cast<DataNode**>(newField.m_data.Data());
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
		auto it = m_fields.find(nameStr);
		if (it != m_fields.end())
		{
			Field& field = it->second;
			assert(field.m_type == EFieldType::DATA_NODE);
			
			return *reinterpret_cast<DataNode**>(field.m_data.Data());
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
		for (auto& field : node.attributes())
		{
			if (strcmp(field.name(), "TYPE") == 0)
			{
				typeAttrib = field;
				validType = true;
			}
			else if (strcmp(field.name(), "VALUE") == 0)
			{
				valueAttrib = field;
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
				CLib::Vector<GUID, 0, 8, true> guidArr;
				std::string valueStr = valueAttrib.value();

				size_t pos = 0;
				size_t term = 0;
				while (term != std::string::npos)
				{
					pos = valueStr.find_first_not_of(' ', pos);
					term = valueStr.find_first_of(',', pos);

					if (pos == std::string::npos)
						break;

					std::string substr = term != std::string::npos ? valueStr.substr(pos, term - pos) : valueStr.substr(pos);
					if(substr.length() >= GUIDLength)
					{
						guidArr.PushBackInit(substr.c_str());
					}
					else
					{
						guidArr.PushBackInit();
					}

					pos = term + 1;
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

	void DataNode::FreeFieldData(Field& field, uint32_t index)
	{
		switch (field.m_type)
		{
			case EFieldType::DATA_NODE:
			{
				uint32_t count = field.m_data.Count() / sizeof(DataNode*);
				DataNode** dataNodes = reinterpret_cast<DataNode**>(field.m_data.Data());
				if (index < count)
				{
					// Remove a single node at the index.
					s_dataAllocator.Free(dataNodes[index]);
					if (index < count - 1)
					{
						size_t copyCount = count - (index + 1);
						std::memcpy(&dataNodes[index], &dataNodes[index + 1], sizeof(DataNode*) * copyCount);
					}
					field.m_data.SetCount(field.m_data.Count() - sizeof(DataNode*));
				}
				else
				{
					// Delete all data of this field.
					for (uint32_t i = 0; i < count; ++i)
					{
						s_dataAllocator.Free(dataNodes[i]);
					}
					field.m_data.Clear();
				}
				break;
			}
			default:
				assert(index == ~uint32_t(0) && "NOT IMPLEMENTED!");
				break; // Other field types do not dynamically allocate data (aside from the field's data vector which frees its memory upon destruction).
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
			if(guidArray[i].IsValid()) // Don't write a null GUID as that would involve writing null terminators into the string. This would prevent output of any GUIDs after the null one.
			{
				outStr.write(guidArray[i], GUIDLength);
			}
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
			openFlags = std::ios::in;
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

	bool DataFile::WriteData(const char* dstFilename)
	{
		if (m_base == nullptr)
			return false;

		m_document.reset();
		pugi::xml_node root = m_document.root();

		m_base->WriteData(root, "Base", false);

		std::filesystem::path fileName = dstFilename ? dstFilename : m_fileName;
		std::filesystem::path directory = fileName;
		directory.remove_filename();

		if (directory.empty() == false)
		{
			std::filesystem::create_directory(directory);
		}
		std::ofstream outStream(fileName, std::ofstream::out);
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