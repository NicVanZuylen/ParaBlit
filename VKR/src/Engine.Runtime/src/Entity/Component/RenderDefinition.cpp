#include "Entity/Component/RenderDefinition.h"
#include "Engine.Control/IDataClass.h"
#include "Engine.Math/Quaternion.h"
#include "Engine.Math/Vector3.h"
#include "Engine.Reflectron/ReflectronAPI.h"
#include "Entity/Component/DynamicEntityTracker.h"
#include "Entity/Component/Transform.h"
#include "Entity/EntityHierarchy.h"
#include "WorldRender/DynamicDrawPool.h"

namespace Eng
{
	// ****************************************************************************************************************************
	// ****************************************************************************************************************************
	RenderDefinition::RenderDefinition(TObjectPtr<Material> material, EEntityUpdateMethod updateMethod) : EntityComponent(this)
	{
		m_material = material;
		m_updateMethod = updateMethod;
	}
	// ****************************************************************************************************************************
	// ****************************************************************************************************************************
	void RenderDefinition::GetMeshData()
	{
		if (m_meshID == 0)
		{
			const char* assetGuid = (m_mesh != nullptr) ? m_mesh->GetAssetGUID().c_str() : nullptr;
			const char* fallback = "Meshes/Primitives/sphere";
			const char* assetIdentifier = assetGuid ? assetGuid : fallback; // Can be GUID or name.

			m_meshID = AssetEncoder::AssetHandle(assetIdentifier).GetID(&Mesh::s_meshDatabaseLoader);

			if (m_mesh != nullptr)
			{
				m_meshBounds.m_origin = m_mesh->GetBoundOrigin();
				m_meshBounds.m_extents = m_mesh->GetBoundExtents();
			}
			else
			{
				AssetPipeline::MeshCacheData data;
				Mesh::GetMeshData(m_meshID, &data);

				m_meshBounds.m_origin = data.m_boundOrigin;
				m_meshBounds.m_extents = data.m_boundExtents;
			}
		}
	}
	// ****************************************************************************************************************************
	// ****************************************************************************************************************************
	void RenderDefinition::GetMeshBounds(Bounds& bounds)
	{
		GetMeshData();

		bounds.m_origin = m_meshBounds.m_origin;
		bounds.m_extents = m_meshBounds.m_extents;
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

			if (m_material.IsInstantiated() == false)
			{
				m_material = m_host->GetHierarchy()->GetDefaultMaterial();
			}

			DynamicDrawPool::InstanceCullData* cullData = m_drawPool->GetInstanceCullingData(m_drawInstanceID);
			cullData->m_origin = m_meshBounds.Center();
			cullData->m_radius = Math::Max(Math::Max(m_meshBounds.m_extents.x, m_meshBounds.m_extents.y), m_meshBounds.m_extents.z);

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
	void RenderDefinition::UpdateRenderEntity(const float& interpT)
	{
		assert(m_updateMethod == EEntityUpdateMethod::DYNAMIC);
		if (m_committed == true)
		{
			DrawBatch::DrawBatchInstanceData* dynamicInstanceData = m_drawPool->GetInstanceData(m_drawInstanceID);

			// Update last frame's transform matrix.
			std::memcpy(dynamicInstanceData->m_prevFrameModelMatrix, dynamicInstanceData->m_modelMatrix, sizeof(DrawBatch::DrawBatchInstanceData::m_prevFrameModelMatrix));

			// Calculate interpolation transform matrix.
			Matrix4& transformMatrix = *reinterpret_cast<Matrix4*>(dynamicInstanceData->m_modelMatrix);
			{
				Vector3f deltaPos = m_framePosition - m_priorFramePosition;
				Quaternion deltaRotation = m_frameQuaternion * Math::Inverse(m_priorFrameQuaternion);
				Vector3f deltaScale = m_frameScale - m_priorFrameScale;

				Vector3f interpPos = Math::Lerp(m_framePosition, m_framePosition + deltaPos, interpT);
				Quaternion interpRotation = Math::Slerp(m_frameQuaternion, deltaRotation * m_frameQuaternion, interpT);
				Vector3f interpScale = Math::Lerp(m_frameScale, m_frameScale + deltaScale, interpT);

				transformMatrix.ToIdentity();
				transformMatrix.Translate(interpPos);
				transformMatrix *= interpRotation.ToMatrix4();
				transformMatrix.Scale(interpScale);
			}

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
	// ****************************************************************************************************************************
	// ****************************************************************************************************************************
	void RenderDefinition::OnReferenceChanged(const ObjectPtr& ref)
	{
		if (ref == m_material)
		{
			printf("Entity: %s [%p] will be hot-reloaded as its material properties have been modified.\n", m_host->GetName(), m_host);
			m_host->SoftReload();
		}
	}
	// ****************************************************************************************************************************
	// ****************************************************************************************************************************
	void RenderDefinition::OnFieldChanged(const ReflectronFieldData& field)
	{
		if (strcmp(field.m_name, "m_mesh") == 0)
		{
			m_meshID = 0;
			m_host->SoftReload();
		}
		else if (strcmp(field.m_name, "m_material") == 0)
		{
			m_host->SoftReload();
		}
	}
}