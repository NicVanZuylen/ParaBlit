#pragma once
#include <CLib/Vector.h>
#include <CLib/Allocator.h>
#include <Clib/Reflection.h>

namespace Eng
{
	class Entity;
	class EntityHierarchy;

	class EntityComponent
	{
	public:

		virtual void OnConstruction() {}
		virtual void OnDestruction() = 0;

		Entity* GetHost() { return m_host; }
		virtual void GetReflection(CLib::Reflector& outReflector) {}

		const Entity* GetHost() const { return m_host; }
		size_t GetTypeHash() const { return m_typeHash; }

	protected:

		friend class Entity;

		EntityComponent() = default;
		~EntityComponent() = default;

		Entity* m_host = nullptr;
		size_t m_typeHash = ~size_t(0);	// Used to identify the derrived component's type.
	};

#define EC_CONSTRUCTOR_NAME ECCreate
#define EC_DESTRUCTOR_NAME ECDestroy

	// Dedicated thread safe storage for entity components.
	static CLib::Allocator s_entityComponentStorage{ 1024 * 1024, true };

	class Entity
	{
	public:

		Entity(EntityHierarchy* hierarchy)
			: m_hierarchy(hierarchy)
		{

		}

		~Entity();

		Entity* GetChild(uint32_t index) { return m_children[index]; }
		EntityComponent* GetComponentAt(uint32_t index) { return m_components[index]; }

		/*
		Description: Add a new component of the specified type and return its address.
		Return Type: T* (Component Type)
		Param:
			Args&&... args: Parameter pack of constructor arguments for the component.
		*/
		template<typename T, typename... Args>
		inline T* AddComponent(Args&&... args)
		{
			T* newComponent = T::EC_CONSTRUCTOR_NAME(args...);
			newComponent->m_host = this;
			newComponent->m_typeHash = typeid(T).hash_code();
			newComponent->OnConstruction();

			m_components.PushBack(newComponent);
			return newComponent;
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
					T::EC_DESTRUCTOR_NAME(reinterpret_cast<T*>(m_components[i]));

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
		inline T* GetComponent()
		{
			auto searchType = typeid(T).hash_code();
			for (auto& component : m_components)
			{
				auto componentType = component->m_typeHash;
				if (componentType == searchType)
					return reinterpret_cast<T*>(component);
			}

			return nullptr;
		}

		uint32_t ChildCount() const { return m_children.Count(); }
		uint32_t ComponentCount() const { return m_components.Count(); }
		const Entity* GetChild(uint32_t index) const { return m_children[index]; }
		const EntityComponent* GetComponentAt(uint32_t index) const { return m_components[index]; }

		/*
		Description: Get the component instance attached to this entity or return nullptr if it doesn't exist.
		Return Type: const T* (Component Type)
		*/
		template <typename T>
		inline const T* GetComponent() const { return GetComponent<T>(); }

		/*
		Description: Return "true" if the entity contains a component of the specified type.
		Return Type: bool
		*/
		template <typename T>
		inline bool HasComponent() const
		{
			auto searchType = typeid(T).hash_code();
			for (auto& component : m_components)
			{
				auto componentType = component->m_typeHash;
				if (componentType == searchType)
					return true;
			}

			return false;
		}

		inline EntityHierarchy* GetHierarchy() { return m_hierarchy; }

	private:

		EntityHierarchy* m_hierarchy;
		CLib::Vector<Entity*, 4, 4> m_children;
		CLib::Vector<EntityComponent*, 4, 4> m_components;
	};
};