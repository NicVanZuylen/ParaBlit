#pragma once
#include "VulkanInstance.h"
#include "Device.h"

namespace VKR 
{
	struct RendererDesc 
	{
		const char** extensionNames = nullptr;
		u32 extensionCount = 0;
	};

	class Renderer
	{
	public:

		VKR_API Renderer(RendererDesc desc);

		VKR_API ~Renderer();

	private:

		VulkanInstance m_vkInstance;
		Device m_device;
	};
}

