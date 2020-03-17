#pragma once
#include "VKRApi.h"

#include <vulkan/vulkan.h>

namespace VKR 
{
	class Swapchain
	{
	public:

		VKR_API Swapchain();

		VKR_API Swapchain(VkDevice device);

		VKR_API ~Swapchain();

	private:

		inline VKR_API void CreateSwapChain();

		inline VKR_API VkPresentModeKHR ChoosePresentMode();

		VkDevice m_device = VK_NULL_HANDLE;
	};
}

