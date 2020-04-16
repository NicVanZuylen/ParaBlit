#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include "Renderer.h"
#include "VKRDebug.h"

namespace VKR 
{
	Renderer::Renderer(RendererDesc desc)
	{
		m_vkInstance.Create(desc.m_extensionNames, desc.m_extensionCount);
		m_device.Init(m_vkInstance.GetHandle());

		CreateWindowSurface(desc.m_windowInfo);
	}

	Renderer::~Renderer()
	{
		if (m_windowSurface)
			vkDestroySurfaceKHR(m_vkInstance.GetHandle(), m_windowSurface, nullptr);
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
}
