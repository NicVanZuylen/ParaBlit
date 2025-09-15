#include "DynamicEntityTracker.h"
#include "Entity/EntityHierarchy.h"
#include "Component/Transform.h"
#include "Component/RenderDefinition.h"

namespace Eng
{
	void DynamicEntityTracker::CommitEntity(DynamicDrawPool* drawPool)
	{
		RenderDefinition* renderDef = m_host->GetComponent<RenderDefinition>();
		if (renderDef != nullptr)
		{
			renderDef->CommitRenderEntity(drawPool);
		}
	}

	void DynamicEntityTracker::UncommitEntity()
	{
		RenderDefinition* renderDef = m_host->GetComponent<RenderDefinition>();
		if (renderDef != nullptr)
		{
			renderDef->UncommitRenderEntity();
		}
	}

	void DynamicEntityTracker::UpdateEntity()
	{
		RenderDefinition* renderDef = m_host->GetComponent<RenderDefinition>();
		if (renderDef != nullptr)
		{
			renderDef->UpdateRenderEntity();
		}
	}
}