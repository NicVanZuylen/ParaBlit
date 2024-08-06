#include "Engine.Control/GUID.h"
#include "../MurmurHash/MurmurHash3.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <cassert>

#pragma warning (push)
#pragma warning (disable : 26495) // m_data member of GUID is intentionally uninitialized.

namespace Ctrl
{
	uint8_t RandomChar()
	{
		std::random_device rand;
		std::mt19937 gen(rand());
		std::uniform_int_distribution dis(0, 255);
		return uint8_t(dis(gen));
	}

	GUID::GUID()
	{
		m_data[GUIDLength] = ~char(0); // Null terminator character is used for validation.
	}

	GUID::GUID(const char* guidStr)
	{
		if (guidStr == nullptr)
			return;

		std::memcpy(m_data, guidStr, GUIDLength);
		m_data[GUIDLength] = '\0';
	}

	GUID::~GUID()
	{
	}

	bool GUID::IsValid() const
	{
		return m_data[GUIDLength] == '\0';
	}

	bool GUID::operator==(const GUID& other) const
	{
		return memcmp(m_data, other.m_data, GUIDLength) == 0;
	}

	bool GUID::operator==(const char* guidStr) const
	{
		return strcmp(guidStr, m_data) == 0;
	}

	bool GUID::operator==(const std::string& guidStr) const
	{
		return strcmp(guidStr.c_str(), m_data) == 0;
	}

	size_t GUID::Hash() const
	{
		return MurmurHash3_x64_64(m_data, GUIDLength, 0);
	}

	size_t GUID::operator() () const
	{
		return Hash();
	}

	GUID& GUID::operator=(const char* guidStr)
	{
		assert(strlen(guidStr) >= GUIDLength);
		std::memcpy(m_data, guidStr, GUIDLength);
		m_data[GUIDLength] = '\0';
		return *this;
	}

	GUID& GUID::operator=(const std::string& guidStr)
	{
		assert(guidStr.size() >= GUIDLength);

		std::memcpy(m_data, guidStr.c_str(), GUIDLength);
		m_data[GUIDLength] = '\0';
		return *this;
	}

	GUID& GUID::operator=(const GUID& other)
	{
		assert(other.IsValid());

		std::memcpy(m_data, other.m_data, GUIDLength + 1);
		return *this;
	}

	void GenerateGUID(GUID& outGUID)
	{
		std::stringstream strBuf;
		strBuf << std::setfill('0') << std::setw(2);
		for (uint32_t i = 0; i < GUIDLength / 2; ++i)
		{
			uint8_t randChar = RandomChar();
			// Ensure hex width is 2 as since guids are fixed-length, the otherwise truncated zeros from hex values should be included.
			strBuf << std::setfill('0') << std::setw(2) << std::hex << int(randChar);
		}

		strBuf.seekp(23);
		strBuf << '-';
		strBuf.seekp(18);
		strBuf << '-';
		strBuf.seekp(13);
		strBuf << '-';
		strBuf.seekp(8);
		strBuf << '-';

		// Ensure fixed length.
		strBuf.seekg(0, std::ios::end);
		assert(strBuf.tellg() == GUIDLength);

		strBuf.seekg(0, std::ios::beg);
		strBuf.read(outGUID.m_data, GUIDLength);
		outGUID.m_data[GUIDLength] = '\0';
	}

	GUID GenerateGUID()
	{
		GUID guid;
		GenerateGUID(guid);
		return guid;
	}
}

size_t std::hash<Ctrl::GUID>::operator()(const Ctrl::GUID& guid) const
{
	return guid.Hash();
}

#pragma warning (pop)