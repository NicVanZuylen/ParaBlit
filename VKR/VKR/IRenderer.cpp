#include "IRenderer.h"
#include "Renderer.h"

namespace VKR 
{
	IRenderer* CreateRenderer()
	{
		return new Renderer;
	}

	void DestroyRenderer(IRenderer*& renderer)
	{
		delete static_cast<Renderer*>(renderer);
		renderer = nullptr;
	}
}