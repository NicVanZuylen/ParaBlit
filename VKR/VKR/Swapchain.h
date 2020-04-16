#pragma once
#include "VKRApi.h"
#include <vulkan/vulkan.h>

namespace VKR 
{
	class Device;

	struct SwapChainDesc 
	{
		Device* m_device;
		VkSurfaceKHR m_windowSurface;
		u32 m_width;
		u32 m_height;
		bool m_useVSync = true;           // Choose a present mode that will present in-sync with display V-blanks.
		u8 m_imageCount;
	};

	class Swapchain
	{
	public:

		VKR_API Swapchain();

		VKR_API Swapchain(const SwapChainDesc& desc);

		VKR_API ~Swapchain();

	private:

		inline VKR_API void CreateSwapChain(const SwapChainDesc& desc);

		inline VKR_API VkSurfaceCapabilitiesKHR GetSurfaceCapabilities();

		inline VKR_API VkSurfaceFormatKHR ChooseSurfaceFormat(const SwapChainDesc& desc);

		inline VKR_API VkPresentModeKHR ChoosePresentMode(const SwapChainDesc& desc);

		VkSwapchainKHR m_handle;
		Device* m_device = nullptr;
		VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;
		u32 m_width;
		u32 m_height;
		u8 m_imageCount;
	};
}

