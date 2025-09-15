#include "Entity.h"

namespace Eng
{
	Entity::~Entity()
	{
		for (TObjectPtr<EntityComponent> component : m_components)
		{
			component->OnHostDestroy();
		}
	}

	void Entity::BindToHierarchy(EntityHierarchy* hierarchy)
	{
		m_hierarchy = hierarchy;
		for (TObjectPtr<EntityComponent> component : m_components)
		{
			component->m_host = this;
		}
	}

	bool Entity::RemoveComponent(uint32_t index)
	{
		if (index < m_components.Count())
		{
			m_components[index]->OnHostDestroy();

			// Shift components to close gap and preserve order.
			uint32_t shiftCount = m_components.Count() - index;
			memcpy(m_components.Data() + index, m_components.Data() + (index + 1), shiftCount * sizeof(EntityComponent*));
			m_components.PopBack();

			return true;
		}
		return false;
	}
};