#include "Entity/Component/RenderDefinition.h"
#include "Entity/Component/Transform.h"
#include "Entity/EntityHierarchy.h"

namespace Eng
{
	RenderDefinition::RenderDefinition(Mesh* mesh, Material* material)
	{
		m_mesh = mesh;
		m_material = material;
	}

	void RenderDefinition::OnConstruction()
	{
		EntityHierarchy* hierarchy = m_host->GetHierarchy();
		Transform* transform = m_host->GetComponent<Transform>();

		m_objectData.m_mesh = m_mesh;
		m_objectData.m_material = m_material;
		m_objectData.m_transform = transform->GetMatrix();
		m_objectData.m_bounds = m_mesh->GetBounds();
		m_objectData.m_bounds.Transform(m_objectData.m_transform);

		hierarchy->GetRenderHierarchy().AddObject(&m_objectData);
	}
}