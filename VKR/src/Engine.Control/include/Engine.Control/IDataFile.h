#pragma once
#include "ControlLib.h"
#include "GUID.h"
#include "CLib/Vector.h"

#include <functional>

namespace Ctrl
{
	enum class EFieldType
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

	struct Field
	{
		EFieldType m_type = EFieldType::UNKNOWN_OR_INVALID;
		CLib::Vector<uint8_t> m_data;
	};

	class IDataNode
	{
	public:

		CONTROL_INTERFACE void PeekFields(std::function<void(const std::string&, const Field&)> peekFunc) const = 0;

		/* Array getter -- Pointer will be invalidated if array size is changed. */
		CONTROL_INTERFACE const int* GetInteger(const char* name, uint32_t& outCount) const = 0;
		/* Single getter -- Get a single value or first array element. */
		CONTROL_INTERFACE int GetInteger(const char* name, int fallbackValue = 0) const = 0;

		/* Array getter -- Pointer will be invalidated if array size is changed. */
		CONTROL_INTERFACE const float* GetFloat(const char* name, uint32_t& outCount) const = 0;
		/* Single getter -- Get a single value or first array element. */
		CONTROL_INTERFACE float GetFloat(const char* name, float fallbackValue = 0.0f) const = 0;

		/* Array getter -- Pointer will be invalidated if array size is changed. */
		CONTROL_INTERFACE const double* GetDouble(const char* name, uint32_t& outCount) const = 0;
		/* Single getter -- Get a single value or first array element. */
		CONTROL_INTERFACE double GetDouble(const char* name, double fallbackValue = 0.0) const = 0;

		/* Array getter -- Pointer will be invalidated if array size is changed. */
		CONTROL_INTERFACE const bool* GetBool(const char* name, uint32_t& outCount) const = 0;
		/* Single getter -- Get a single value or first array element. */
		CONTROL_INTERFACE bool GetBool(const char* name, bool fallbackValue = false) const = 0;

		CONTROL_INTERFACE void GetString(const char* name, CLib::Vector<const char*>& outStr) const = 0;
		/* Single getter -- Get a single value or first array element. */
		CONTROL_INTERFACE const char* GetString(const char* name, const char* fallbackValue = nullptr) const = 0;

		/* Array getter -- Pointer will be invalidated if array size is changed. */
		CONTROL_INTERFACE IDataNode** GetDataNode(const char* name, uint32_t& outCount) = 0;
		/* Array getter -- Pointer will be invalidated if array size is changed. */
		CONTROL_INTERFACE const IDataNode* const* GetDataNode(const char* name, uint32_t& outCount) const = 0;
		CONTROL_INTERFACE const IDataNode* const* GetDataNode(const Field& field, uint32_t& outCount) const = 0;
		/* Single getter -- Get a single value or first array element. */
		CONTROL_INTERFACE IDataNode* GetDataNode(const char* name) = 0;
		/* Single getter -- Get a single value or first array element. */
		CONTROL_INTERFACE const IDataNode* GetDataNode(const char* name) const = 0;

		CONTROL_INTERFACE void GetAllChildDataNodes(CLib::Vector<IDataNode*>& outNodes) = 0;

		/* Array getter -- Pointer will be invalidated if array size is changed. */
		CONTROL_INTERFACE const GUID* GetGUID(const char* name, uint32_t& outCount) const = 0;
		CONTROL_INTERFACE const GUID* GetGUID(const Field& field, uint32_t& outCount) const = 0;
		/* Single getter -- Get a single value or first array element. */
		CONTROL_INTERFACE const GUID* GetGUID(const char* name) const = 0;

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

		CONTROL_INTERFACE IDataNode* AddDataNode(const char* name, const char* type = nullptr) = 0;
		CONTROL_INTERFACE IDataNode* GetOrAddDataNode(const char* name, const char* type = nullptr) = 0;

		CONTROL_INTERFACE void SetGUID(const char* name, const GUID* value, uint32_t count = 1) = 0;
		CONTROL_INTERFACE void SetGUID(const char* name, const GUID& value) = 0;

		// Remove field entirely unless a valid index is provided. Otherwise remove one array element at the index.
		CONTROL_INTERFACE void RemoveFieldOrNode(const char* name, uint32_t index = ~uint32_t(0)) = 0;

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
		CONTROL_API static void Destroy(IDataFile* file);

		CONTROL_INTERFACE EFileStatus Open(const char* fileName, EOpenMode openMode, bool allowFail = false) = 0;
		CONTROL_INTERFACE EFileStatus GetStatus() = 0;
		CONTROL_INTERFACE IDataNode* GetRoot() = 0;
		CONTROL_INTERFACE const IDataNode* GetRoot() const = 0;
		CONTROL_INTERFACE void ParseData() = 0;
		CONTROL_INTERFACE bool WriteData(const char* dstFilename = nullptr) = 0;
		CONTROL_INTERFACE void Close() = 0;
	};
}