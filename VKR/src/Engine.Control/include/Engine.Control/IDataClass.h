#pragma once
#include "ControlLib.h"
#include "Engine.Reflectron/ReflectronAPI.h"
#include "IDataFile.h"
#include "CLib/Reflection.h"
#include "Engine.Reflectron/ReflectronReflector.h"
#include "GUID.h"

#include <cassert>
#include <cstddef>
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
	class DataClass;

	struct DataClassTracking
	{
		DataClass* m_ptr = nullptr;
		std::atomic_int m_refCount = 0;
		
		~DataClassTracking();
		
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
	static_assert(offsetof(DataClassTracking, m_ptr) == 0 && "DataClassTracking: m_ptr's offset should always be zero to allow access from a double pointer via dereferencing.");

	/*
	Pointer wrapper to a DataClass instance which can be referenced by GUID.
	*/
	class ObjectPtr
	{
	public:

		ObjectPtr() = default;
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

		DataClass* operator -> () { return *m_ptr; }
		const DataClass* operator -> () const { return *m_ptr; }

		operator DataClass*() { return *m_ptr; }
		operator const DataClass* () const { return *m_ptr; }

		void Invalidate()
		{
			GetTrackingPtr()->RemRef();
			m_ptr = &NullObjectPtr;
		}

		inline bool IsValid() const
		{
			return m_ptr != &NullObjectPtr && m_ptr != nullptr;
		}

		inline bool IsInstantiated()
		{
			return m_ptr != nullptr && *m_ptr != nullptr;
		}

		CONTROL_API void AssignDataClass(const DataClass* dataClass);

		CONTROL_API void Assign(const GUID& guid);
		ObjectPtr& operator = (const GUID& guid)
		{
			if (IsValid())
			{
				Invalidate();
			}

			Assign(guid);
			return *this;
		}

		const GUID& GetAssignedGUID() const;

		ObjectPtr& operator = (const DataClass* dataClass)
		{
			if (IsValid())
			{
				Invalidate();
			}

			AssignDataClass(dataClass);
			return *this;
		}

		ObjectPtr& operator = (const ObjectPtr& other)
		{
			if (IsValid())
			{
				Invalidate();
			}

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

		static inline DataClass* NullObjectPtr = nullptr;

		DataClassTracking* GetTrackingPtr() const { return reinterpret_cast<DataClassTracking*>(m_ptr); }

		DataClass** m_ptr = &NullObjectPtr; // This is a pointer another pointer residing within DataClass::s_globalDataClassMap.
	};

	/*
	Typed version of ObjectPtr. Can use the derived type of DataClass.
	*/
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
			if (IsValid())
			{
				Invalidate();
			}

			m_ptr = ptr.GetBasePtr();
			GetTrackingPtr()->AddRef();
			return *this;
		}

		TObjectPtr<T>& operator = (DataClass* dataClass)
		{
			if (IsValid())
			{
				Invalidate();
			}

			AssignDataClass(dataClass);
			return *this;
		}
	};
	static_assert(sizeof(TObjectPtr<void*>) == sizeof(ObjectPtr));

	/*
	Vector of ObjectPtr.
	*/
	class ObjectPtrArray : public CLib::Vector<ObjectPtr, 0, 1, true>
	{
	public:
		using VectorT = CLib::Vector<ObjectPtr, 0, 1, true>;

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

	/*
	Vector of typed ObjectPtr.
	*/
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

	/*
	[DataClass]

	DataClass is a serializable base class with built in reflection and serialization capabilities.

	Each DataClass instance is assigned a GUID to identify it when serialized. This GUID is used to translate references to a pointer to the matching instance at runtime.

	Derived classes of DataClass can call the templated constructor 'DataClass<T>' passing in the type of the derived instance.
	This allows reflection and subsequently serialization of the derived class's reflectable fields (those marked with REFLECTRON_FIELD()).

	An implementation of a derived class of DataClass should be marked with CLIB_REFLECTABLE_CLASS(Derived) to allow it to be instantiated via the DataNode system.
	It will also need to be reflected via Reflectron, so the class should implement the appropriate macros: REFLECTRON_CLASS() and REFLECTRON_GENERATED_Derived

	Example class:

	#include "Example_generated.h"

	...

	class Example : public Ctrl::DataClass
	{
		REFLECTRON_CLASS()
	public:

		REFLECTRON_GENERATED_Example()

		Example() : Ctrl::DataClass(this)
		{
			...
		}

		~Example();

	private:
	
		// Member variables declared under REFLECTRON_FIELD() are reflectable and serializable.

		REFLECTRON_FIELD()
		int foo;
		REFLECTRON_FIELD()
		float bar;
		REFLECTRON_FIELD()
		TObjectPtr<Derived> dcReference; // A serializable typed reference to a dataclass implementation. Usable as a pointer at runtime.
	};
	CLIB_REFLECTABLE_CLASS(Example)

	*/
	class alignas(128) DataClass
	{
	public:

		/*
		Use reflection to instantiate all valid DataClass-formatted DataNodes as DataClass instances under the provided root node.
		*/
		CONTROL_API static void InstantiateNodeTree(const Ctrl::IDataNode* root);

		/*
		Save changes to all DataClass instances to their corresponding data nodes under the provided root node.
		*/
		CONTROL_API static void SaveNodeTree(Ctrl::IDataNode* root);

		/*
		Get the DataNode field type and array size from the provided Reflectron field data.
		*/
		CONTROL_API static void GetTypeInfoFromField(const ReflectronFieldData& field, EFieldType& outType, size_t& outArrayCount);

		/*
		Get the DataClass derived type name 'T' from a TObjectPtr<T> field. Namespaces are ommitted.
		*/
		CONTROL_API static std::string GetDataClassTypeFromField(const ReflectronFieldData& field);

		template<typename T>
		DataClass(T* instance)
		{
			m_reflector.Init<T>(instance);
		}
		CONTROL_API ~DataClass();

		Reflectron::Reflector& GetReflection() { return m_reflector; }
		const Reflectron::Reflector& GetReflection() const { return m_reflector; }

		DataClass* DerivedAddress() { return reinterpret_cast<DataClass*>(m_reflector.GetAddress()); }

		/*
		Deserialize data to this DataClass instance from the provided DataNode.
		*/
		CONTROL_API void FillFromDataNode(const IDataNode* node);

		/*
		Returns true of the GUID identifier and field values within the provided data node match those of this DataClass.
		NOTE: If a field is missing in either the node or this DataClass, it is not considered a mismatch.
		*/
		CONTROL_API bool FieldsMatchDataNode(const IDataNode* node);

		/*
		Serialize this DataClass instance to the provided DataNode.
		*/
		CONTROL_API void SaveToDataNode(IDataNode* dstNode) const;

		/*
		This dummy virtual function is here to ensure the VTable (_vfptr) pointer is located within this class.
		Why? If there's no virtual function here and we create a virtual function in a derived class,
		the offset of this class will be incorrect when instantiating a derived class via CLib class reflection.
		Having this here ensures the offset of Ctrl::DataClass in any inheritance tree will be zero.
		*/
		// virtual void VTablePtrDummy() {} // Uncomment this if there are no other virtual functions defined here.

		/*
		Can be overriden to provide a custom name for the DataClass instance.
		*/
		virtual const char* GetDataClassInstanceName() { return "Unknown DataClass"; }

		/*
		Can be called to hint that a field has been changed. The implementation can react to the change by overriding this function.
		*/
		virtual void OnFieldChanged(const ReflectronFieldData& field) {};

		/*
		Can be called to hint that an object reference (TObjectPtr<T>) has been modified externally. The newly assigned reference is provided to compare with fields.
		*/
		virtual void OnReferenceChanged(const ObjectPtr& ref) {}

		const GUID& GetGUID() const { return m_guid; }
		
	private:
		
		friend class ObjectPtr;
		friend struct DataClassStaticTypeMapping;

		static inline std::unordered_map<std::string, std::pair<EFieldType, size_t>> s_typeMap =
		{
			{ "int", { EFieldType::INT, 1 } },
			{ "int32_t", { EFieldType::INT, 1 } },
			{ "unsigned int", { EFieldType::INT, 1 } },
			{ "uint32_t", { EFieldType::INT, 1 } },
			{ "float", { EFieldType::FLOAT, 1 } },
			{ "double", { EFieldType::DOUBLE, 1 } },
			{ "std::string", { EFieldType::STRING, 1 } },
			{ "string", { EFieldType::STRING, 1 } },
			{ "bool", { EFieldType::BOOL, 1 } },
		};
		static std::unordered_map<Ctrl::GUID, DataClassTracking> s_globalDataClassMap;
		static std::unordered_map<DataClass**, Ctrl::GUID> s_globalGUIDMap;
		static std::mutex s_globalDataClassMapMutex;

		void AssignNewGUID(const GUID& guid);

		using FieldTypeData = std::pair<Ctrl::EFieldType, uint32_t>; // Type, Count

	protected:

		Reflectron::Reflector m_reflector;
		GUID m_guid = nullGUID;
	};

	struct DataClassStaticTypeMapping
	{
		DataClassStaticTypeMapping(const char* fromType, const char* toType, size_t toTypeCount)
		{
			auto& toField = DataClass::s_typeMap[toType];
			DataClass::s_typeMap.insert({ fromType, { toField.first, toField.second * toTypeCount } });
		}
	};
}

#define DATA_CLASS_TYPE_MAP(fromType, toType, toTypeCount)					\
inline DataClassStaticTypeMapping dataClassFrom_##fromType##_To_##toType	\
(																			\
	#fromType,																\
	#toType,																\
	toTypeCount																\
)																			\

// eof