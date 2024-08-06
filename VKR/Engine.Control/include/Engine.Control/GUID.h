#pragma once
#include "ControlLib.h"
#include <string>

namespace Ctrl
{
	const uint32_t GUIDLength = 36;
	struct GUID
	{
		CONTROL_API GUID();
		CONTROL_API GUID(const char* guidStr);
		CONTROL_API ~GUID();

		CONTROL_API bool IsValid() const;
		inline const char* AsCString() const { return m_data; };

		inline operator const char* () const { return m_data; };
		CONTROL_API GUID& operator = (const char* guidStr);
		CONTROL_API GUID& operator = (const std::string& guidStr);
		CONTROL_API GUID& operator = (const GUID& other);
		CONTROL_API bool operator == (const GUID& other) const;
		CONTROL_API bool operator == (const char* guidStr) const;
		CONTROL_API bool operator == (const std::string& guidStr) const;

		CONTROL_API size_t Hash() const;
		CONTROL_API size_t operator () () const;

		char m_data[GUIDLength + 1];
	};

	inline GUID nullGUID = {};

	CONTROL_API void GenerateGUID(GUID& outGUID);
	CONTROL_API GUID GenerateGUID();
}

namespace std
{
	template <>
	struct std::hash<Ctrl::GUID>
	{
		CONTROL_API size_t operator () (const Ctrl::GUID& guid) const;
	};
}