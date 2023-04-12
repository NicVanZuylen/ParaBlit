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
		if (renderDef != nullptr)
		{
			renderDef->CommitStaticRenderEntity();
		}

		Bounds& entityBounds = m_entityObjectData.m_bounds;
		if (renderDef != nullptr)
		{
			entityBounds = renderDef->GetRenderBounds();
		}
		else
		{
			Transform* transform = m_host->GetComponent<Transform>();

			entityBounds = Bounds::Identity();
			entityBounds.Transform(transform->GetMatrix());
		}

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
			renderDef->UncommitStaticRenderEntity();
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

		Bounds& entityBounds = m_entityObjectData.m_bounds;
		if (renderDef != nullptr)
		{
			entityBounds = renderDef->GetRenderBounds();
		}
		else
		{
			Transform* transform = m_host->GetComponent<Transform>();

			entityBounds = Bounds::Identity();
			entityBounds.Transform(transform->GetMatrix());
		}

		EntityHierarchy* entityHierarchy = m_host->GetHierarchy();
		EntityBoundingVolumeHierarchy& ebvh = entityHierarchy->GetEntityBoundingVolumeHierarchy();
		ebvh.UpdateObject(&m_entityObjectData);
	}
}