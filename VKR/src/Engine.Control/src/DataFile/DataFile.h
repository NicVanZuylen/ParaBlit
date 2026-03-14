#pragma once
#include "CLib/Vector.h"
#include "Engine.Control/IDataFile.h"
#include "pugixml/pugixml.hpp"
#include "CLib/Allocator.h"

#include <sstream>
#include <map>

namespace Ctrl
{
	static CLib::Allocator s_dataAllocator{ 1024 * 1024, true };

	class DataFile;

	class DataNode : public IDataNode
	{
	public:

		DataNode(const char* type = nullptr);
		DataNode(const pugi::xml_node& node);
		~DataNode();

		void InitializeWithNode(const pugi::xml_node& node);
		void WriteData(pugi::xml_node& parent, const char* name, bool includeSelf = true);

		CONTROL_API void PeekFields(std::function<void(const std::string&, const Field&)> peekFunc) const override;

		CONTROL_API const int* GetInteger(const char* name, uint32_t& outCount) const override;
		CONTROL_API int GetInteger(const char* name, int fallbackValue) const override;
		CONTROL_API const float* GetFloat(const char* name, uint32_t& outCount) const override;
		CONTROL_API float GetFloat(const char* name, float fallbackValue) const override;
		CONTROL_API const double* GetDouble(const char* name, uint32_t& outCount) const override;
		CONTROL_API double GetDouble(const char* name, double fallbackValue) const override;
		CONTROL_API const bool* GetBool(const char* name, uint32_t& outCount) const override;
		CONTROL_API bool GetBool(const char* name, bool fallbackValue) const override;
		CONTROL_API void GetString(const char* name, CLib::Vector<const char*>& outStr) const override;
		CONTROL_API const char* GetString(const char* name, const char* fallbackValue) const override;
		CONTROL_API IDataNode** GetDataNode(const char* name, uint32_t& outCount) override;
		CONTROL_API const IDataNode* const* GetDataNode(const char* name, uint32_t& outCount) const override;
		CONTROL_API const IDataNode* const* GetDataNode(const Field& field, uint32_t& outCount) const override;
		CONTROL_API IDataNode* GetDataNode(const char* name) override;
		CONTROL_API const IDataNode* GetDataNode(const char* name) const override;
		CONTROL_API void GetAllChildDataNodes(CLib::Vector<IDataNode*>& outNodes) override;
		CONTROL_API const GUID* GetGUID(const char* name, uint32_t& outCount) const override;
		CONTROL_API const GUID* GetGUID(const Field& field, uint32_t& outCount) const override;
		CONTROL_API const GUID* GetGUID(const char* name) const override;

		CONTROL_API void SetInteger(const char* name, int* value, uint32_t count = 1) override;
		CONTROL_API void SetInteger(const char* name, int value) override;
		CONTROL_API void SetFloat(const char* name, float* value, uint32_t count = 1) override;
		CONTROL_API void SetFloat(const char* name, float value) override;
		CONTROL_API void SetDouble(const char* name, double* value, uint32_t count = 1) override;
		CONTROL_API void SetDouble(const char* name, double value) override;
		CONTROL_API void SetBool(const char* name, bool* value, uint32_t count = 1) override;
		CONTROL_API void SetBool(const char* name, bool value) override;
		CONTROL_API void SetString(const char* name, const char** value, uint32_t count = 1) override;
		CONTROL_API void SetString(const char* name, const char* value) override;
		CONTROL_API IDataNode* AddDataNode(const char* name, const char* type = nullptr) override;
		CONTROL_API IDataNode* GetOrAddDataNode(const char* name, const char* type = nullptr) override;
		CONTROL_API void SetGUID(const char* name, const GUID* value, uint32_t count = 1) override;
		CONTROL_API void SetGUID(const char* name, const GUID& value) override;

		CONTROL_API void RemoveFieldOrNode(const char* name, uint32_t index) override;

		CONTROL_API void SetSelfGUID(const GUID& value) override;
		CONTROL_API const GUID& GetSelfGUID() const override;

		CONTROL_API const char* GetTypeName() const override;

	private:

		IDataNode* AddDataNode(const char* name, const pugi::xml_node* nodeData, const char* type = nullptr);

		void ReadChildren(pugi::xml_node& node);
		void FreeFieldData(Field& field, uint32_t index = ~uint32_t(0));
		void WriteIntArray(std::stringstream& outStr, int* data, uint32_t count);

		template<typename T>
		void StringifyArray(std::stringstream& outStr, void* data, uint32_t byteCount)
		{
			T* typedData = reinterpret_cast<T*>(data);
			uint32_t count = byteCount / sizeof(T);
			for (uint32_t i = 0; i < count; ++i)
			{
				if (i != 0)
				{
					outStr << ", ";
				}
				outStr << typedData[i];
			}
		}

		void StringifyStringArray(std::stringstream& outStr, CLib::Vector<const char*>& strArray);
		void StringifyGUIDArray(std::stringstream& outStr, const GUID* guidArray, uint32_t count);
		
		std::string m_type;
		GUID m_guid = nullGUID;
		std::map<const std::string, Field> m_fields;
	};

	class DataFile : public IDataFile
	{
	public:

		DataFile() = default;
		~DataFile();

		CONTROL_API EFileStatus Open(const char* fileName, EOpenMode openMode, bool allowFail) override;
		CONTROL_API EFileStatus GetStatus() override { return m_status; };
		CONTROL_API IDataNode* GetRoot() override { return m_base; };
		CONTROL_API const IDataNode* GetRoot() const override { return m_base; };
		CONTROL_API void ParseData() override;
		CONTROL_API bool WriteData(const char* dstFilename) override;
		CONTROL_API void Close() override;

	private:

		EFileStatus m_status = EFileStatus::CANT_OPEN;
		std::string m_fileName;
		pugi::xml_document m_document;
		DataNode* m_base = nullptr;
	};
}