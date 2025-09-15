#include "Engine.Control/IDataClass.h"
#include "DataFile/DataFile.h"

#include <cassert>

namespace Ctrl
{
	std::unordered_map<Ctrl::GUID, DataClass::DataClassTracking> DataClass::s_globalDataClassMap;
	std::unordered_map<DataClass**, Ctrl::GUID> DataClass::s_globalGUIDMap;
	std::mutex DataClass::s_globalDataClassMapMutex;

	DATA_CLASS_TYPE_MAP(Vector2f, float, 2);
	DATA_CLASS_TYPE_MAP(Vector2d, double, 2);
	DATA_CLASS_TYPE_MAP(Vector3f, float, 3);
	DATA_CLASS_TYPE_MAP(Vector3d, double, 3);
	DATA_CLASS_TYPE_MAP(Vector4f, float, 4);
	DATA_CLASS_TYPE_MAP(Vector4d, double, 4);

	DATA_CLASS_TYPE_MAP(Vector2i, int, 2);
	DATA_CLASS_TYPE_MAP(Vector2u, int, 2);
	DATA_CLASS_TYPE_MAP(Vector3i, int, 3);
	DATA_CLASS_TYPE_MAP(Vector3u, int, 3);
	DATA_CLASS_TYPE_MAP(Vector4i, int, 4);
	DATA_CLASS_TYPE_MAP(Vector4u, int, 4);

	DATA_CLASS_TYPE_MAP(Quaternion, float, 4);
	DATA_CLASS_TYPE_MAP(Quaterniond, double, 4);

	DATA_CLASS_TYPE_MAP(Matrix4f, float, 16);
	DATA_CLASS_TYPE_MAP(Matrix4d, double, 16);

	void DataClass::DataClassTracking::FreeDataClass()
	{
		CLib::Reflection::DeleteClass(m_ptr->GetReflection().GetTypeName(), m_ptr);
		m_ptr = nullptr;
	}

	void ObjectPtr::Assign(const GUID& guid)
	{
		if (guid.IsValid())
		{
			auto& classMap = DataClass::s_globalDataClassMap;
			auto& guidMap = DataClass::s_globalGUIDMap;

			auto it = classMap.find(guid);
			if (it != classMap.end())
			{
				auto& tracking = it->second;
				tracking.AddRef();
				m_ptr = &tracking.m_ptr;
			}
			else
			{
				auto& tracking = classMap[guid] = { nullptr, 0 };
				tracking.AddRef();
				m_ptr = &tracking.m_ptr;
				guidMap[m_ptr] = guid;
			}
		}
	}

	void ObjectPtr::AssignDataClass(const DataClass* dataClass)
	{
		DataClass* dc = const_cast<DataClass*>(dataClass);
		if (dc->m_guid.IsValid() == false)
		{
			dc->AssignNewGUID(Ctrl::GenerateGUID());
		}

		Assign(dc->m_guid);
	}

	void DataClass::InstantiateNodeTree(const Ctrl::IDataNode* root)
	{
		auto peekAttrib = [&](const std::string& name, const Attribute& attrib)
		{
			if(attrib.m_type == EAttributeType::DATA_NODE)
			{
				uint32_t nodeCount = attrib.m_data.Count() / sizeof(DataNode*);
				const DataNode** nodes = (const DataNode**)attrib.m_data.Data();
				for (uint32_t i = 0; i < nodeCount; ++i)
				{
					const DataNode* node = nodes[i];
					const char* typeName = node->GetTypeName();

					if (typeName != nullptr)
					{
						DataClass* nodeClass = CLib::Reflection::InstantiateClass<DataClass>(typeName);
						nodeClass->FillFromDataNode(node);

						// Validate object ptr in global DataClass map to allow referencing by guid.
						const GUID& nodeGuid = node->GetSelfGUID();
						if (nodeGuid.IsValid())
						{
							nodeClass->AssignNewGUID(nodeGuid);
						}
						else
						{
							nodeClass->AssignNewGUID(Ctrl::GenerateGUID()); // If the node was not assigned a GUID, give it a new one here.
						}
					}
					else
					{
						InstantiateNodeTree(node);
					}
				}
			}
		};

		root->PeekAttributes(peekAttrib);
	}

	void DataClass::SaveNodeTree(Ctrl::IDataNode* root)
	{
		/*
		TODO:

		1. Modify CLib reflection to get the type name from a reflectable field declaration.
		2. Use the type name determine which type of data attribute to add.
		3. Add raw binary blob attribute functionality to DataNode to load/store unrecognized types.
		4. Add alias macro to reveal basic type values for math Vectors etc. "DATACLASS_VIEW_TYPES(Type, member types...)" Eg. DATACLASS_VIEW_TYPES(Vector3f, float, float, float) or DATACLASS_VIEW_TYPES(Vector3f, (3)float) (Array)
			^ This macro will add the type's name to a static-initialized map which contains information on which DataNode attribute types each member corresponds to.
			  The macro could also be used for basic types (int, float, bool, etc.) to avoid creating special cases for them. E.g. DATACLASS_VIEW_TYPES(int, int)
			  In turn, the type specified in the macro will be treated as a known type instead of a binary blob.
		*/

		auto peekAttrib = [&](const std::string& name, const Attribute& attrib)
		{
			if (attrib.m_type == EAttributeType::DATA_NODE)
			{
				uint32_t nodeCount = attrib.m_data.Count() / sizeof(DataNode*);
				DataNode** nodes = (DataNode**)attrib.m_data.Data();
				for (int i = 0; i < nodeCount; ++i)
				{
					DataNode* node = nodes[i];
					const char* typeName = node->GetTypeName();

					if (typeName != nullptr)
					{
						const GUID& nodeGuid = node->GetSelfGUID();
						if (nodeGuid.IsValid())
						{
							DataClass* nodeClass = s_globalDataClassMap[nodeGuid].m_ptr;
							if (nodeClass != nullptr)
							{
								nodeClass->SaveToDataNode(root, node);
							}
							else
							{
								root->RemoveAttributeOrNode(name.c_str(), i);
								// Correct index and count.
								nodeCount--;
								i--;
							}
						}
					}
					else
					{
						SaveNodeTree(node);
					}
				}
			}
		};

		root->PeekAttributes(peekAttrib);
	}

	DataClass::~DataClass()
	{
		if (m_guid.IsValid())
		{
			std::lock_guard<std::mutex> lock(s_globalDataClassMapMutex);
			s_globalDataClassMap[m_guid].m_ptr = nullptr;
		}
	}

	void DataClass::AssignNewGUID(const GUID& guid)
	{
		m_guid = guid;
		if (m_guid.IsValid())
		{
			std::lock_guard<std::mutex> lock(s_globalDataClassMapMutex);
			DataClassTracking& tracking = s_globalDataClassMap[m_guid];

			tracking.m_ptr = DerivedAddress();
			s_globalGUIDMap[&tracking.m_ptr] = m_guid;
		}
	}

	void DataClass::FillFromDataNode(const IDataNode* node)
	{
		auto fillFromAttrib = [&](const std::string& name, const Attribute& attrib)
		{
			size_t fieldSize = 0;
			void* fieldData = m_reflector.GetFieldWithName(name.c_str(), &fieldSize);

			if (fieldData != nullptr)
			{
				switch (attrib.m_type)
				{
					case EAttributeType::STRING:
					{
						// Field is expected to be an std::string.
						assert(fieldSize % sizeof(std::string) == 0);

						uint32_t fieldStringCount = fieldSize / sizeof(std::string);
						CLib::Vector<const char*> attribStrings; 
						node->GetString(name.c_str(), attribStrings);

						uint32_t stringCount = std::min<uint32_t>(fieldStringCount, attribStrings.Count());
						for (uint32_t i = 0; i < stringCount; ++i)
						{
							reinterpret_cast<std::string*>(fieldData)[i] = attribStrings[i];
						}
						break;
					}
					case EAttributeType::DATA_NODE:
					{
						uint32_t nodeCount = attrib.m_data.Count() / sizeof(DataNode*);
						const DataNode** nodes = (const DataNode**)attrib.m_data.Data();
						CLib::Reflection::ReflectableClass* reflectableClasses = reinterpret_cast<CLib::Reflection::ReflectableClass*>(fieldData);
						for (uint32_t i = 0; i < nodeCount; ++i)
						{
							const DataNode* node = nodes[i];
							const char* typeName = node->GetTypeName();

							// Field is expected to be a ReflectableClass instance of a derived class of DataClass. 
							// Instantiate and fill the data class field with this data node attribute.
							CLib::Reflection::PlacedConstructClass(typeName, &reflectableClasses[i]);
							DataClass* nodeClass = reinterpret_cast<DataClass*>(&reflectableClasses[i]);
							nodeClass->FillFromDataNode(node);

							// Validate object ptr in global DataClass map to allow referencing by guid.
							const GUID& nodeGuid = node->GetSelfGUID();
							if (nodeGuid.IsValid())
							{
								nodeClass->AssignNewGUID(nodeGuid);
							}
							else
							{
								nodeClass->AssignNewGUID(Ctrl::GenerateGUID()); // If the node was not assigned a GUID, give it a new one here.
							}
						}
						break;
					}
					case EAttributeType::GUID:
					{
						bool isDynamicArray = fieldSize == sizeof(ObjectPtrArray);
						if (isDynamicArray == false)
						{
							assert(fieldSize % sizeof(ObjectPtr) == 0);

							// Field is expected to be a pointer (or C-array of pointers) to the DataClass object instance referenced by this GUID.
							const uint32_t guidCount = std::min<uint32_t>(fieldSize / sizeof(ObjectPtr), attrib.m_data.Count() / sizeof(GUID));
							const Ctrl::GUID* guidArr = reinterpret_cast<const GUID*>(attrib.m_data.Data());
							for (uint32_t i = 0; i < guidCount; ++i)
							{
								ObjectPtr& ptr = reinterpret_cast<ObjectPtr*>(fieldData)[i];
								ptr.Assign(guidArr[i]);
							}
						}
						else
						{
							ObjectPtrArray& ptrArr = *reinterpret_cast<ObjectPtrArray*>(fieldData);

							const uint32_t guidCount = attrib.m_data.Count() / sizeof(GUID);
							const Ctrl::GUID* guidArr = reinterpret_cast<const GUID*>(attrib.m_data.Data());
							for (uint32_t i = 0; i < guidCount; ++i)
							{
								ObjectPtr newPtr;
								newPtr.Assign(guidArr[i]);
								ptrArr.PushBack(newPtr);
							}
						}
						break;
					}
					default:
					{
						// Field is a known-length scalar value (or array of scalar values). Copy the attribute data to the field.
						fieldSize = std::min<size_t>(fieldSize, attrib.m_data.Count());
						std::memcpy(fieldData, attrib.m_data.Data(), fieldSize);

						uint32_t fieldCount = 1;
						switch (attrib.m_type)
						{
							case EAttributeType::INT:
							{ fieldCount = fieldSize / sizeof(int); break; }
							case EAttributeType::FLOAT:
							{ fieldCount = fieldSize / sizeof(float); break; }
							case EAttributeType::DOUBLE:
							{ fieldCount = fieldSize / sizeof(double); break; }
							case EAttributeType::BOOL:
							{ fieldCount = fieldSize / sizeof(bool); break; }
							default:
								assert(false && "SCALAR TYPE NOT IMPLEMENTED!");
								break;
						}

						break;
					}
				}
			}
		};

		AssignNewGUID(node->GetSelfGUID());
		node->PeekAttributes(fillFromAttrib);
	}

	void DataClass::SaveToDataNode(IDataNode* parentNode, IDataNode* dstNode) const
	{
		dstNode->SetSelfGUID(m_guid);

		auto& reflectionFields = m_reflector.GetReflectionFields();
		uint32_t i = 0;
		for (auto& f : reflectionFields)
		{
			auto& field = f.second;
			void* fieldData = m_reflector.GetFieldAtOffset(field.m_offset);

			EAttributeType type = EAttributeType::UNKNOWN_OR_INVALID;
			size_t scalarArrayCount = field.m_arrayCount;
			auto typeIt = s_typeMap.find(field.m_typeName);
			if(typeIt != s_typeMap.end())
			{
				type = typeIt->second.first;
				scalarArrayCount *= typeIt->second.second;
			}
			else if(std::strstr(field.m_typeName, "TObjectPtr<") || std::strstr(field.m_typeName, "TObjectPtrArray<"))
			{
				type = EAttributeType::GUID;
			}
			assert(type != EAttributeType::UNKNOWN_OR_INVALID);

			switch (type)
			{
				case EAttributeType::UNKNOWN_OR_INVALID:
					break;
				case EAttributeType::STRING:
				{
					// Field is expected to be a single value or array of std::string.
					assert(field.m_size % sizeof(std::string) == 0);

					const std::string* fieldStrings = reinterpret_cast<const std::string*>(fieldData);
					CLib::Vector<const char*, 4, 4> cStrings;
					for (uint32_t i = 0; i < scalarArrayCount; ++i)
					{
						cStrings.PushBack(fieldStrings[i].c_str());
					}
					dstNode->SetString(field.m_name, cStrings.Data(), cStrings.Count());
					break;
				}
				case EAttributeType::DATA_NODE:
				{
					assert(false && "NOT IMPLEMENTED!");
					break;
				}
				case EAttributeType::GUID:
				{
					std::lock_guard<std::mutex> lock(s_globalDataClassMapMutex);
					CLib::Vector<GUID, 8, 8> guids;
					if(field.m_size == sizeof(ObjectPtrArray))
					{
						const ObjectPtrArray& fieldPtrArr = *reinterpret_cast<const ObjectPtrArray*>(fieldData);
						for (const ObjectPtr& fieldPtr : fieldPtrArr)
						{
							guids.PushBack(s_globalGUIDMap[fieldPtr.GetBasePtr()]);
						}
					}
					else
					{
						const ObjectPtr* fieldPtrs = reinterpret_cast<const ObjectPtr*>(fieldData);
						for (uint32_t i = 0; i < scalarArrayCount; ++i)
						{
							guids.PushBack(s_globalGUIDMap[fieldPtrs[i].GetBasePtr()]);
						}
						dstNode->SetGUID(field.m_name, guids.Data(), guids.Count());
					}
					dstNode->SetGUID(field.m_name, guids.Data(), guids.Count());
					break;
				}
				case EAttributeType::INT:
					dstNode->SetInteger(field.m_name, reinterpret_cast<int*>(fieldData), scalarArrayCount);
					break;
				case EAttributeType::FLOAT:
					dstNode->SetFloat(field.m_name, reinterpret_cast<float*>(fieldData), scalarArrayCount);
					break;
				case EAttributeType::DOUBLE:
					dstNode->SetDouble(field.m_name, reinterpret_cast<double*>(fieldData), scalarArrayCount);
					break;
				case EAttributeType::BOOL:
					dstNode->SetBool(field.m_name, reinterpret_cast<bool*>(fieldData), scalarArrayCount);
					break;
				default:
					assert(false && "SCALAR TYPE NOT IMPLEMENTED!");
					break;
			}

			++i;
		}
	}
}
