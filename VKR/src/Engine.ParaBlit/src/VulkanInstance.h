#pragma once
#include "ParaBlitApi.h"
#include "ExtensionManager.h"

namespace PB 
{
	class VulkanInstance 
	{
	public:

		PARABLIT_API VulkanInstance();

		PARABLIT_API ~VulkanInstance();

		// Setup instance extensions.
		inline PARABLIT_API void EnableExtensions();

		// Setup instance layers.
		inline PARABLIT_API void EnableLayers();

		PARABLIT_API void Create(const char** requiredExtensions, uint32_t extCount);

		PARABLIT_API void Destroy();

		PARABLIT_API VkInstance GetHandle();

	private:

		// Create debug messenger to print validation messages.
		inline PARABLIT_API void CreateDebugMessenger();

		inline PARABLIT_API void DestroyDebugMessenger();

		ExtensionManager m_instanceExtensionManager;
		VkDebugUtilsMessengerEXT m_messenger = VK_NULL_HANDLE;
		VkInstance m_instance = VK_NULL_HANDLE;
	};
}