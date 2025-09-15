#include "StaticEntityTracker.h"
#include "Entity/EntityHierarchy.h"
#include "Entity/EntityBoundingVolumeHierarchy.h"
#include "Component/Transform.h"
#include "Component/RenderDefinition.h"

namespace Eng
{
	void StaticEntityTracker::CommitEntity()
	{
		RenderDefinition* renderDef = m_host->GetComponent<RenderDefinition>();
		RetrieveEntityBounds(renderDef, m_host->GetComponent<Transform>());

		EntityHierarchy* entityHierarchy = m_host->GetHierarchy();
		EntityBoundingVolumeHierarchy& ebvh = entityHierarchy->GetEntityBoundingVolumeHierarchy();
		m_entityObjectData.m_entity = m_host;
		ebvh.AddObject(&m_entityObjectData);
	}

	void StaticEntityTracker::UncommitEntity()
	{
		RenderDefinition* renderDef = m_host->GetComponent<RenderDefinition>();
		if (renderDef != nullptr)
		{
			renderDef->UncommitRenderEntity();
		}

		EntityHierarchy* entityHierarchy = m_host->GetHierarchy();
		EntityBoundingVolumeHierarchy& ebvh = entityHierarchy->GetEntityBoundingVolumeHierarchy();
		ebvh.RemoveObject(&m_entityObjectData);
	}

	void StaticEntityTracker::UpdateEntity()
	{
		RenderDefinition* renderDef = m_host->GetComponent<RenderDefinition>();
		if (renderDef != nullptr)
		{
			renderDef->UpdateStaticRenderEntity();
		}

		RetrieveEntityBounds(renderDef, m_host->GetComponent<Transform>());

		EntityHierarchy* entityHierarchy = m_host->GetHierarchy();
		EntityBoundingVolumeHierarchy& ebvh = entityHierarchy->GetEntityBoundingVolumeHierarchy();
		ebvh.UpdateObject(&m_entityObjectData);
	}

	void StaticEntityTracker::RetrieveEntityBounds(RenderDefinition* renderDef, const Transform* transform)
	{
		Bounds& entityBounds = m_entityObjectData.m_bounds;
		if (renderDef != nullptr)
		{
			renderDef->GetMeshBounds(entityBounds);
		}
		else
		{
			entityBounds = Bounds::Identity();
		}

		// Bounds are expected to be in object space. Transform to world space.
		entityBounds.Transform(transform->GetMatrix());
	}
}