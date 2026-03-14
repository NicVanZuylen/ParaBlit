#pragma once
#include "Engine.Control/IDataClass.h"
#include "Entity/Entity.h"
#include "RenderDefinition_generated.h"
#include "Engine.Reflectron/ReflectronAPI.h"
#include "WorldRender/Bounds.h"
#include "Material.h"
#include "WorldRender/DrawBatch.h"
#include "Resource/AssetStreamer.h"
#include "Engine.Math/Vector3.h"
#include "Engine.Math/Quaternion.h"
#include "Engine.AssetPipeline/MeshAsset.h"

namespace Eng
{
	class DynamicDrawPool;
	class DynamicEntityTracker;

	class RenderDefinition : public EntityComponent
	{
		REFLECTRON_CLASS()
	public:

		REFLECTRON_GENERATED_RenderDefinition()
		
		RenderDefinition() 
			: EntityComponent(this)
		{}

		RenderDefinition(TObjectPtr<Material> material, EEntityUpdateMethod updateMethod);

		~RenderDefinition() = default;

		const AssetEncoder::AssetID GetMeshID() const { return m_meshID; }
		const Material* GetMaterial() const { return m_material; }
		const StreamingBatch* GetStreamingBatch() const { return m_streamingBatch; }
		void GetMeshBounds(Bounds& bounds);

		void CommitRenderEntity(DynamicDrawPool* drawPool);
		void UpdateRenderEntity(const float& interpT);
		void UpdateStaticRenderEntity();
		void UncommitRenderEntity();
		void SetDynamicInstanceEnable(bool enable);

		inline void SetPriorFrameTransform(const Vector3f& position, const Quaternion& quaternion, const Vector3f& scale)
		{
			m_priorFramePosition = m_framePosition;
			m_framePosition = position;
			m_priorFrameScale = m_frameScale;
			m_frameScale = scale;
			m_priorFrameQuaternion = m_frameQuaternion;
			m_frameQuaternion = quaternion;
		}

		virtual void OnReferenceChanged(const ObjectPtr& ref) override final;

		virtual void OnFieldChanged(const ReflectronFieldData& field) override final;

	private:

		void GetMeshData();
		void OnStreamingComplete(DrawBatch::DrawBatchInstanceData*& dynamicInstanceData);

		Vector3f m_framePosition;
		Vector3f m_frameScale;
		Vector3f m_priorFramePosition;
		Vector3f m_priorFrameScale;
		Quaternion m_frameQuaternion;
		Quaternion m_priorFrameQuaternion;

		CLib::Vector<PB::ResourceView, 0, 8> m_batchBindingsDst;
		Bounds m_meshBounds;
		REFLECTRON_FIELD()
		TObjectPtr<Material> m_material;
		StreamingBatch* m_streamingBatch = nullptr;
		DynamicDrawPool* m_drawPool = nullptr;
		REFLECTRON_FIELD(enum)
		EEntityUpdateMethod m_updateMethod = EEntityUpdateMethod::STATIC;
		PB::u32 m_drawInstanceID = ~PB::u32(0);
		AssetEncoder::AssetID m_meshID = 0;
		bool m_committed = false;
		bool m_streamingComplete = false;

		REFLECTRON_FIELD()
		TObjectPtr<AssetPipeline::MeshAsset> m_mesh;
	};
	CLIB_REFLECTABLE_CLASS(RenderDefinition)
}