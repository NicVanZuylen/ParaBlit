#pragma once
#include "Vector.h"

namespace CLib
{
	template<int FixedCapacity = 8, typename CharType = char>
	class String
	{
	public:

		String()
		{
			m_contents.PushBack('\0');
		}

		String(const String& other) { m_contents = other.m_contents; }

		String(const Vector<CharType>& other) 
		{ 
			m_contents = other;
			if (other.Back() != '\0') // Append null terminator if its missing.
			{
				m_contents.PushBack('\0');
			}
		}

		String(const CharType* cstr)
		{
			auto count = static_cast<uint32_t>(CStringLength(cstr)) + 1;
			m_contents.SetCount(count);
			memcpy(m_contents.Data(), cstr, sizeof(CharType) * count);
		}

		inline String& operator += (const String& other)
		{
			m_contents.PopBack(); // Remove null terminator.
			m_contents += other.m_contents;
			return *this;
		}

		inline String& operator += (const Vector<CharType>& other)
		{
			m_contents.PopBack(); // Remove null terminator.
			m_contents += other.m_contents;
			if (other.Back() != '\0') // Append null terminator if its missing.
			{
				m_contents.PushBack('\0');
			}
			return *this;
		}

		inline String& operator += (const CharType* cstr)
		{
			m_contents.PopBack(); // Remove null terminator.
			auto count = CStringLength(cstr) + 1;
			m_contents.Reserve(m_contents.Count() + count);
			memcpy(&m_contents.Back(), cstr, sizeof(CharType) * count); // Append C string.
			return *this;
		}

		inline uint32_t Count() { return m_contents.Count() - 1; } // Subtract 1 to remove null terminator.

		// The C String will be invalidated if the string instance is destroyed.
		inline const CharType* CString() { return m_contents.Data(); }

		// The C String will be invalidated if the string instance is destroyed.
		operator const CharType* () const { return m_contents.Data(); }

	private:

		uint32_t CStringLength(const CharType* cstr)
		{
			uint32_t count = 0;
			while (cstr[count] != '\0')
				++count;
			return count;
		}

		CLib::Vector<CharType, FixedCapacity> m_contents;
	};
}