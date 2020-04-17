#pragma once
#include "VKRApi.h"
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

		VKR_API VkInstance GetHandle();

	private:

		// Create debug messenger to print validation messages.
		inline VKR_API void CreateDebugMessenger();

		inline VKR_API void DestroyDebugMessenger();

		ExtensionManager m_instanceExtensionManager;
		VkDebugUtilsMessengerEXT m_messenger = VK_NULL_HANDLE;
		VkInstance m_instance = VK_NULL_HANDLE;
	};
}