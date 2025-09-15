#pragma once
#include "Entity_generated.h"
#include <CLib/Vector.h>
#include <CLib/String.h>
#include <CLib/Allocator.h>
#include <Engine.Control/IDataClass.h>
#include <Engine.Reflectron/ReflectronAPI.h>

namespace Eng
{
	using namespace Ctrl;

	class Entity;
	class EntityHierarchy;

	enum class EEntityUpdateMethod
	{
		STATIC,
		DYNAMIC
	};

	class EntityComponent : public Ctrl::DataClass
	{
	public:

		/*
		Description: Called upon completion of construction and linking of entities and associated data.
		*/
		virtual void OnInitialize() {};

		/*
		Description: Called upon destruction of the host entity, or removal of the component.
		*/
		virtual void OnHostDestroy() {};

		Entity* GetHost() { return m_host; }

		const Entity* GetHost() const { return m_host; }
		size_t GetTypeHash() const { return m_typeHash; }

	protected:

		friend class Entity;

		template<typename T>
		EntityComponent(T* componentInstance) : Ctrl::DataClass(componentInstance) 
		{
			m_typeHash = typeid(T).hash_code();
		};
		~EntityComponent() = default;

		Entity* m_host = nullptr;
		size_t m_typeHash;	// Used to identify the derrived component's type.
	};

	class Entity : public Ctrl::DataClass
	{
		REFLECTRON_CLASS()
	public:

		REFLECTRON_GENERATED_Entity()

		Entity()
			: Ctrl::DataClass(this)
			, m_hierarchy(nullptr)
			, m_name("UNNAMED_ENTITY")
		{
		}

		Entity(EntityHierarchy* hierarchy, const char* name = nullptr) 
			: Ctrl::DataClass(this)
			, m_hierarchy(hierarchy)
		{
			if (name != nullptr)
				m_name = name;
			else
				m_name = "UNNAMED_ENTITY";
		}

		~Entity();

		void BindToHierarchy(EntityHierarchy* hierarchy);

		Entity* GetChild(uint32_t index) { return m_children[index]; }
		EntityComponent* GetComponentAt(uint32_t index) { return m_components[index]; }

		/*
		Description: Add a new component of the specified type and return its address.
		Return Type: T* (Component Type)
		Param:
			Args&&... args: Parameter pack of constructor arguments for the component.
		*/
		template<typename T, typename... Args>
		TObjectPtr<T> AddComponent(Args&&... args)
		{
			TObjectPtr<T> newComponent = new T(args...);
			newComponent->m_host = this;

			m_components.PushBack(newComponent);
			return m_components.Back();
		}

		/*
		Description: Remove a component of the specified type. Return "true" if the component was found and removed.
		Return Type: bool
		*/
		template<typename T>
		inline bool RemoveComponent()
		{
			auto searchType = typeid(T).hash_code();
			for (uint32_t i = 0; i < m_components.Count(); ++i)
			{
				auto componentType = m_components[i]->GetTypeHash();
				if (componentType == searchType)
				{
					m_components[i].Invalidate();

					// Shift components to close gap and preserve order.
					uint32_t shiftCount = m_components.Count() - i;
					memcpy(&m_components[i], &m_components[i + 1], shiftCount * sizeof(EntityComponent*));
					m_components.PopBack();

					return true;
				}
			}

			return false;
		}

		/*
		Description: Remove a component at the specified index. Returns true if the index is valid and the component is removed.
		Return Type: bool
		Param:
			uint32_t index: The index of the component to remove.
		*/
		bool RemoveComponent(uint32_t index);

		/*
		Description: Get the component instance attached to this entity or return nullptr if it doesn't exist.
		Return Type: T* (Component Type)
		*/
		template <typename T>
		inline TObjectPtr<T> GetComponent()
		{
			auto searchType = typeid(T).hash_code();
			for (TObjectPtr<EntityComponent> component : m_components)
			{
				auto componentType = component->m_typeHash;
				if (componentType == searchType)
					return component;
			}

			return {};
		}

		uint32_t ChildCount() const { return m_children.Count(); }
		uint32_t ComponentCount() const { return m_components.Count(); }
		const Entity* GetChild(uint32_t index) const { return m_children[index]; }
		const TObjectPtr<EntityComponent> GetComponentAt(uint32_t index) const { return m_components[index]; }

		/*
		Description: Get the component instance attached to this entity or return nullptr if it doesn't exist.
		Return Type: const T* (Component Type)
		*/
		template <typename T>
		inline const TObjectPtr<T> GetComponent() const { return GetComponent<T>(); }

		/*
		Description: Return "true" if the entity contains a component of the specified type.
		Return Type: bool
		*/
		template <typename T>
		inline bool HasComponent() const
		{
			auto searchType = typeid(T).hash_code();
			for (TObjectPtr<EntityComponent> component : m_components)
			{
				auto componentType = component->m_typeHash;
				if (componentType == searchType)
					return true;
			}

			return false;
		}

		const TObjectPtrArray<EntityComponent>& GetAllComponents() const { return m_components; }

		inline EntityHierarchy* GetHierarchy() { return m_hierarchy; }

		inline void Rename(const char* name) { m_name = name; }
		inline const char* GetName() const { return m_name.c_str(); }

	private:

		REFLECTRON_FIELD()
		std::string m_name;
		REFLECTRON_FIELD()
		TObjectPtrArray<EntityComponent> m_components;

		EntityHierarchy* m_hierarchy;
		CLib::Vector<Entity*, 4, 4> m_children;
	};
	CLIB_REFLECTABLE_CLASS(Entity)
};