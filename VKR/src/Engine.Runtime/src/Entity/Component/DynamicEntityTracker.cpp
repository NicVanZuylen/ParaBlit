#include "DynamicEntityTracker.h"
#include "Entity/EntityHierarchy.h"
#include "Component/Transform.h"
#include "Component/RenderDefinition.h"

namespace Eng
{
	void DynamicEntityTracker::CommitEntity(DynamicDrawPool* drawPool)
	{
		m_transform = m_host->GetComponent<Transform>();

		RenderDefinition* renderDef = m_host->GetComponent<RenderDefinition>();
		if (renderDef != nullptr)
		{
			renderDef->CommitRenderEntity(drawPool);
			renderDef->GetMeshBounds(m_meshBounds);

			m_renderDefinition = renderDef;
		}
	}

	void DynamicEntityTracker::UncommitEntity()
	{
		if (m_renderDefinition != nullptr)
		{
			m_renderDefinition->UncommitRenderEntity();

			m_transform.Invalidate();
			m_renderDefinition.Invalidate();
		}
	}
}