#pragma once
#include "Engine.Control/IDataFile.h"
#include "../src/pugixml/pugixml.hpp"
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
		DataNode(pugi::xml_node& node);
		~DataNode();

		void InitializeWithNode(pugi::xml_node& node);
		void WriteData(pugi::xml_node& parent, const char* name, bool includeSelf = true);

		CONTROL_API void PeekAttributes(std::function<void(const std::string&, const Attribute&)> peekFunc) const override;

		CONTROL_API const int* GetInteger(const char* name, uint32_t& outCount) const override;
		CONTROL_API int GetInteger(const char* name) const override;
		CONTROL_API const float* GetFloat(const char* name, uint32_t& outCount) const override;
		CONTROL_API float GetFloat(const char* name) const override;
		CONTROL_API const double* GetDouble(const char* name, uint32_t& outCount) const override;
		CONTROL_API double GetDouble(const char* name) const override;
		CONTROL_API const bool* GetBool(const char* name, uint32_t& outCount) const override;
		CONTROL_API bool GetBool(const char* name) const override;
		CONTROL_API void GetString(const char* name, CLib::Vector<const char*>& outStr) const override;
		CONTROL_API const char* GetString(const char* name) const override;
		CONTROL_API IDataNode* GetDataNode(const char* name) override;
		CONTROL_API const IDataNode* GetDataNode(const char* name) const override;
		CONTROL_API const char* GetGUID(const char* name, uint32_t& outCount) const override;
		CONTROL_API const char* GetGUID(const char* name) const override;

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
		CONTROL_API IDataNode* GetOrAddDataNode(const char* name, const char* type = nullptr) override;
		CONTROL_API void SetGUID(const char* name, const char** value, uint32_t count = 1) override;
		CONTROL_API void SetGUID(const char* name, const char* value) override;

		CONTROL_API void RemoveAttributeOrNode(const char* name) override;

		CONTROL_API void SetSelfGUID(const GUID& value) override;
		CONTROL_API const GUID& GetSelfGUID() const override;

		CONTROL_API const char* GetTypeName() const override;

	private:

		void ReadChildren(pugi::xml_node& node);
		void FreeAttributeData(Attribute& attribute);
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
		void StringifyGUIDArray(std::stringstream& outStr, const char* guidArray, uint32_t count);
		
		std::string m_type;
		GUID m_guid;
		std::map<const std::string, Attribute> m_attributes;
	};

	class DataFile : public IDataFile
	{
	public:

		DataFile() = default;
		~DataFile();

		CONTROL_API void Open(const char* fileName, EOpenMode openMode) override;
		CONTROL_API IDataNode* GetRoot() override;
		CONTROL_API void ParseData() override;
		CONTROL_API bool WriteData() override;
		CONTROL_API void Close() override;

	private:

		EFileStatus m_status = EFileStatus::CANT_OPEN;
		std::string m_fileName;
		pugi::xml_document m_document;
		DataNode* m_base = nullptr;
	};
}