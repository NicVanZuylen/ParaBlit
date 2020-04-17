#pragma once
#include "IRenderer.h"
#include "VKRApi.h"
#include "VulkanInstance.h"
#include "Device.h"
#include "Swapchain.h"
#include <vulkan/vulkan.h>

//#ifdef VKR_WINDOWS
//#include <Windows.h>
//#endif

namespace VKR 
{
//#ifdef VKR_WINDOWS
//	struct Win32WindowInfo
//	{
//		HINSTANCE m_instance;
//		HWND m_handle;
//	};
//	using WindowDesc = Win32WindowInfo;
//#else
//	using WindowDesc = void*;
//#endif
//
//
//	struct RendererDesc 
//	{
//		const char** m_extensionNames = nullptr;
//		u32 m_extensionCount = 0;
//
//		// Platform-native window info.
//		WindowDesc* m_windowInfo;
//	};

	class Renderer : public IRenderer
	{
	public:

		VKR_API Renderer();

		VKR_API ~Renderer();

		VKR_API void Init(const RendererDesc& desc) override;

		VKR_API void CreateSwapChain(const SwapChainDesc& desc) override;

	private:

		VKR_API inline void CreateWindowSurface(WindowDesc* windowHandle) override;


		VulkanInstance m_vkInstance;
		Device m_device;
		VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;
		SwapChainDesc m_swapchainDesc;
		Swapchain m_swapchain;
	};
}

