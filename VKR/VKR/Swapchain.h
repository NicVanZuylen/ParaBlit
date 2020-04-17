#pragma once
#include "ISwapChain.h"
#include "DynamicArray.h"
#include "VKRApi.h"
#include <vulkan/vulkan.h>

namespace VKR 
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

		VKR_API Swapchain();

		VKR_API ~Swapchain();

		/*
		Description: Create a swapchain using the provided swapchain description.
		Param: const SwapChainDesc& desc
		*/
		VKR_API void Init(const SwapChainDesc& desc, Device* device, VkSurfaceKHR windowSurface);

		/*
		Description: Destroy an existing swapchain if there is one.
		*/
		VKR_API void Destroy();

	private:

		inline VKR_API void CreateSwapChain(const SwapChainDesc& desc);

		inline VKR_API VkSurfaceCapabilitiesKHR GetSurfaceCapabilities();

		inline VKR_API VkBool32 GetDeviceSurfaceSupport();

		inline VKR_API VkSurfaceFormatKHR ChooseSurfaceFormat(const SwapChainDesc& desc);

		inline VKR_API VkPresentModeKHR ChoosePresentMode(const SwapChainDesc& desc);

		VkSwapchainKHR m_handle;
		Device* m_device = nullptr;
		VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;
		u32 m_width;
		u32 m_height;
		DynamicArray<VkImage> m_swapchainImages;
		u8 m_imageCount;
	};
}

