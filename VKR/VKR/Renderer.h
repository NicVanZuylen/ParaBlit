#pragma once
#include "VulkanInstance.h"
#include "Device.h"
#include "Swapchain.h"

#ifdef VKR_WINDOWS
#include <Windows.h>
#endif

namespace VKR 
{
#ifdef VKR_WINDOWS
	struct Win32WindowInfo
	{
		HINSTANCE m_instance;
		HWND m_handle;
	};
	using WindowDesc = Win32WindowInfo;
#else
	using WindowDesc = void*;
#endif


	struct RendererDesc 
	{
		const char** m_extensionNames = nullptr;
		u32 m_extensionCount = 0;

		// Platform-native window info.
		WindowDesc* m_windowInfo;
	};

	class Renderer
	{
	public:

		VKR_API Renderer(const RendererDesc& desc);

		VKR_API ~Renderer();

		VKR_API Device* GetDevice();

		VKR_API void CreateSwapChain(const SwapChainDesc& desc);

	private:

		VKR_API inline void CreateWindowSurface(WindowDesc* windowHandle);


		VulkanInstance m_vkInstance;
		Device m_device;
		VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;
		SwapChainDesc m_swapchainDesc;
		Swapchain m_swapchain;
	};
}

