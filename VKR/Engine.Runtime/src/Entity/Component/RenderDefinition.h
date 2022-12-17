#include "Entity/Entity.h"
#include "Mesh.h"
#include "Material.h"

namespace Eng
{
	class RenderDefinition : public EntityComponent
	{
	public:

		RenderDefinition(const Mesh* mesh, const Material* material);
		~RenderDefinition() = default;

		static RenderDefinition* ECCreate(const Mesh* mesh, const Material* material)
		{
			return s_entityComponentStorage.Alloc<RenderDefinition>(mesh, material);
		}

		static void ECDestroy(RenderDefinition* component)
		{
			s_entityComponentStorage.Free(component);
		}

		void OnDestruction() override { ECDestroy(this); }

		void GetReflection(CLib::Reflector& outReflector) override { outReflector.Init(this); }

		const Mesh* GetMesh() const { return m_mesh; }
		const Material* GetMaterial() const { return m_material; }

	private:

		CLIB_REFLECTABLE(RenderDefinition,
			(const Mesh*) m_mesh,
			(const Material*) m_material
		)
	};
}