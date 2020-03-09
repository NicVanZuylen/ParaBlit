#include "Renderer.h"

namespace VKR 
{
	Renderer::Renderer(RendererDesc desc)
	{
		m_vkInstance.Create(desc.extensionNames, desc.extensionCount);
		m_device.Init(m_vkInstance.GetHandle());
	}

	Renderer::~Renderer()
	{

	}
}
