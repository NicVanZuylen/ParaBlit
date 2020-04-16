#include "Swapchain.h"
#include "VKRDebug.h"
#include "Device.h"

namespace VKR
{
	Swapchain::Swapchain()
	{
	}

	Swapchain::Swapchain(const SwapChainDesc& desc)
	{
		m_device = desc.m_device;
		m_windowSurface = desc.m_windowSurface;
		m_width = desc.m_width;
		m_height = desc.m_height;
		m_imageCount = desc.m_imageCount;

		CreateSwapChain(desc);
	}

	Swapchain::~Swapchain()
	{
		if (m_handle)
		{
			vkDestroySwapchainKHR(m_device->GetHandle(), m_handle, nullptr);
			m_handle = VK_NULL_HANDLE;
		}
	}

	void Swapchain::CreateSwapChain(const SwapChainDesc& desc)
	{
		auto format = ChooseSurfaceFormat(desc);
		auto surfaceCapabilities = GetSurfaceCapabilities();

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

		VKR_ERROR_CHECK(vkCreateSwapchainKHR(m_device->GetHandle(), &swapChainInfo, nullptr, &m_handle));
		VKR_ASSERT(m_handle);
	}

	VkSurfaceCapabilitiesKHR Swapchain::GetSurfaceCapabilities()
	{
		VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
		VKR_ERROR_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_device->GetPhysicalDevice(), m_windowSurface, &surfaceCapabilities));

		return surfaceCapabilities;
	}

	VkSurfaceFormatKHR Swapchain::ChooseSurfaceFormat(const SwapChainDesc& desc)
	{
		VkSurfaceFormatKHR desiredFormat = { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

		u32 formatCount = 0;
		VKR_ERROR_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_device->GetPhysicalDevice(), m_windowSurface, &formatCount, nullptr));
		VKR_ASSERT(formatCount > 0);

		DynamicArray<VkSurfaceFormatKHR> availableFormats(formatCount);
		VKR_ERROR_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_device->GetPhysicalDevice(), m_windowSurface, &formatCount, availableFormats.Data()));

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

		VKR_LOG("WARNING: Desired swap chain format is not available on this device.");
		return availableFormats[0];
	}

	VkPresentModeKHR Swapchain::ChoosePresentMode(const SwapChainDesc& desc)
	{
		u32 presentModeCount = 0;
		VKR_ERROR_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_device->GetPhysicalDevice(), m_windowSurface, &presentModeCount, nullptr));
		VKR_ASSERT(presentModeCount > 0);

		DynamicArray<VkPresentModeKHR> presentModes(presentModeCount);
		presentModes.SetCount(presentModeCount);
		VKR_ERROR_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_device->GetPhysicalDevice(), m_windowSurface, &presentModeCount, presentModes.Data()));

		auto bestMode = VK_PRESENT_MODE_FIFO_KHR;
		for (u32 i = 0; i < presentModes.Count(); ++i)
		{
			if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR && desc.m_useVSync)
				return presentModes[i]; // Always prefer mailbox, unless V-sync is not allowed in the desc.
			else if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
				bestMode = presentModes[i];
		}

		return bestMode;
	}
}
