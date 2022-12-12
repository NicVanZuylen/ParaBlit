#pragma once
#include "ControlLib.h"

#include <cstdint>

namespace Ctrl
{
	class IDataContainer
	{
	public:

		CONTROL_INTERFACE const char* GetStringValue(const char* token) const = 0;
		CONTROL_INTERFACE int GetIntegerValue(const char* token) const = 0;
		CONTROL_INTERFACE uint32_t GetUIntegerValue(const char* token) const = 0;
		CONTROL_INTERFACE bool GetBooleanValue(const char* token) const = 0;
		CONTROL_INTERFACE bool HasToken(const char* token) const = 0;
		CONTROL_INTERFACE bool HasValue(const char* token) const = 0;
		CONTROL_INTERFACE const char* GetRawInputString() const = 0;

		CONTROL_INTERFACE void SetStringValue(const char* token, const char* value) = 0;
		CONTROL_INTERFACE void SetIntegerValue(const char* token, int value) = 0;
		CONTROL_INTERFACE void SetUIntegerValue(const char* token, uint32_t value) = 0;
		CONTROL_INTERFACE void SetBooleanValue(const char* token, bool value) = 0;
	};

	class ICommandLine
	{
	public:

		static ICommandLine* Create(int argc, char** argv);
		static void Destroy(ICommandLine* parser);

		CONTROL_INTERFACE const IDataContainer* GetData() const = 0;
	};

	class IConfigFile
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

		static IConfigFile* Create(const char* filePath, EOpenMode openMode = EOpenMode::OPEN_READ_ONLY);
		static void Destroy(IConfigFile* file);

		CONTROL_INTERFACE EFileStatus GetStatus() = 0;
		CONTROL_INTERFACE IDataContainer* GetData() = 0;
		CONTROL_INTERFACE const IDataContainer* GetData() const = 0;

		CONTROL_INTERFACE bool WriteData() = 0;
	};

	class ISettingsHub
	{
	public:

		enum class EMergeBehavior
		{
			OVERWRITE,
			IGNORE_DUPLICATES
		};

		static ISettingsHub* GetOrCreate();
		static void Destroy();

		CONTROL_INTERFACE void AddSettings(const IDataContainer* parser, EMergeBehavior mergeBehavior = EMergeBehavior::OVERWRITE) = 0;
		CONTROL_INTERFACE const char* GetStringValue(const char* token) = 0;
		CONTROL_INTERFACE int GetIntegerValue(const char* token) = 0;
		CONTROL_INTERFACE uint32_t GetUIntegerValue(const char* token) = 0;
		CONTROL_INTERFACE bool GetBooleanValue(const char* token) = 0;
		CONTROL_INTERFACE bool HasToken(const char* token) = 0;
		CONTROL_INTERFACE bool HasValue(const char* token) = 0;
	};
}