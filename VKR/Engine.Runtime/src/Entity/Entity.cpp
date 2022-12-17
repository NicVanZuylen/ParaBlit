#include "Entity.h"

namespace Eng
{
	Entity::~Entity()
	{
		for (auto& component : m_components)
		{
			component->OnDestruction();
		}
	}

	bool Entity::RemoveComponent(uint32_t index)
	{
		if (index < m_components.Count())
		{
			m_components[index]->OnDestruction();

			// Shift components to close gap and preserve order.
			uint32_t shiftCount = m_components.Count() - index;
			memcpy(&m_components[index], &m_components[index + 1], shiftCount * sizeof(EntityComponent*));
			m_components.PopBack();

			return true;
		}
		return false;
	}
};