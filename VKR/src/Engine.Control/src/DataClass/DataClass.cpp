#include "Engine.Control/GUID.h"
#include "Engine.Control/IDataClass.h"
#include "DataFile/DataFile.h"
#include "CLib/Reflection.h"

#include <cassert>

namespace Ctrl
{
	std::unordered_map<Ctrl::GUID, DataClassTracking> DataClass::s_globalDataClassMap;
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

	DataClassTracking::~DataClassTracking()
	{
		if (m_ptr != nullptr && m_refCount.load() == 0)
		{
			printf("[Engine.Control] WARNING: DataClass [%s] (%p) was instantiated but never referenced.\n", m_ptr->GetGUID().AsCString(), m_ptr);
		}
	}

	void DataClassTracking::FreeDataClass()
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
		else
		{
			Invalidate();
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

	const GUID& ObjectPtr::GetAssignedGUID() const
	{
		auto& guidMap = DataClass::s_globalGUIDMap;
		auto it = guidMap.find(m_ptr);
		if(it != guidMap.end())
		{
			return it->second;
		}

		return Ctrl::nullGUID;
	}

	void DataClass::InstantiateNodeTree(const Ctrl::IDataNode* root)
	{
		auto peekField = [&](const std::string& name, const Field& field)
		{
			if(field.m_type == EFieldType::DATA_NODE)
			{
				uint32_t nodeCount = field.m_data.Count() / sizeof(DataNode*);
				const DataNode** nodes = (const DataNode**)field.m_data.Data();
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

		root->PeekFields(peekField);
	}

	void DataClass::SaveNodeTree(Ctrl::IDataNode* root)
	{
		/*
		TODO:

		1. Modify CLib reflection to get the type name from a reflectable field declaration.
		2. Use the type name determine which type of data field to add.
		3. Add raw binary blob field functionality to DataNode to load/store unrecognized types.
		4. Add alias macro to reveal basic type values for math Vectors etc. "DATACLASS_VIEW_TYPES(Type, member types...)" Eg. DATACLASS_VIEW_TYPES(Vector3f, float, float, float) or DATACLASS_VIEW_TYPES(Vector3f, (3)float) (Array)
			^ This macro will add the type's name to a static-initialized map which contains information on which DataNode field types each member corresponds to.
			  The macro could also be used for basic types (int, float, bool, etc.) to avoid creating special cases for them. E.g. DATACLASS_VIEW_TYPES(int, int)
			  In turn, the type specified in the macro will be treated as a known type instead of a binary blob.
		*/

		auto peekField = [&](const std::string& name, const Field& field)
		{
			if (field.m_type == EFieldType::DATA_NODE)
			{
				uint32_t nodeCount = field.m_data.Count() / sizeof(DataNode*);
				DataNode** nodes = (DataNode**)field.m_data.Data();
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
								nodeClass->SaveToDataNode(node);
							}
							else
							{
								root->RemoveFieldOrNode(name.c_str(), i);
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

		root->PeekFields(peekField);
	}

	void DataClass::GetTypeInfoFromField(const ReflectronFieldData& field, EFieldType& outType, size_t& outArrayCount)
	{
		outArrayCount = field.m_arrayCount;

		auto typeIt = s_typeMap.find(field.m_typeName);
		if (typeIt != s_typeMap.end())
		{
			outType = typeIt->second.first;
			outArrayCount *= typeIt->second.second;

			return;
		}
		else if (std::strstr(field.m_typeName, "TObjectPtr<") || std::strstr(field.m_typeName, "TObjectPtrArray<"))
		{
			outType = EFieldType::GUID;

			return;
		}

		outType = EFieldType::UNKNOWN_OR_INVALID;
	}

	std::string DataClass::GetDataClassTypeFromField(const ReflectronFieldData& field)
	{
		std::string typeName = field.m_typeName;
			
		auto dcTypeStartPos = typeName.find_first_of('<');
		if(dcTypeStartPos != std::string::npos)
		{
			dcTypeStartPos += 1;
		}

		auto dcTypeEndPos = typeName.find_first_of('>', dcTypeStartPos);

		if (dcTypeStartPos != std::string::npos && dcTypeEndPos != std::string::npos)
		{
			std::string dcTypeName = typeName.substr(dcTypeStartPos, dcTypeEndPos - dcTypeStartPos);

			auto lastColumnPos = dcTypeName.find_last_of(':');
			if (lastColumnPos != std::string::npos)
			{
				dcTypeName = dcTypeName.substr(lastColumnPos + 1);
			}

			return dcTypeName;
		}

		return nullptr;
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
		auto fillFromField = [&](const std::string& name, const Field& field /* serialized field */)
		{
			auto* runtimeField = m_reflector.GetFieldWithName(name.c_str());
			if (runtimeField == nullptr) // Field doesn't exist. Ignore it.
				return;

			size_t fieldSize = runtimeField->m_size;
			void* fieldData = m_reflector.GetFieldValue(*runtimeField);

			if (fieldData != nullptr)
			{
				switch (field.m_type)
				{
					case EFieldType::STRING:
					{
						// Field is expected to be an std::string.
						assert(fieldSize % sizeof(std::string) == 0);

						uint32_t fieldStringCount = fieldSize / sizeof(std::string);
						CLib::Vector<const char*> fieldStrings; 
						node->GetString(name.c_str(), fieldStrings);

						uint32_t stringCount = std::min<uint32_t>(fieldStringCount, fieldStrings.Count());
						for (uint32_t i = 0; i < stringCount; ++i)
						{
							reinterpret_cast<std::string*>(fieldData)[i] = fieldStrings[i];
						}
						break;
					}
					case EFieldType::DATA_NODE:
					{
						// uint32_t nodeCount = field.m_data.Count() / sizeof(DataNode*);
						// const DataNode** nodes = (const DataNode**)field.m_data.Data();
						// CLib::Reflection::ReflectableClass* reflectableClasses = reinterpret_cast<CLib::Reflection::ReflectableClass*>(fieldData);
						// for (uint32_t i = 0; i < nodeCount; ++i)
						// {
						// 	const DataNode* node = nodes[i];
						// 	const char* typeName = node->GetTypeName();

						// 	// Field is expected to be a ReflectableClass instance of a derived class of DataClass. 
						// 	// Instantiate and fill the data class field with this data node field.
						// 	CLib::Reflection::PlacedConstructClass(typeName, &reflectableClasses[i]);
						// 	DataClass* nodeClass = reinterpret_cast<DataClass*>(&reflectableClasses[i]);
						// 	nodeClass->FillFromDataNode(node);

						// 	// Validate object ptr in global DataClass map to allow referencing by guid.
						// 	const GUID& nodeGuid = node->GetSelfGUID();
						// 	if (nodeGuid.IsValid())
						// 	{
						// 		nodeClass->AssignNewGUID(nodeGuid);
						// 	}
						// 	else
						// 	{
						// 		nodeClass->AssignNewGUID(Ctrl::GenerateGUID()); // If the node was not assigned a GUID, give it a new one here.
						// 	}
						// }
						break;
					}
					case EFieldType::GUID:
					{
						bool isDynamicArray = fieldSize == sizeof(ObjectPtrArray);
						if (isDynamicArray == false)
						{
							assert(fieldSize % sizeof(ObjectPtr) == 0);

							// Field is expected to be a pointer (or C-array of pointers) to the DataClass object instance referenced by this GUID.
							const uint32_t guidCount = std::min<uint32_t>(fieldSize / sizeof(ObjectPtr), field.m_data.Count() / sizeof(GUID));
							const Ctrl::GUID* guidArr = reinterpret_cast<const GUID*>(field.m_data.Data());
							for (uint32_t i = 0; i < guidCount; ++i)
							{
								ObjectPtr& ptr = reinterpret_cast<ObjectPtr*>(fieldData)[i];
								ptr.Assign(guidArr[i]);
							}
						}
						else
						{
							ObjectPtrArray& ptrArr = *reinterpret_cast<ObjectPtrArray*>(fieldData);

							const uint32_t guidCount = field.m_data.Count() / sizeof(GUID);
							const Ctrl::GUID* guidArr = reinterpret_cast<const GUID*>(field.m_data.Data());
							for (uint32_t i = 0; i < guidCount; ++i)
							{
								ptrArr.PushBackInit(guidArr[i]);
							}
						}
						break;
					}
					default:
					{
						// Field is a known-length scalar value (or array of scalar values). Copy the field data to the field.
						fieldSize = std::min<size_t>(fieldSize, field.m_data.Count());
						std::memcpy(fieldData, field.m_data.Data(), fieldSize);

						uint32_t fieldCount = 1;
						switch (field.m_type)
						{
							case EFieldType::INT:
							{ fieldCount = fieldSize / sizeof(int); break; }
							case EFieldType::FLOAT:
							{ fieldCount = fieldSize / sizeof(float); break; }
							case EFieldType::DOUBLE:
							{ fieldCount = fieldSize / sizeof(double); break; }
							case EFieldType::BOOL:
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
		node->PeekFields(fillFromField);
	}

	bool DataClass::FieldsMatchDataNode(const IDataNode* node)
	{
		if (node->GetSelfGUID() != m_guid)
			return false;

		bool fieldsMatch = true;
		auto compareFields = [&](const std::string& name, const Field& field /* serialized field */)
		{
			if (fieldsMatch == false)
				return;

			auto* runtimeField = m_reflector.GetFieldWithName(name.c_str());
			if (runtimeField == nullptr) // Field doesn't exist. Ignore it.
				return;

			size_t fieldSize = runtimeField->m_size;
			void* fieldData = m_reflector.GetFieldValue(*runtimeField);

			if (fieldData != nullptr)
			{
				switch (field.m_type)
				{
					case EFieldType::STRING:
					{
						// Field is expected to be an std::string.
						assert(fieldSize % sizeof(std::string) == 0);

						uint32_t fieldStringCount = fieldSize / sizeof(std::string);
						CLib::Vector<const char*> fieldStrings; 
						node->GetString(name.c_str(), fieldStrings);

						uint32_t stringCount = std::min<uint32_t>(fieldStringCount, fieldStrings.Count());

						for (uint32_t i = 0; i < stringCount; ++i)
						{
							fieldsMatch &= (reinterpret_cast<std::string*>(fieldData)[i] == fieldStrings[i]);
						}
						break;
					}
					case EFieldType::DATA_NODE:
					{
						break;
					}
					case EFieldType::GUID:
					{
						bool isDynamicArray = fieldSize == sizeof(ObjectPtrArray);
						if (isDynamicArray == false)
						{
							assert(fieldSize % sizeof(ObjectPtr) == 0);

							// Field is expected to be a pointer (or C-array of pointers) to the DataClass object instance referenced by this GUID.
							const uint32_t fieldCount = fieldSize / sizeof(ObjectPtr);
							const uint32_t guidCount = field.m_data.Count() / sizeof(GUID);

							const uint32_t count = std::min<uint32_t>(fieldCount, guidCount);
							const Ctrl::GUID* guidArr = reinterpret_cast<const GUID*>(field.m_data.Data());
							for (uint32_t i = 0; i < count; ++i)
							{
								const ObjectPtr& ptr = reinterpret_cast<ObjectPtr*>(fieldData)[i];
								const GUID& guid = guidArr[i];

								bool bothNull = (ptr.IsValid() == false && guid.IsValid() == false);
								fieldsMatch &= (bothNull || (ptr.IsValid() && ptr->GetGUID() == guid));
							}
						}
						else
						{
							ObjectPtrArray& ptrArr = *reinterpret_cast<ObjectPtrArray*>(fieldData);

							const uint32_t guidCount = field.m_data.Count() / sizeof(GUID);
							const Ctrl::GUID* guidArr = reinterpret_cast<const GUID*>(field.m_data.Data());
							for (uint32_t i = 0; i < guidCount; ++i)
							{
								const ObjectPtr& ptr = ptrArr[i];
								const GUID& guid = guidArr[i];

								bool bothNull = (ptr.IsValid() == false && guid.IsValid() == false);
								fieldsMatch &= (bothNull || (ptr.IsValid() && ptr->GetGUID() == guid));
							}
						}
						break;
					}
					default:
					{
						if (field.m_data.Count() != fieldSize)
						{
							fieldsMatch = false;
							return;
						}

						// Simple binary compare for scalar primitives.
						fieldsMatch &= (memcmp(fieldData, field.m_data.Data(), fieldSize) == 0);
						break;
					}
				}
			}
		};

		node->PeekFields(compareFields);

		return fieldsMatch;
	}

	void DataClass::SaveToDataNode(IDataNode* dstNode) const
	{
		dstNode->SetSelfGUID(m_guid);

		auto& reflectionFields = m_reflector.GetReflectionFields();
		uint32_t i = 0;
		for (auto& field : reflectionFields)
		{
			void* fieldData = m_reflector.GetFieldValue(field);

			EFieldType type;
			size_t scalarArrayCount;
			GetTypeInfoFromField(field, type, scalarArrayCount);
			assert(type != EFieldType::UNKNOWN_OR_INVALID);

			switch (type)
			{
				case EFieldType::UNKNOWN_OR_INVALID:
					break;
				case EFieldType::STRING:
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
				case EFieldType::DATA_NODE:
				{
					assert(false && "NOT IMPLEMENTED!");
					break;
				}
				case EFieldType::GUID:
				{
					std::lock_guard<std::mutex> lock(s_globalDataClassMapMutex);
					CLib::Vector<GUID, 8, 8, true> guids;
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
				case EFieldType::INT:
					dstNode->SetInteger(field.m_name, reinterpret_cast<int*>(fieldData), scalarArrayCount);
					break;
				case EFieldType::FLOAT:
					dstNode->SetFloat(field.m_name, reinterpret_cast<float*>(fieldData), scalarArrayCount);
					break;
				case EFieldType::DOUBLE:
					dstNode->SetDouble(field.m_name, reinterpret_cast<double*>(fieldData), scalarArrayCount);
					break;
				case EFieldType::BOOL:
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
