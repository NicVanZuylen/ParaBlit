#include "Entity/Entity.h"
#include "RenderDefinition_generated.h"
#include "Engine.Reflectron/ReflectronAPI.h"
#include "Mesh.h"
#include "Material.h"
#include "Resource/AssetStreamer.h"

namespace Eng
{
	class DynamicDrawPool;

	class RenderDefinition : public EntityComponent
	{
		REFLECTRON_CLASS()
	public:

		REFLECTRON_GENERATED_RenderDefinition()
		
		RenderDefinition() 
			: EntityComponent(this)
		{}

		RenderDefinition(const char* meshName, TObjectPtr<Material> material, EEntityUpdateMethod updateMethod);

		~RenderDefinition() = default;

		const AssetEncoder::AssetID GetMeshID() const { return m_meshID; }
		const Material* GetMaterial() const { return m_material; }
		const StreamingBatch* GetStreamingBatch() const { return m_streamingBatch; }
		void GetMeshBounds(Bounds& bounds);

		void CommitRenderEntity(DynamicDrawPool* drawPool);
		void UpdateRenderEntity();
		void UpdateStaticRenderEntity();
		void UncommitRenderEntity();
		void SetDynamicInstanceEnable(bool enable);

	private:

		void GetMeshData();
		void OnStreamingComplete(DrawBatch::DrawBatchInstanceData*& dynamicInstanceData);

		REFLECTRON_FIELD()
		std::string m_meshName;
		REFLECTRON_FIELD()
		TObjectPtr<Material> m_material;
		REFLECTRON_FIELD(enum)
		EEntityUpdateMethod m_updateMethod = EEntityUpdateMethod::STATIC;

		DynamicDrawPool* m_drawPool = nullptr;
		StreamingBatch* m_streamingBatch = nullptr;
		AssetPipeline::MeshCacheData m_meshData;
		CLib::Vector<PB::ResourceView, 0, 8> m_batchBindingsDst;
		AssetEncoder::AssetID m_meshID = 0;
		PB::u32 m_drawInstanceID = ~PB::u32(0);
		bool m_committed = false;
		bool m_streamingComplete = false;
	};
	CLIB_REFLECTABLE_CLASS(RenderDefinition)
}