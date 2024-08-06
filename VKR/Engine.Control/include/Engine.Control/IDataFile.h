#pragma once
#include "ControlLib.h"
#include "GUID.h"
#include "CLib/Vector.h"

#include <functional>

namespace Ctrl
{
	enum class EAttributeType
	{
		UNKNOWN_OR_INVALID,
		INT,
		FLOAT,
		DOUBLE,
		BOOL,
		STRING,
		DATA_NODE,
		GUID
	};

	struct Attribute
	{
		EAttributeType m_type = EAttributeType::UNKNOWN_OR_INVALID;
		CLib::Vector<uint8_t> m_data;
	};

	class IDataNode
	{
	public:

		CONTROL_INTERFACE void PeekAttributes(std::function<void(const std::string&, const Attribute&)> peekFunc) const = 0;

		CONTROL_INTERFACE const int* GetInteger(const char* name, uint32_t& outCount) const = 0;
		CONTROL_INTERFACE int GetInteger(const char* name) const = 0;

		CONTROL_INTERFACE const float* GetFloat(const char* name, uint32_t& outCount) const = 0;
		CONTROL_INTERFACE float GetFloat(const char* name) const = 0;

		CONTROL_INTERFACE const double* GetDouble(const char* name, uint32_t& outCount) const = 0;
		CONTROL_INTERFACE double GetDouble(const char* name) const = 0;

		CONTROL_INTERFACE const bool* GetBool(const char* name, uint32_t& outCount) const = 0;
		CONTROL_INTERFACE bool GetBool(const char* name) const = 0;

		CONTROL_INTERFACE void GetString(const char* name, CLib::Vector<const char*>& outStr) const = 0;
		CONTROL_INTERFACE const char* GetString(const char* name) const = 0;

		CONTROL_INTERFACE IDataNode* GetDataNode(const char* name) = 0;
		CONTROL_INTERFACE const IDataNode* GetDataNode(const char* name) const = 0;

		CONTROL_INTERFACE const char* GetGUID(const char* name, uint32_t& outCount) const = 0;
		CONTROL_INTERFACE const char* GetGUID(const char* name) const = 0;

		CONTROL_INTERFACE void SetInteger(const char* name, int* value, uint32_t count = 1) = 0;
		CONTROL_INTERFACE void SetInteger(const char* name, int value) = 0;

		CONTROL_INTERFACE void SetFloat(const char* name, float* value, uint32_t count = 1) = 0;
		CONTROL_INTERFACE void SetFloat(const char* name, float value) = 0;

		CONTROL_INTERFACE void SetDouble(const char* name, double* value, uint32_t count = 1) = 0;
		CONTROL_INTERFACE void SetDouble(const char* name, double value) = 0;

		CONTROL_INTERFACE void SetBool(const char* name, bool* value, uint32_t count = 1) = 0;
		CONTROL_INTERFACE void SetBool(const char* name, bool value) = 0;

		CONTROL_INTERFACE void SetString(const char* name, const char** value, uint32_t count = 1) = 0;
		CONTROL_INTERFACE void SetString(const char* name, const char* value) = 0;

		CONTROL_INTERFACE IDataNode* GetOrAddDataNode(const char* name, const char* type = nullptr) = 0;

		CONTROL_INTERFACE void SetGUID(const char* name, const char** value, uint32_t count = 1) = 0;
		CONTROL_INTERFACE void SetGUID(const char* name, const char* value) = 0;

		CONTROL_INTERFACE void RemoveAttributeOrNode(const char* name) = 0;

		CONTROL_INTERFACE void SetSelfGUID(const GUID& value) = 0;
		CONTROL_INTERFACE const GUID& GetSelfGUID() const = 0;

		CONTROL_INTERFACE const char* GetTypeName() const = 0;
	};

	class IDataFile
	{
	public:

		enum EOpenMode
		{
			OPEN_READ_ONLY,
			OPEN_READ_WRITE
		};

		enum EFileStatus
		{
			CANT_OPEN,
			OPENED_SUCCESSFULLY
		};

		CONTROL_API static IDataFile* Create();
		CONTROL_API static IDataFile* Create(const char* fileName, EOpenMode openMode);
		CONTROL_API static void Destroy(IDataFile* file);

		CONTROL_INTERFACE void Open(const char* fileName, EOpenMode openMode) = 0;
		CONTROL_INTERFACE IDataNode* GetRoot() = 0;
		CONTROL_INTERFACE void ParseData() = 0;
		CONTROL_INTERFACE bool WriteData() = 0;
		CONTROL_INTERFACE void Close() = 0;
	};
}