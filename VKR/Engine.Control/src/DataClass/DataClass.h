#pragma once
#include "../DataFile/DataFile.h"
#include <CLib/Reflection.h>

namespace Ctrl
{
	class DataClass
	{
	public:

		DataClass(CLib::Reflection::Reflector* reflection);
		~DataClass();

		CLib::Reflection::Reflector* GetReflection() { return m_reflector; }
		const CLib::Reflection::Reflector* GetReflection() const { return m_reflector; }

		void FillFromDataNode(const IDataNode* node);

	private:

		friend class ObjectPtr;

		static std::unordered_map<Ctrl::GUID, DataClass*> s_globalDataClassMap;
		static std::mutex s_globalDataClassMapMutex;

		CLib::Reflection::Reflector* m_reflector = nullptr;
		GUID m_guid;
	};

	class ObjectPtr
	{
	public:

		inline void Assign(const GUID& guid)
		{
			if (guid.IsValid())
			{
				auto& map = DataClass::s_globalDataClassMap;

				auto it = map.find(guid);
				if (it != map.end())
				{
					m_ptr = &it->second;
				}
				else
				{
					m_ptr = &map.insert({ guid, nullptr }).first->second;
				}
			}
		}

		inline ObjectPtr* operator = (const DataClass* dataClass)
		{
			Assign(dataClass->m_guid);
			return this;
		}

	protected:

		void* m_ptr = nullptr; // This is a pointer to a pointer (void**)
	};

	template<class T>
	class TObjectPtr : public ObjectPtr
	{
		inline T* operator -> ()
		{
			return *reinterpret_cast<T**>(m_ptr);
		}

		inline TObjectPtr<T> operator = (const DataClass* dataClass)
		{
			Assign(dataClass->m_guid);
			return this;
		}
	};
}