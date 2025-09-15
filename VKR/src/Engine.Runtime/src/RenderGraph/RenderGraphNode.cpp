#include "RenderGraphNode.h"
#include "Resource/Mesh.h"

namespace Eng
{
	RenderGraphBehaviour::RenderGraphBehaviour(PB::IRenderer* renderer, CLib::Allocator* allocator)
	{
		m_renderer = renderer;
		m_allocator = allocator;
	}

};