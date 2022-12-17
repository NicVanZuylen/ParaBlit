#include "RenderDefinition.h"

namespace Eng
{
	RenderDefinition::RenderDefinition(const Mesh* mesh, const Material* material)
	{
		m_mesh = mesh;
		m_material = material;
	}
}