#pragma once
#include "ISwapChain.h"
#include "CLib/Vector.h"
#include "ParaBlitApi.h"
#include "vulkan/vulkan.h"

namespace PB 
{
	class Renderer;
	class Device;
	class Texture;

	class Swapchain : public ISwapChain
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

		// Interface functions. Descriptions can be found in ISwapChain.h.

		PARABLIT_API ITexture* GetImage(u32 imageIndex) override;
		PARABLIT_API u32 GetWidth();
		PARABLIT_API u32 GetHeight();
		PARABLIT_API u32 GetImageCount() override;
		ETextureFormat GetImageFormat() override;

	private:

		inline PARABLIT_API void CreateSwapChain(const SwapChainDesc& desc);

		inline PARABLIT_API VkSurfaceCapabilitiesKHR GetSurfaceCapabilities();

		inline PARABLIT_API VkBool32 GetDeviceSurfaceSupport();

		inline PARABLIT_API VkSurfaceFormatKHR ChooseSurfaceFormat(const SwapChainDesc& desc);

		inline PARABLIT_API VkPresentModeKHR ChoosePresentMode(const SwapChainDesc& desc);

		inline PARABLIT_API void WrapImages();

		VkSwapchainKHR m_handle = VK_NULL_HANDLE;
		Renderer* m_renderer = nullptr;
		Device* m_device = nullptr;
		VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;
		u32 m_width = 0;
		u32 m_height = 0;
		CLib::Vector<VkImage, 3> m_swapchainImages;
		Texture* m_wrappedSwapchainImages = nullptr;
		ETextureFormat m_imageFormat = PB_TEXTURE_FORMAT_UNKNOWN;
		u8 m_imageCount = 0;
	};
}

