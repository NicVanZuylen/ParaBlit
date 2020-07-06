#include "Swapchain.h"
#include "Renderer.h"
#include "ParaBlitDebug.h"
#include "Device.h"
#include "Texture.h"
#include "PBUtil.h"

namespace PB
{
	Swapchain::Swapchain()
	{
	}

	Swapchain::~Swapchain()
	{
	}

	void Swapchain::Init(const SwapChainDesc& desc, Renderer* renderer, VkSurfaceKHR windowSurface)
	{
		PB_ASSERT(m_device == nullptr);

		m_renderer = renderer;
		m_device = m_renderer->GetDevice();
		m_windowSurface = windowSurface;
		m_width = desc.m_width;
		m_height = desc.m_height;
		m_imageCount = desc.m_imageCount;

		PB_ASSERT(m_device);
		PB_ASSERT(m_imageCount > 0, "Swap chain image count must be greater than zero.");
		PB_ASSERT(desc.m_presentMode < VKR_PRESENT_MODE_END_RANGE, "Invalid present mode.");

		CreateSwapChain(desc);
	}

	void Swapchain::Destroy()
	{
		if (m_handle)
		{
			vkDestroySwapchainKHR(m_device->GetHandle(), m_handle, nullptr);

			m_swapchainImages.Clear();
			for (u32 i = 0; i < m_swapchainImages.Count(); ++i)
			{
				m_wrappedSwapchainImages[i].Destroy();
			}
			delete[] m_wrappedSwapchainImages;

			m_handle = VK_NULL_HANDLE;
		}
	}

	VkSwapchainKHR Swapchain::GetHandle() const
	{
		return m_handle;
	}

	const VkSwapchainKHR* Swapchain::GetHandlePtr() const
	{
		return &m_handle;
	}

	ITexture* Swapchain::GetImage(u32 imageIndex)
	{
		return reinterpret_cast<ITexture*>(&m_wrappedSwapchainImages[imageIndex]);
	}

	u32 Swapchain::GetWidth()
	{
		return m_width;
	}

	u32 Swapchain::GetHeight()
	{
		return m_height;
	}

	u32 Swapchain::GetImageCount()
	{
		return m_swapchainImages.Count();
	}

	void Swapchain::CreateSwapChain(const SwapChainDesc& desc)
	{
		auto format = ChooseSurfaceFormat(desc);
		auto surfaceCapabilities = GetSurfaceCapabilities();

		PB_ASSERT(GetDeviceSurfaceSupport() != VK_FALSE, "Physical device does not support surface.");
		PB_ASSERT(m_width <= surfaceCapabilities.currentExtent.width && m_height < surfaceCapabilities.currentExtent.height, "Swap chain width/height cannot be greater than window dimensions.");

		if (m_width == 0)
			m_width = surfaceCapabilities.currentExtent.width;
		if (m_height == 0)
			m_height = surfaceCapabilities.currentExtent.height;

		VkSwapchainCreateInfoKHR swapChainInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, nullptr };
		swapChainInfo.surface = m_windowSurface;
		swapChainInfo.imageFormat = format.format;
		swapChainInfo.imageColorSpace = format.colorSpace;
		swapChainInfo.imageExtent = { m_width, m_height };
		swapChainInfo.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		swapChainInfo.presentMode = ChoosePresentMode(desc);
		swapChainInfo.minImageCount = m_imageCount;
		swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapChainInfo.imageArrayLayers = 1;
		swapChainInfo.preTransform = surfaceCapabilities.currentTransform;
		swapChainInfo.clipped = VK_TRUE;
		swapChainInfo.oldSwapchain = VK_NULL_HANDLE;

		PB_ERROR_CHECK(vkCreateSwapchainKHR(m_device->GetHandle(), &swapChainInfo, nullptr, &m_handle));
		PB_ASSERT(m_handle);

		WrapImages();
	}

	VkSurfaceCapabilitiesKHR Swapchain::GetSurfaceCapabilities()
	{
		VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
		PB_ERROR_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_device->GetPhysicalDevice(), m_windowSurface, &surfaceCapabilities));

		return surfaceCapabilities;
	}

	VkBool32 Swapchain::GetDeviceSurfaceSupport()
	{
		VkBool32 surfaceSupported = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(m_device->GetPhysicalDevice(), m_device->GetGraphicsQueueFamilyIndex(), m_windowSurface, &surfaceSupported);
		return surfaceSupported;
	}

	VkSurfaceFormatKHR Swapchain::ChooseSurfaceFormat(const SwapChainDesc& desc)
	{
		VkSurfaceFormatKHR desiredFormat = { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

		u32 formatCount = 0;
		PB_ERROR_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_device->GetPhysicalDevice(), m_windowSurface, &formatCount, nullptr));
		PB_ASSERT(formatCount > 0);

		DynamicArray<VkSurfaceFormatKHR> availableFormats(formatCount);
		PB_ERROR_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_device->GetPhysicalDevice(), m_windowSurface, &formatCount, availableFormats.Data()));

		if (availableFormats.Count() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
		{
			// Any format is available.
			return desiredFormat;
		}

		for (u32 i = 0; i < availableFormats.Count(); ++i)
		{
			if (availableFormats[i].format == desiredFormat.format && availableFormats[i].colorSpace == desiredFormat.colorSpace)
				return availableFormats[i];
		}

		if(availableFormats[0].format != desiredFormat.format || availableFormats[0].colorSpace != desiredFormat.colorSpace)
			PB_LOG("WARNING: Desired swap chain format is not available on this device.");
		return availableFormats[0];
	}

	VkPresentModeKHR Swapchain::ChoosePresentMode(const SwapChainDesc& desc)
	{
		u32 presentModeCount = 0;
		PB_ERROR_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_device->GetPhysicalDevice(), m_windowSurface, &presentModeCount, nullptr));
		PB_ASSERT(presentModeCount > 0);

		DynamicArray<VkPresentModeKHR> presentModes(presentModeCount);
		presentModes.SetCount(presentModeCount);
		PB_ERROR_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_device->GetPhysicalDevice(), m_windowSurface, &presentModeCount, presentModes.Data()));

		auto bestMode = VK_PRESENT_MODE_FIFO_KHR;
		if (bestMode == desc.m_presentMode)
			return bestMode;

		for (u32 i = 0; i < presentModes.Count(); ++i)
		{
			if (presentModes[i] == desc.m_presentMode)
				return presentModes[i]; // Always prefer mailbox, unless V-sync is not allowed in the desc.
			else if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
				bestMode = presentModes[i];
		}

		PB_LOG("WARNING: Preferred present mode not available, falling back to next available mode.");
		return bestMode;
	}

	void Swapchain::WrapImages()
	{
		u32 imageCount = 0;
		vkGetSwapchainImagesKHR(m_device->GetHandle(), m_handle, &imageCount, nullptr);
		PB_ASSERT(imageCount > 0, "Attempting to retrieve swapchain images when no swapchain images currently exist.");

		m_swapchainImages.SetSize(imageCount);
		m_swapchainImages.SetCount(imageCount);
		vkGetSwapchainImagesKHR(m_device->GetHandle(), m_handle, &imageCount, m_swapchainImages.Data());

		m_wrappedSwapchainImages = new Texture[imageCount];

		// Wrap swapchain images into ParaBlit texture objects.
		WrappedTextureDesc wrappedTextureDesc = {};
		for (u32 i = 0; i < imageCount; ++i)
		{
			PB_ASSERT(m_swapchainImages[i], "Attempting to wrap NULL swapchain image");
			wrappedTextureDesc.m_wrappedImage = m_swapchainImages[i];
			wrappedTextureDesc.m_usageFlags = PB_TEXTURE_STATE_PRESENT | PB_TEXTURE_STATE_COLORTARGET;
			m_wrappedSwapchainImages[i].Create(m_renderer, wrappedTextureDesc);
		}

		// Transition images to initial layout (present)

		CommandContext internalContext;
		MakeInternalContext(internalContext, m_renderer);
		internalContext.Begin();

		for (u32 i = 0; i < imageCount; ++i)
		{
			internalContext.CmdTransitionTexture(reinterpret_cast<ITexture*>(&m_wrappedSwapchainImages[i]), PB_TEXTURE_STATE_PRESENT, {});
		}

		internalContext.End();
		internalContext.Return();
	}
}
