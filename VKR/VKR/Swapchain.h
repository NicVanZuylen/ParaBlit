#pragma once
#include "ISwapChain.h"
#include "DynamicArray.h"
#include "ParaBlitApi.h"
#include <vulkan/vulkan.h>

namespace PB 
{
	class Device;

	//enum EPresentMode : u16
	//{
	//	VKR_PRESENT_MODE_IMMEDIATE,
	//	VKR_PRESENT_MODE_MAILBOX,
	//	VKR_PRESENT_MODE_FIFO,
	//	VKR_PRESENT_MODE_FIFO_RELAXED,
	//	VKR_PRESENT_MODE_END_RANGE,
	//};

	//struct SwapChainDesc
	//{
	//	u32 m_width = 0;                                      // Leave as zero to use the surface dimension.
	//	u32 m_height = 0;                                     // Leave as zero to use the surface dimension.
	//	EPresentMode m_presentMode = VKR_PRESENT_MODE_FIFO;
	//	u16 m_imageCount = 3;
	//};

	class Swapchain
	{
	public:

		PARABLIT_API Swapchain();

		PARABLIT_API ~Swapchain();

		/*
		Description: Create a swapchain using the provided swapchain description.
		Param: const SwapChainDesc& desc
		*/
		PARABLIT_API void Init(const SwapChainDesc& desc, Device* device, VkSurfaceKHR windowSurface);

		/*
		Description: Destroy an existing swapchain if there is one.
		*/
		PARABLIT_API void Destroy();

	private:

		inline PARABLIT_API void CreateSwapChain(const SwapChainDesc& desc);

		inline PARABLIT_API VkSurfaceCapabilitiesKHR GetSurfaceCapabilities();

		inline PARABLIT_API VkBool32 GetDeviceSurfaceSupport();

		inline PARABLIT_API VkSurfaceFormatKHR ChooseSurfaceFormat(const SwapChainDesc& desc);

		inline PARABLIT_API VkPresentModeKHR ChoosePresentMode(const SwapChainDesc& desc);

		VkSwapchainKHR m_handle;
		Device* m_device = nullptr;
		VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;
		u32 m_width;
		u32 m_height;
		DynamicArray<VkImage> m_swapchainImages;
		u8 m_imageCount;
	};
}

