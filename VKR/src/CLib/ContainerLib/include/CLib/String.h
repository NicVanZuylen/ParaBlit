#pragma once
#include "Clib/Vector.h"

namespace CLib
{
	template<int FixedCapacity, typename CharType>
	class TString
	{
	public:

		TString()
		{
			m_contents.PushBack('\0');
		}

		TString(const TString& other) { m_contents = other.m_contents; }

		TString(const Vector<CharType>& other)
		{ 
			m_contents = other;
			if (other.Back() != '\0') // Append null terminator if its missing.
			{
				m_contents.PushBack('\0');
			}
		}

		TString(const CharType* cstr)
		{
			auto count = static_cast<uint32_t>(CStringLength(cstr)) + 1;
			m_contents.SetCount(count);
			memcpy(m_contents.Data(), cstr, sizeof(CharType) * count);
		}

		TString(const std::initializer_list<CharType>& list)
		{
			m_contents = list;
			m_contents.PushBack('\0');
		}

		inline TString& operator += (const TString& other)
		{
			m_contents.PopBack(); // Remove null terminator.
			m_contents += other.m_contents;
			return *this;
		}

		inline TString& operator += (const Vector<CharType>& other)
		{
			m_contents.PopBack(); // Remove null terminator.
			m_contents += other.m_contents;
			if (other.Back() != '\0') // Append null terminator if its missing.
			{
				m_contents.PushBack('\0');
			}
			return *this;
		}

		inline TString& operator += (const CharType* cstr)
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

	using String = TString<8, char>;
}