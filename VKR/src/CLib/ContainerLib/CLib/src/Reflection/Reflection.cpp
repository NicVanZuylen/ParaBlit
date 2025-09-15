#include "CLib/Reflection.h"
#include <shared_mutex>

namespace CLib
{
	namespace Reflection
	{
		static ClassFuncMap CLib_classFuncMap;
		static std::unordered_map<void*, std::function<void(void*)>> CLib_classDestructMap;
		static std::mutex CLib_classDestructMapMutex;

		ClassFuncMap& ClassReflection::GetClassFuncMap()
		{
			return CLib_classFuncMap;
		}

		ReflectableClass::~ReflectableClass()
		{
			{
				std::lock_guard<std::mutex> lock(CLib_classDestructMapMutex);
				auto it = CLib_classDestructMap.find(this);
				if (it != CLib_classDestructMap.end())
				{
					it->second(this);
					CLib_classDestructMap.erase(it);
				}
			}
		}

		ClibRefClassFuncs GetClassFunctions(const char* classTypeName)
		{
			auto it = CLib_classFuncMap.find(classTypeName);
			if (it != CLib_classFuncMap.end())
			{
				return it->second;
			}

			return {};
		}

		void DeleteClass(const char* classTypeName, void* ptr)
		{
			CLib_classFuncMap[classTypeName].delFunc(ptr);
		}

		void PlacedConstructClass(const char* classTypeName, ReflectableClass* c)
		{
			auto& funcs = CLib_classFuncMap[classTypeName];
			funcs.constructFunc(c);

			{
				std::lock_guard<std::mutex> lock(CLib_classDestructMapMutex);
				CLib_classDestructMap.insert({ c, funcs.destructFunc });
			}
		}

		void PlacedDestructClass(const char* classTypeName, ReflectableClass* c)
		{
			void* base = c;

			{
				std::lock_guard<std::mutex> lock(CLib_classDestructMapMutex);
				auto it = CLib_classDestructMap.find(base);
				it->second(base);
				CLib_classDestructMap.erase(it);
			}
		}
	}
}