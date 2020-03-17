#include "Swapchain.h"

namespace VKR
{
	Swapchain::Swapchain() : m_device(0)
	{
	}

	Swapchain::Swapchain(VkDevice device)
	{
		m_device = device;

		CreateSwapChain();
	}

	Swapchain::~Swapchain()
	{

	}

	VKR_API void Swapchain::CreateSwapChain()
	{
		VkSwapchainCreateInfoKHR swapChainInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, nullptr };
		

	}

	VKR_API VkPresentModeKHR Swapchain::ChoosePresentMode()
	{
		return VK_PRESENT_MODE_MAILBOX_KHR;
	}
}
