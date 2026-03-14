#pragma once
#include "ReflectronAPI.h"
#include "CLib/Vector.h"
#include <cmath>

namespace Reflectron
{
	class Reflector
	{
	public:

		using ReflectionFields = CLib::Vector<ReflectronFieldData>;

		Reflector()
			: m_className(nullptr)
			, m_address(nullptr)
		{
		}

		template<typename T>
		void Init(T* instance)
		{
			using ReflectionDataType = typename T::Reflectron_Generated;

			m_className = ReflectionDataType::ClassName;
			m_address = reinterpret_cast<unsigned char*>(instance);

			ReflectionDataType reflectionData{};

			constexpr uint32_t FieldCount = uint32_t(ReflectionDataType::FieldCount);
			if constexpr (FieldCount == 1) // FieldCount is always at least 1, so ensure the first field is valid. If not, early exit before adding the invalid field.
			{
				if (reflectionData.m_fieldData[0].m_size == 0)
					return;
			}

			m_fields.SetCount(FieldCount);

			for(uint32_t i = 0; i < FieldCount; ++i)
			{
				const ReflectronFieldData& field = reflectionData.m_fieldData[i];
				if (field.m_name != nullptr)
				{
					m_fields[i] = field;
				}
			}
		}

		template<typename T>
		void AddFields(T* instance)
		{
			using ReflectionDataType = typename T::Reflectron_Generated;

			ReflectionDataType reflectionData{};

			constexpr uint32_t AdditionalFieldCount = uint32_t(ReflectionDataType::FieldCount);
			if constexpr (AdditionalFieldCount == 1) // AdditionalFieldCount is always at least 1, so ensure the first field is valid. If not, set the field count to zero.
			{
				if (reflectionData.m_fieldData[0].m_size == 0)
					return;
			}

			m_fields.Reserve(m_fields.Count() + AdditionalFieldCount);

			for(uint32_t i = 0; i < AdditionalFieldCount; ++i)
			{
				const ReflectronFieldData& field = reflectionData.m_fieldData[i];
				if (field.m_name != nullptr)
				{
					m_fields.PushBack() = field;
				}
			}
		}

		template<typename T>
		Reflector(T* instance)
		{
			Init(instance);
		}

		template<typename FieldType = void>
		FieldType* GetFieldValueWithName(const char* name)
		{
			for (auto& field : m_fields)
			{
				if (strcmp(field.m_name, name) == 0)
				{
					return GetFieldValue<FieldType>(field);
				}
			}

			return nullptr;
		}

		template<typename FieldType = void>
		const FieldType* GetFieldValueWithName(const char* name) const
		{
			return GetFieldValueWithName<FieldType>(name);
		}

		const ReflectronFieldData* GetFieldWithName(const char* name) const
		{
			for (auto& field : m_fields)
			{
				if (strcmp(field.m_name, name) == 0)
				{
					return &field;
				}
			}

			return nullptr;
		}

		template<typename FieldType = void>
		FieldType* GetFieldValue(const ReflectronFieldData& field) const
		{
			return reinterpret_cast<FieldType*>(m_address + field.m_offset);
		}

		template<typename FieldType = void>
		FieldType* GetFieldValueAtOffset(size_t offset) const
		{
			return reinterpret_cast<FieldType*>(m_address + offset);
		}

		const ReflectionFields& GetReflectionFields() const { return m_fields; }
		const size_t FieldCount() const { return m_fields.Count(); }

		template<typename T>
		T* GetAddress() { return reinterpret_cast<T*>(m_address); }
		template<typename T>
		const T* GetAddress() const { return reinterpret_cast<T*>(m_address); }

		unsigned char* GetAddress() { return m_address; }
		const unsigned char* GetAddress() const { return m_address; }

		const char* GetTypeName() const { return m_className; }

		static bool FieldIsBoundedBothEnds(const ReflectronFieldData& field)
		{
			return std::isfinite(field.m_editMinValue) && std::isfinite(field.m_editMaxValue);
		}

	private:

		const char* m_className;
		unsigned char* m_address;
		ReflectionFields m_fields;
	};
}