#include "IRenderer.h"
#include "Renderer.h"

namespace PB 
{
	IRenderer* CreateRenderer()
	{
		return new Renderer;
	}

	void DestroyRenderer(IRenderer*& renderer)
	{
		delete reinterpret_cast<Renderer*>(renderer);
		renderer = nullptr;
	}
}