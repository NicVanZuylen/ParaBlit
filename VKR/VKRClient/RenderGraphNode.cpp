#include "RenderGraphNode.h"
#include "Mesh.h"

RenderGraphBehaviour::RenderGraphBehaviour(PB::IRenderer* renderer, CLib::Allocator* allocator)
{
	m_renderer = renderer;
	m_allocator = allocator;
}
