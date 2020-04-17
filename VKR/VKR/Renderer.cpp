#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include "Renderer.h"
#include "VKRDebug.h"

namespace VKR 
{
	Renderer::Renderer(const RendererDesc& desc)
	{
		m_vkInstance.Create(desc.m_extensionNames, desc.m_extensionCount);
		m_device.Init(m_vkInstance.GetHandle());

		CreateWindowSurface(desc.m_windowInfo);
	}

	Renderer::~Renderer()
	{
		m_swapchain.Destroy();
		if (m_windowSurface)
			vkDestroySurfaceKHR(m_vkInstance.GetHandle(), m_windowSurface, nullptr);
	}

	Device* Renderer::GetDevice()
	{
		return &m_device;
	}

	void Renderer::CreateWindowSurface(WindowDesc* windowInfo)
	{
#ifdef VKR_WINDOWS
		VkWin32SurfaceCreateInfoKHR surfaceInfo =
		{
			VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
			nullptr,
			0,
			windowInfo->m_instance,
			windowInfo->m_handle
		};

		VKR_ERROR_CHECK(vkCreateWin32SurfaceKHR(m_vkInstance.GetHandle(), &surfaceInfo, nullptr, &m_windowSurface));
		VKR_ASSERT(m_windowSurface);
#endif
	}

	VKR_API void Renderer::CreateSwapChain(const SwapChainDesc& desc)
	{
		m_swapchainDesc = desc; // Cache desc for swap chain re-creation if swap-chain is lost/outdated.
		m_swapchain.Init(m_swapchainDesc, &m_device, m_windowSurface);
	}
}
