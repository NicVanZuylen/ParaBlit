#pragma once
#include "ISwapChain.h"
#include "DynamicArray.h"
#include "ParaBlitApi.h"
#include "vulkan/vulkan.h"

namespace PB 
{
	class Renderer;
	class Device;
	class Texture;

	class Swapchain
	{
	public:

		PARABLIT_API Swapchain();

		PARABLIT_API ~Swapchain();

		/*
		Description: Create a swapchain using the provided swapchain description.
		Param: const SwapChainDesc& desc
		*/
		PARABLIT_API void Init(const SwapChainDesc& desc, Renderer* renderer, VkSurfaceKHR windowSurface);

		/*
		Description: Destroy an existing swapchain if there is one.
		*/
		PARABLIT_API void Destroy();

		PARABLIT_API VkSwapchainKHR GetHandle() const;

		PARABLIT_API const VkSwapchainKHR* GetHandlePtr() const;

		PARABLIT_API u8 ImageCount();

	private:

		inline PARABLIT_API void CreateSwapChain(const SwapChainDesc& desc);

		inline PARABLIT_API VkSurfaceCapabilitiesKHR GetSurfaceCapabilities();

		inline PARABLIT_API VkBool32 GetDeviceSurfaceSupport();

		inline PARABLIT_API VkSurfaceFormatKHR ChooseSurfaceFormat(const SwapChainDesc& desc);

		inline PARABLIT_API VkPresentModeKHR ChoosePresentMode(const SwapChainDesc& desc);

		inline PARABLIT_API void GetImages();

		VkSwapchainKHR m_handle = VK_NULL_HANDLE;
		Renderer* m_renderer = nullptr;
		Device* m_device = nullptr;
		VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;
		u32 m_width = 0;
		u32 m_height = 0;
		DynamicArray<VkImage> m_swapchainImages;
		DynamicArray<Texture*> m_wrappedSwapchainImages;
		u8 m_imageCount = 0;
	};
}

