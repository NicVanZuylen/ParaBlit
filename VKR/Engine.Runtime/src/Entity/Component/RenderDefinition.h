#include "Entity/Entity.h"
#include "Mesh.h"
#include "Material.h"
#include "RenderBoundingVolumeHierarchy.h"

namespace Eng
{
	class RenderDefinition : public EntityComponent
	{
	public:

		RenderDefinition(Mesh* mesh, Material* material);
		~RenderDefinition() = default;

		static RenderDefinition* ECCreate(Mesh* mesh, Material* material)
		{
			return s_entityComponentStorage.Alloc<RenderDefinition>(mesh, material);
		}

		static void ECDestroy(RenderDefinition* component)
		{
			s_entityComponentStorage.Free(component);
		}

		void OnConstruction() override;

		void OnDestruction() override { ECDestroy(this); }

		void GetReflection(CLib::Reflector& outReflector) override { outReflector.Init(this); }

		const Mesh* GetMesh() const { return m_mesh; }
		const Material* GetMaterial() const { return m_material; }

	private:

		RenderBoundingVolumeHierarchy::ObjectData m_objectData{};

		CLIB_REFLECTABLE(RenderDefinition,
			(Mesh*) m_mesh,
			(Material*) m_material
		)
	};
}