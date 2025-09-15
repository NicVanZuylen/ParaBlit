#include "Entity/Component/RenderDefinition.h"
#include "Entity/Component/Transform.h"
#include "Entity/EntityHierarchy.h"
#include "WorldRender/DynamicDrawPool.h"

namespace Eng
{
	// ****************************************************************************************************************************
	// ****************************************************************************************************************************
	RenderDefinition::RenderDefinition(const char* meshName, TObjectPtr<Material> material, EEntityUpdateMethod updateMethod) : EntityComponent(this)
	{
		m_meshName = meshName;
		m_material = material;
		m_updateMethod = updateMethod;
	}
	// ****************************************************************************************************************************
	// ****************************************************************************************************************************
	void RenderDefinition::GetMeshData()
	{
		m_meshID = AssetEncoder::AssetHandle(m_meshName.c_str()).GetID(&Mesh::s_meshDatabaseLoader);
		Mesh::GetMeshData(m_meshID, &m_meshData);
	}
	// ****************************************************************************************************************************
	// ****************************************************************************************************************************
	void RenderDefinition::GetMeshBounds(Bounds& bounds)
	{
		if (m_meshID == 0)
		{
			GetMeshData();
		}

		bounds.m_origin = m_meshData.m_boundOrigin;
		bounds.m_extents = m_meshData.m_boundExtents;
	}
	// ****************************************************************************************************************************
	// ****************************************************************************************************************************
	void RenderDefinition::CommitRenderEntity(DynamicDrawPool* drawPool)
	{
		if (m_committed == true)
		{
			UncommitRenderEntity();
		}

		m_drawPool = drawPool;
		{
			m_drawInstanceID = m_drawPool->AddInstance();

			m_drawPool->SetInstanceEnable(m_drawInstanceID, false); // Disable instance until streaming is complete.
			m_streamingComplete = false;

			m_streamingBatch = m_host->GetHierarchy()->GetAssetStreamer()->AllocStreamingBatch();

			if (m_meshID == 0)
			{
				GetMeshData();
			}

			DynamicDrawPool::InstanceCullData* cullData = m_drawPool->GetInstanceCullingData(m_drawInstanceID);

			Bounds meshBounds(m_meshData.m_boundOrigin, m_meshData.m_boundExtents);
			cullData->m_origin = meshBounds.Centre();
			cullData->m_radius = Math::Max(Math::Max(meshBounds.m_extents.x, meshBounds.m_extents.y), meshBounds.m_extents.z);

			m_streamingBatch->AddResource(StreamableHandle(m_meshID, EStreamableResourceType::MESH, StreamableHandle::EBindingType::STORAGE)); // Mesh library instance index.

			m_material->ResolveTextureIDs();
			AssetEncoder::AssetID* textureIDs = m_material->GetTextureIDs();
			for (uint32_t i = 0; i < m_material->GetTextureCount(); ++i)
			{
				m_streamingBatch->AddResource(StreamableHandle(textureIDs[i], EStreamableResourceType::TEXTURE, StreamableHandle::EBindingType::SRV));
			}

			m_batchBindingsDst.SetCount(m_streamingBatch->CalculateOutputBindingCount());
			m_streamingBatch->SetOutputBindingLocation(m_batchBindingsDst.Data());
			m_streamingBatch->BeginStreaming();

			m_committed = true;
		}
	}
	// ****************************************************************************************************************************
	// ****************************************************************************************************************************
	void RenderDefinition::OnStreamingComplete(DrawBatch::DrawBatchInstanceData*& dynamicInstanceData)
	{
		std::memcpy(dynamicInstanceData->m_bindings, m_batchBindingsDst.Data(), m_batchBindingsDst.Count() * sizeof(PB::ResourceView));
		m_batchBindingsDst.Trim();

		PB::SamplerDesc batchTextureSamplerDesc;
		batchTextureSamplerDesc.m_anisotropyLevels = 1.0f;
		batchTextureSamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
		batchTextureSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::REPEAT;
		dynamicInstanceData->m_sampler = m_host->GetHierarchy()->GetRenderer()->GetSampler(batchTextureSamplerDesc);

		m_streamingComplete = true;
		m_drawPool->SetInstanceEnable(m_drawInstanceID, true);

		m_streamingBatch->EndStreamingAndDelete();
		m_streamingBatch = nullptr;
	}
	// ****************************************************************************************************************************
	// ****************************************************************************************************************************
	void RenderDefinition::UpdateRenderEntity()
	{
		assert(m_updateMethod == EEntityUpdateMethod::DYNAMIC);
		if (m_committed == true)
		{
			DrawBatch::DrawBatchInstanceData* dynamicInstanceData = m_drawPool->GetInstanceData(m_drawInstanceID);

			// Update last frame's transform matrix.
			std::memcpy(dynamicInstanceData->m_prevFrameModelMatrix, dynamicInstanceData->m_modelMatrix, sizeof(DrawBatch::DrawBatchInstanceData::m_prevFrameModelMatrix));

			Matrix4& transformMatrix = *reinterpret_cast<Matrix4*>(dynamicInstanceData->m_modelMatrix);
			m_host->GetComponent<Transform>()->GetMatrix(transformMatrix);

			if (m_streamingComplete == false && m_streamingBatch->GetStatus() == StreamingBatch::EStreamingStatus::IDLE)
			{
				OnStreamingComplete(dynamicInstanceData);
			}
		}
	}

	// ****************************************************************************************************************************
	// ****************************************************************************************************************************
	void RenderDefinition::UpdateStaticRenderEntity()
	{
		assert(m_updateMethod == EEntityUpdateMethod::STATIC);
		if (m_committed == true)
		{
			DrawBatch::DrawBatchInstanceData* dynamicInstanceData = m_drawPool->GetInstanceData(m_drawInstanceID);

			Matrix4& transformMatrix = *reinterpret_cast<Matrix4*>(dynamicInstanceData->m_modelMatrix);
			m_host->GetComponent<Transform>()->GetMatrix(transformMatrix);

			// For static entities the previous frame's model matrix should be the same as the current frame's.
			// This way when the entity is moved on one frame and updated but not updated on the next, the motion vectors are not stuck in motion when there should be no motion.
			std::memcpy(dynamicInstanceData->m_prevFrameModelMatrix, transformMatrix, sizeof(DrawBatch::DrawBatchInstanceData::m_prevFrameModelMatrix));

			if (m_streamingComplete == false && m_streamingBatch->GetStatus() == StreamingBatch::EStreamingStatus::IDLE)
			{
				OnStreamingComplete(dynamicInstanceData);
			}
		}
	}

	// ****************************************************************************************************************************
	// ****************************************************************************************************************************
	void RenderDefinition::UncommitRenderEntity()
	{
		if (m_committed == true)
		{
			if (m_drawInstanceID != DynamicDrawPool::InvalidInstanceID)
			{
				m_drawPool->RemoveInstance(m_drawInstanceID);
				m_drawInstanceID = DynamicDrawPool::InvalidInstanceID;
			}
			m_streamingComplete = false;
			m_committed = false;
		}

		m_drawPool = nullptr;
	}
	// ****************************************************************************************************************************
	// ****************************************************************************************************************************
	void RenderDefinition::SetDynamicInstanceEnable(bool enable)
	{
		if (m_updateMethod == EEntityUpdateMethod::DYNAMIC)
		{
			m_host->GetHierarchy()->GetDynamicDrawPool().SetInstanceEnable(m_drawInstanceID, enable);
		}
	}
}