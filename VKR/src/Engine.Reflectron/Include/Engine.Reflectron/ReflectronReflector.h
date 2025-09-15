#pragma once
#include "ReflectronAPI.h"
#include <string>
#include <map>

namespace Reflectron
{
	class Reflector
	{
	public:

		using ReflectionFields = std::map<std::string, const ReflectronFieldData>;

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
			for (const ReflectronFieldData& field : reflectionData.m_fieldData)
			{
				if (field.m_name != nullptr)
				{
					m_fields.insert({ field.m_name, field });
				}
			}
		}

		template<typename T>
		Reflector(T* instance)
		{
			Init(instance);
		}

		template<typename FieldType = void>
		FieldType* GetFieldWithName(const char* name, size_t* fieldSize = nullptr)
		{
			auto fieldDataIt = m_fields.find(name);
			if (fieldDataIt != m_fields.end())
			{
				const ReflectronFieldData& field = fieldDataIt->second;
				if (fieldSize != nullptr)
				{
					*fieldSize = field.m_size;
				}
				return GetFieldAtOffset<FieldType>(field.m_offset);
			}

			return nullptr;
		}

		template<typename FieldType = void>
		const FieldType* GetFieldWithName(const char* name, size_t* fieldSize = nullptr) const
		{
			return GetFieldWithName<FieldType>(name, fieldSize);
		}

		const ReflectronFieldData& GetFieldDataWithName(const char* name) const
		{
			auto it = m_fields.find(name);
			if (it != m_fields.end())
			{
				return it->second;
			}

			return {};
		}

		template<typename FieldType = void>
		FieldType* GetFieldAtOffset(size_t offset) const
		{
			return reinterpret_cast<FieldType*>(m_address + offset);
		}

		const ReflectionFields& GetReflectionFields() const { return m_fields; }
		const size_t FieldCount() const { return m_fields.size(); }

		template<typename T>
		T* GetAddress() { return reinterpret_cast<T*>(m_address); }
		template<typename T>
		const T* GetAddress() const { return reinterpret_cast<T*>(m_address); }

		unsigned char* GetAddress() { return m_address; }
		const unsigned char* GetAddress() const { return m_address; }

		const char* GetTypeName() const { return m_className; }

	private:

		const char* m_className;
		unsigned char* m_address;
		ReflectionFields m_fields;
	};
}