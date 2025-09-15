#pragma once
#include "ControlLib.h"
#include "IDataFile.h"
#include "CLib/Reflection.h"
#include "Engine.Reflectron/ReflectronReflector.h"
#include "GUID.h"

#include <cassert>
#include <mutex>
#include <atomic>

#define ENABLE_VERBOSE_REFCOUNT_LOGGING 0
#if ENABLE_VERBOSE_REFCOUNT_LOGGING
#define LOG_REF(...) printf(__VA_ARGS__);
#else
#define LOG_REF(...)
#endif

namespace Ctrl
{
	class DataClass
	{
	public:

		CONTROL_API static void InstantiateNodeTree(const Ctrl::IDataNode* root);
		CONTROL_API static void SaveNodeTree(Ctrl::IDataNode* root);

		template<typename T>
		DataClass(T* instance)
		{
			m_reflector.Init<T>(instance);
		}
		CONTROL_API ~DataClass();

		Reflectron::Reflector& GetReflection() { return m_reflector; }
		const Reflectron::Reflector& GetReflection() const { return m_reflector; }

		DataClass* DerivedAddress() { return reinterpret_cast<DataClass*>(m_reflector.GetAddress()); }

		CONTROL_API void FillFromDataNode(const IDataNode* node);
		CONTROL_API void SaveToDataNode(IDataNode* parentNode, IDataNode* dstNode) const;

		/*
		This dummy virtual function is here to ensure the VTable (_vfptr) pointer is located within this class.
		Why? If there's no virtual function here and we create a virtual function in a derived class,
		the offset of this class will be incorrect when instantiating a derived class via CLib class reflection.
		Having this here ensures the offset of Ctrl::DataClass in any inheritance tree will be zero.
		*/
		virtual void VTablePtrDummy() {}

	private:

		friend class ObjectPtr;
		friend struct DataClassStaticTypeMapping;

		struct DataClassTracking
		{
			DataClass* m_ptr = nullptr;
			std::atomic_int m_refCount = 0;

			~DataClassTracking()
			{
				assert(m_ptr == nullptr && m_refCount.load() < 1);
			}

			DataClassTracking& operator = (const DataClassTracking& other)
			{
				m_ptr = other.m_ptr;
				m_refCount = other.m_refCount.load();
				return *this;
			}

			CONTROL_API void FreeDataClass();

			inline int AddRef()
			{
				int count = m_refCount.fetch_add(1) + 1;
				LOG_REF("AddRef: %s Count: %i\n", m_ptr ? m_ptr->GetReflection().GetTypeName() : "", count);
				return count;
			}
			inline int RemRef()
			{
				int count = m_refCount.fetch_sub(1) - 1;
				LOG_REF("RemRef: %s Count: %i\n", m_ptr ? m_ptr->GetReflection().GetTypeName() : "", count);
				if (count < 1 && m_ptr != nullptr)
				{
					LOG_REF("Delete: %s\n", m_ptr->GetReflection().GetTypeName());
					FreeDataClass();
				}
				return count;
			}
		};

		static inline std::unordered_map<std::string, std::pair<EAttributeType, size_t>> s_typeMap =
		{
			{ "int", { EAttributeType::INT, 1 } },
			{ "int32_t", { EAttributeType::INT, 1 } },
			{ "unsigned int", { EAttributeType::INT, 1 } },
			{ "uint32_t", { EAttributeType::INT, 1 } },
			{ "float", { EAttributeType::FLOAT, 1 } },
			{ "double", { EAttributeType::DOUBLE, 1 } },
			{ "std::string", { EAttributeType::STRING, 1 } },
		};
		static std::unordered_map<Ctrl::GUID, DataClassTracking> s_globalDataClassMap;
		static std::unordered_map<DataClass**, Ctrl::GUID> s_globalGUIDMap;
		static std::mutex s_globalDataClassMapMutex;

		void AssignNewGUID(const GUID& guid);

		using FieldTypeData = std::pair<Ctrl::EAttributeType, uint32_t>; // Type, Count

		Reflectron::Reflector m_reflector;
		GUID m_guid = nullGUID;
	};

	struct DataClassStaticTypeMapping
	{
		DataClassStaticTypeMapping(const char* fromType, const char* toType, size_t toTypeCount)
		{
			auto& toAttribute = DataClass::s_typeMap[toType];
			DataClass::s_typeMap.insert({ fromType, { toAttribute.first, toAttribute.second * toTypeCount } });
		}
	};

	inline DataClass* NullObjectPtr = nullptr;

	/*
	Pointer wrapper to a Dataclass instance which can be referenced by GUID.
	*/
	class ObjectPtr
	{
	public:

		explicit ObjectPtr() = default;
		explicit ObjectPtr(const GUID& guid)
		{
			Assign(guid);
		}
		ObjectPtr(const DataClass* dataClass)
		{
			AssignDataClass(dataClass);
		}
		explicit ObjectPtr(const ObjectPtr& other)
		{
			m_ptr = other.m_ptr;
			GetTrackingPtr()->AddRef();
		}
		~ObjectPtr()
		{
			Invalidate();
		}

		void Invalidate()
		{
			GetTrackingPtr()->RemRef();
			m_ptr = &NullObjectPtr;
		}

		CONTROL_API void AssignDataClass(const DataClass* dataClass);

		CONTROL_API void Assign(const GUID& guid);
		ObjectPtr& operator = (const GUID& guid)
		{
			Assign(guid);
			return *this;
		}

		ObjectPtr& operator = (const DataClass* dataClass)
		{
			AssignDataClass(dataClass);
			return *this;
		}

		ObjectPtr& operator = (const ObjectPtr& other)
		{
			m_ptr = other.m_ptr;
			GetTrackingPtr()->AddRef();
			return *this;
		}

		bool operator == (const DataClass* rawPtr) const
		{
			return *m_ptr == rawPtr;
		}

		bool operator == (const ObjectPtr& ptr) const
		{
			return m_ptr == ptr.m_ptr;
		}

		DataClass* GetPtr() const { return *m_ptr; }
		DataClass** GetBasePtr() const { return m_ptr; }

		struct Hash
		{
			size_t operator ()(const ObjectPtr& ptr) const
			{
				return std::hash<uintptr_t>()(uintptr_t(ptr.GetPtr()));
			}
		};

	protected:

		DataClass::DataClassTracking* GetTrackingPtr() const { return reinterpret_cast<DataClass::DataClassTracking*>(m_ptr); }

		DataClass** m_ptr = &NullObjectPtr; // This is a pointer another pointer residing within DataClass::s_globalDataClassMap.
	};

	template<class T>
	class TObjectPtr : public ObjectPtr
	{
	public:

		TObjectPtr() = default;

		TObjectPtr(const GUID& guid)
		{
			Assign(guid);
		}
		TObjectPtr(const T* ptr)
		{
			AssignDataClass(ptr);
		}
		TObjectPtr(const ObjectPtr& ptr) 
		{
			m_ptr = ptr.GetBasePtr();
			GetTrackingPtr()->AddRef();
		}

		T* operator -> () { return reinterpret_cast<T*>(*m_ptr); }
		const T* operator -> () const { return reinterpret_cast<T*>(*m_ptr); }

		operator T*() { return reinterpret_cast<T*>(*m_ptr); }
		operator const T* () const { return reinterpret_cast<T*>(*m_ptr); }

		TObjectPtr<T>& operator = (const ObjectPtr& ptr)
		{
			m_ptr = ptr.m_ptr;
			GetTrackingPtr()->AddRef();
			return *this;
		}

		TObjectPtr<T>& operator = (DataClass* dataClass)
		{
			AssignDataClass(dataClass);
			return *this;
		}
	};
	static_assert(sizeof(TObjectPtr<void*>) == sizeof(ObjectPtr));

	class ObjectPtrArray : public CLib::Vector<ObjectPtr>
	{
	public:
		using VectorT = CLib::Vector<ObjectPtr>;

		void Clear()
		{
			for (auto& ptr : *this)
			{
				ptr.Invalidate();
			}
			VectorT::Clear();
		}

		~ObjectPtrArray()
		{
			Clear();
		}
	};

	template<class T>
	class TObjectPtrArray : public ObjectPtrArray
	{
	public:
		using ObjT = TObjectPtr<T>;

		ObjT* begin() { return reinterpret_cast<ObjT*>(VectorT::begin()); }
		const ObjT* begin() const { return reinterpret_cast<const ObjT*>(VectorT::begin()); }
		ObjT* end() { return reinterpret_cast<ObjT*>(VectorT::end()); }
		const ObjT* end() const { return reinterpret_cast<const ObjT*>(VectorT::end()); }

		ObjT operator [] (size_t index) { return ObjT(At(index)); }
		const ObjT operator [] (size_t index) const { return ObjT(At(index)); }
	};
}

#define DATA_CLASS_TYPE_MAP(fromType, toType, toTypeCount)					\
inline DataClassStaticTypeMapping dataClassFrom_##fromType##_To_##toType	\
(																			\
	#fromType,																\
	#toType,																\
	toTypeCount																\
)																			\