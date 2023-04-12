#include "Entity/Entity.h"
#include "Mesh.h"
#include "Material.h"
#include "RenderBoundingVolumeHierarchy.h"

namespace Eng
{
	class RenderDefinition : public EntityComponent
	{
	public:

		RenderDefinition(AssetEncoder::AssetID meshID, Material* material);
		~RenderDefinition() = default;

		static RenderDefinition* ECCreate(AssetEncoder::AssetID meshID, Material* material)
		{
			return s_entityComponentStorage.Alloc<RenderDefinition>(meshID, material);
		}

		static void ECDestroy(RenderDefinition* component)
		{
			s_entityComponentStorage.Free(component);
		}

		void OnDestruction() override { ECDestroy(this); }

		void GetReflection(CLib::Reflector& outReflector) override { outReflector.Init(this); }

		const AssetEncoder::AssetID GetMeshID() const { return m_meshID; }
		const Material* GetMaterial() const { return m_material; }
		const Bounds& GetRenderBounds() { return m_objectData.m_bounds; }

		void CommitStaticRenderEntity();
		void UpdateStaticRenderEntity();
		void UncommitStaticRenderEntity();

	private:

		RenderBoundingVolumeHierarchy::ObjectData m_objectData{};

		CLIB_REFLECTABLE(RenderDefinition,
			(AssetEncoder::AssetID) m_meshID,
			(Material*) m_material
		)
	};
}