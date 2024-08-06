#include "DataClass.h"

namespace Ctrl
{
	std::unordered_map<Ctrl::GUID, DataClass*> DataClass::s_globalDataClassMap;
	std::mutex DataClass::s_globalDataClassMapMutex;

	DataClass::DataClass(CLib::Reflection::Reflector* reflection)
	{
		m_reflector = reflection;
	}

	DataClass::~DataClass()
	{
		if (m_guid.IsValid())
		{
			std::lock_guard<std::mutex> lock(s_globalDataClassMapMutex);
			s_globalDataClassMap[m_guid] = nullptr;
		}
	}

	void DataClass::FillFromDataNode(const IDataNode* node)
	{
		auto fillFromAttrib = [&](const std::string& name, const Attribute& attrib)
		{
			size_t fieldSize = 0;
			void* fieldData = m_reflector->GetFieldWithName(name.c_str(), &fieldSize);

			if (fieldData != nullptr)
			{
				switch (attrib.m_type)
				{
					case EAttributeType::STRING:
					{
						// Field is expected to be an std::string.
						*reinterpret_cast<std::string*>(fieldData) = reinterpret_cast<const char*>(attrib.m_data.Data());
						break;
					}
					case EAttributeType::DATA_NODE:
					{
						const DataNode* node = *(const DataNode**)attrib.m_data.Data();
						const char* typeName = node->GetTypeName();
						
						// Field is expected to be a ReflectableClass instance of a derived class of DataClass. 
						// Instantiate and fill the data class field with this data node attribute.
						CLib::Reflection::PlacedConstructClass(typeName, reinterpret_cast<CLib::Reflection::ReflectableClass*>(fieldData));
						DataClass* fieldClass = reinterpret_cast<DataClass*>(fieldData);
						fieldClass->FillFromDataNode(node);

						// Validate object ptr in global DataClass map to allow referencing by guid.
						const GUID& nodeGuid = node->GetSelfGUID();
						if (nodeGuid.IsValid())
						{
							std::lock_guard<std::mutex> lock(s_globalDataClassMapMutex);

							auto it = s_globalDataClassMap.find(nodeGuid);
							if (it == s_globalDataClassMap.end())
							{
								s_globalDataClassMap.insert({ nodeGuid, fieldClass });
							}
							else
							{
								it->second = fieldClass;
							}
						}
						break;
					}
					case EAttributeType::GUID:
					{
						// Field is expected to be a pointer to the DataClass object instance referenced by this GUID.
						const Ctrl::GUID* guid = *(const Ctrl::GUID**)attrib.m_data.Data();
						reinterpret_cast<ObjectPtr*>(fieldData)->Assign(*guid);
						break;
					}
					default:
					{
						// Field is a known-length scalar value (or array of scalar values). Copy the attribute data to the field.
						fieldSize = std::min<size_t>(fieldSize, attrib.m_data.Count());
						std::memcpy(fieldData, attrib.m_data.Data(), fieldSize);
						break;
					}
				}
			}
		};

		m_guid = node->GetSelfGUID();
		node->PeekAttributes(fillFromAttrib);
	}
}
