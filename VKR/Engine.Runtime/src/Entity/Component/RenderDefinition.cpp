#include "Entity/Component/RenderDefinition.h"
#include "Entity/Component/Transform.h"
#include "Entity/EntityHierarchy.h"

namespace Eng
{
	RenderDefinition::RenderDefinition(AssetEncoder::AssetID meshID, Material* material)
	{
		m_meshID = meshID;
		m_material = material;

		m_objectData.m_meshID = m_meshID;
		Mesh::GetMeshData(m_meshID, &m_objectData.m_meshData);
	}

	void RenderDefinition::CommitStaticRenderEntity()
	{
		Transform* transform = m_host->GetComponent<Transform>();

		m_objectData.m_material = m_material;
		m_objectData.m_transform = transform->GetMatrix();
		m_objectData.m_bounds = Bounds(m_objectData.m_meshData.m_boundOrigin, m_objectData.m_meshData.m_boundExtents);
		m_objectData.m_bounds.Transform(m_objectData.m_transform);

		EntityHierarchy* hierarchy = m_host->GetHierarchy();
		hierarchy->GetRenderHierarchy().AddObject(&m_objectData);
	}

	void RenderDefinition::UpdateStaticRenderEntity()
	{
		Transform* transform = m_host->GetComponent<Transform>();

		m_objectData.m_transform = transform->GetMatrix();
		m_objectData.m_bounds = Bounds(m_objectData.m_meshData.m_boundOrigin, m_objectData.m_meshData.m_boundExtents);
		m_objectData.m_bounds.Transform(m_objectData.m_transform);

		EntityHierarchy* hierarchy = m_host->GetHierarchy();
		hierarchy->GetRenderHierarchy().UpdateObject(&m_objectData);
	}

	void RenderDefinition::UncommitStaticRenderEntity()
	{
		EntityHierarchy* hierarchy = m_host->GetHierarchy();
		hierarchy->GetRenderHierarchy().RemoveObject(&m_objectData);
	}
}