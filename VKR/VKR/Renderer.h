#pragma once
#include "IRenderer.h"
#include "ParaBlitApi.h"
#include "VulkanInstance.h"
#include "Device.h"
#include "Swapchain.h"
#include <vulkan/vulkan.h>

namespace PB 
{
	class Renderer : public IRenderer
	{
	public:

		PARABLIT_API Renderer();

		PARABLIT_API ~Renderer();

		PARABLIT_API void Init(const RendererDesc& desc) override;

		PARABLIT_API void CreateSwapChain(const SwapChainDesc& desc) override;

	private:

		PARABLIT_API inline void CreateWindowSurface(WindowDesc* windowHandle) override;


		VulkanInstance m_vkInstance;
		Device m_device;
		VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;
		SwapChainDesc m_swapchainDesc;
		Swapchain m_swapchain;
	};
}

