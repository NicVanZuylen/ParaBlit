#pragma once
#include "vulkan/vulkan.h"
#include "glfw3.h"
#include "ExtensionManager.h"

namespace VKR 
{
	class VulkanInstance 
	{
	public:

		VKR_API VulkanInstance();

		VKR_API ~VulkanInstance();

		// Setup instance extensions.
		inline VKR_API void EnableExtensions();

		// Setup instance layers.
		inline VKR_API void EnableLayers();

		VKR_API void Create(const char** requiredExtensions, uint32_t extCount);

		VKR_API void Destroy();

	private:

		ExtensionManager m_instanceExtensionManager;
		VkInstance m_instance;
	};
}