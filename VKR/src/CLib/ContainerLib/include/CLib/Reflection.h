#pragma once
#include "CLib/CLibAPI.h"

#include <string>
#include <cstddef>
#include <map>
#include <unordered_map>
#include <functional>

#include "boost/preprocessor/variadic/size.hpp"
#include "boost/preprocessor/variadic/to_seq.hpp"
#include "boost/preprocessor/seq/for_each_i.hpp"
#include "boost/preprocessor/stringize.hpp"

#pragma warning(push)
#pragma warning(disable : 26495) // Disable uninitialized variable warning for TReflectableClass. It's data is expected to be uninitialized.

namespace CLib
{
	namespace Reflection
	{
		struct CLibRefField
		{
			size_t m_typeHash;
			const char* m_name;
			size_t m_size;
			size_t m_offset;
		};

		struct ReflectableClass
		{
			ReflectableClass()
			{
			}

			CLIB_API ~ReflectableClass();
		};

		// Wrapper for a class which has been declared reflectable via CLIB_REFLECTABLE_CLASS(<ClassName>)
		// The wrapped class will not be automatically constructed, instead it should be constructed via PlacedConstructClass()
		// The wrapped class will automatically be destructed when the wrapper is destructed.
		template<typename T>
		struct TReflectableClass : public ReflectableClass
		{
			TReflectableClass()
			{
			}

			uint8_t m_data[sizeof(T)];

			T* operator -> ()
			{
				return reinterpret_cast<T*>(m_data);
			}

			T* operator & ()
			{
				return reinterpret_cast<T*>(m_data);
			}

			ReflectableClass* Get()
			{
				return this;
			}
		};

		using ClibClassInstantiateFunc = std::function<void* (void)>;
		using ClibClassDeleteFunc = std::function<void(void*)>;
		using ClibClassPlacedConstructFunc = std::function<void(void*)>;
		using ClibClassPlacedDeleteFunc = std::function<void(void*)>;
		struct ClibRefClassFuncs
		{
			ClibClassInstantiateFunc instFunc;
			ClibClassDeleteFunc delFunc;
			ClibClassPlacedConstructFunc constructFunc;
			ClibClassPlacedDeleteFunc destructFunc;
		};

		using ClassFuncMap = std::unordered_map<std::string, ClibRefClassFuncs>;
		class ClassReflection
		{
		public:
			CLIB_API static ClassFuncMap& GetClassFuncMap();
		};

		struct CLibClassStaticInitializer
		{
			CLibClassStaticInitializer
			(
				const char* name, 
				void* (*instFunc)(void), 
				void (*delFunc)(void*),
				void (*constructFunc)(void*),
				void (*destructFunc)(void*)
			)
			{
				ClibRefClassFuncs funcs
				{
					instFunc,
					delFunc,
					constructFunc,
					destructFunc
				};

				ClassReflection::GetClassFuncMap().insert({name, funcs});
			}
		};

		class Reflector
		{
		public:

			using ReflectionFields = std::map<std::string, CLibRefField>;

			Reflector()
				: m_typeName(nullptr)
				, m_address(nullptr)
			{
			}

			template<typename T>
			void Init(T* instance)
			{
				using ReflectionDataType = typename T::CLibRefFieldData;

				m_typeName = T::CLibClassName;
				m_address = reinterpret_cast<unsigned char*>(instance);

				ReflectionDataType reflectionData{};
				for (auto& field : reflectionData.fields)
				{
					m_fields.insert({ field.m_name, field });
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
					CLibRefField& field = fieldDataIt->second;
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

			const CLibRefField GetFieldDataWithName(const char* name) const
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

			const char* GetTypeName() const { return m_typeName; }

		private:

			const char* m_typeName;
			unsigned char* m_address;
			ReflectionFields m_fields;
		};
	}
};

// Implementation made possible thanks to Paul Fultz for his answer to the Stack Overflow question: https://stackoverflow.com/questions/41453/how-can-i-add-reflection-to-a-c-application

// Strip off the type
#define CLIB_STRIP(x) BOOST_PP_EAT x
// Show the type without parenthesis
#define CLIB_PAIR(x) BOOST_PP_REM x

#define CLIB_DECLARE_EACH(r, data, i, x) CLIB_PAIR(x);

#define CLIB_REFLECT_EACH(r, data, i, x)						\
CLib::Reflection::CLibRefField									\
{																\
	typeid(CLIB_STRIP(x)).hash_code(),							\
	BOOST_PP_STRINGIZE(CLIB_STRIP(x)),							\
	sizeof(CLIB_STRIP(x)),										\
	offsetof(CLibReflectedClassType, CLIB_STRIP(x)),			\
},																\

/*
Declare member variables within a class/struct with reflection data.
Data is accessible via CLib::Reflector::Reflector<class/struct T>.
Do not use this without at least one member declaration.
Syntax example:

CLIB_REFLECTABLE(<classname>,
	(bool) foo,
	CLIB_ARRAY(int, 4) bar)
*/
#define CLIB_REFLECTABLE(className, ...)														\
static constexpr int CLibRefFieldCount = BOOST_PP_VARIADIC_SIZE(__VA_ARGS__);					\
using CLibReflectedClassType = className;														\
																								\
using CLibReflectorType = CLib::Reflection::Reflector;											\
friend class CLibReflectorType;																	\
																								\
static inline const char* CLibClassName = BOOST_PP_STRINGIZE(className);						\
																								\
BOOST_PP_SEQ_FOR_EACH_I(CLIB_DECLARE_EACH, data, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))			\
																								\
struct CLibRefFieldData																			\
{																								\
	friend class CLibReflectorType;																\
	CLib::Reflection::CLibRefField fields[CLibRefFieldCount]{									\
		BOOST_PP_SEQ_FOR_EACH_I(CLIB_REFLECT_EACH, data, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))	\
	};																							\
};																								\

/*
Declare a wrapped C array for use as a reflectable field.
*/
#define CLIB_ARRAY(type, count)																\
(																							\
	struct																					\
	{																						\
		type m_data[count];																	\
		__forceinline type& operator [] (size_t i) { return m_data[i]; }					\
		__forceinline const type& operator [] (size_t i) const { return m_data[i]; }		\
		__forceinline type* begin() { return m_data; }										\
		__forceinline const type* begin() const { return m_data; }							\
		__forceinline type* end() { return &m_data[count]; }								\
		__forceinline const type* end() const { return &m_data[count]; }					\
		__forceinline const type& operator () () const { return m_data[0]; }				\
	}																						\
)																							\

#define CLIB_DECL_INSTANTIATE_FUNCTIONS(classType)						\
inline void* CLibInstantiate_##classType()								\
{																		\
	size_t classSize = sizeof(BOOST_PP_REM(##classType));				\
	void* mem = malloc(classSize);										\
	new (mem) BOOST_PP_REM(##classType);								\
	return mem;															\
}																		\
inline void CLibPlacedConstruct_##classType(void* block)				\
{																		\
	new (block) BOOST_PP_REM(##classType);								\
}																		\

#define CLIB_INSTANTIATE_FUNCTION_NAME(classType) CLibInstantiate_##classType
#define CLIB_PLACED_CONSTRUCT_FUNCTION_NAME(classType) CLibPlacedConstruct_##classType

#define CLIB_DECL_DELETE_FUNCTIONS(classType)									\
inline void CLibDelete_##classType(void* instance)								\
{																				\
	BOOST_PP_REM(##classType)* tptr = reinterpret_cast<##classType*>(instance);	\
	tptr->~##classType##();														\
	free(instance);																\
}																				\
inline void CLibPlacedDestruct_##classType(void* block)							\
{																				\
	reinterpret_cast<##classType*>(block)->~##classType();						\
}																				\

#define CLIB_DELETE_FUNCTION_NAME(classType) CLibDelete_##classType
#define CLIB_PLACED_DESTRUCT_FUNCTION_NAME(classType) CLibPlacedDestruct_##classType

/*
Mark a class type as reflectable, allowing for instantiation/deletion and
construction of the class by name string.
Use via:
CLib::Reflection::InstantiateClass,
CLib::Reflection::DeleteClass,
CLib::Reflection::PlacedConstructClass,
CLib::Reflection::PlacedDestructClass.
*/

#define CLIB_REFLECTABLE_CLASS(className)															\
inline const char* CLibClassTypeStr_##className = BOOST_PP_STRINGIZE(className);					\
																									\
CLIB_DECL_INSTANTIATE_FUNCTIONS(className)															\
CLIB_DECL_DELETE_FUNCTIONS(className)																\
																									\
inline CLib::Reflection::CLibClassStaticInitializer className##_static_init							\
(																									\
	CLibClassTypeStr_##className,																	\
	CLIB_INSTANTIATE_FUNCTION_NAME(className),														\
	CLIB_DELETE_FUNCTION_NAME(className),															\
	CLIB_PLACED_CONSTRUCT_FUNCTION_NAME(className),													\
	CLIB_PLACED_DESTRUCT_FUNCTION_NAME(className)													\
);																									\

namespace CLib
{
	namespace Reflection
	{
		template<typename T = void>
		T* InstantiateClass(const char* classTypeName)
		{
			auto instFunc = ClassReflection::GetClassFuncMap()[classTypeName].instFunc;
			void* ptr = instFunc();
			return static_cast<T*>(ptr);
		}

		CLIB_API ClibRefClassFuncs GetClassFunctions(const char* classTypeName);

		CLIB_API void DeleteClass(const char* classTypeName, void* ptr);

		CLIB_API void PlacedConstructClass(const char* classTypeName, ReflectableClass* c);

		CLIB_API void PlacedDestructClass(const char* classTypeName, ReflectableClass* c);
	}
}

#pragma warning(pop)
/* For some reason adding a comment here fixes E0007... Don't ask... */