#pragma once
#include "VKRApi.h"
#include "vulkan/vulkan.h"
#include "DynamicArray.h"
#include <unordered_map>
#include <string>
#include <vector>

namespace VKR
{
	class ExtensionManager
	{
	public:

		VKR_API ExtensionManager(VkPhysicalDevice device = VK_NULL_HANDLE);

		VKR_API ~ExtensionManager();

		// Find available extensions & layers.
		VKR_API void Query();

		// Enable an extension.
		VKR_API bool EnableExtension(const char* name);

		// Enable a list of extensions.
		VKR_API bool EnableExtension(const std::initializer_list<const char*>& names);
	
		// Enable a layer.
		VKR_API bool EnableLayer(const char* name);

		// Enable a list of layers.
		VKR_API bool EnableLayer(const std::initializer_list<const char*>& names);

		// Check if an extension with the provided name is available.
		VKR_API bool ExtensionAvailable(const char* name);

		// Check if an extension with the provided name is enabled.
		VKR_API bool ExtensionEnabled(const char* name);

		// Check if a layer with the provided name is available.
		VKR_API bool LayerAvailable(const char* name);

		// Check if a layer with the provided name is enabled.
		VKR_API bool LayerEnabled(const char* name);

		VKR_API void PrintAvailableExtensions();
		VKR_API void PrintEnabledExtensions();

		VKR_API void PrintAvailableLayers();
		VKR_API void PrintEnabledLayers();

		DynamicArray<const char*> GetEnabledExtensions();

		DynamicArray<const char*> GetEnabledLayers();

	private:

		VKR_API void EnumerateExt(uint32_t& count, VkExtensionProperties* props);
		VKR_API void EnumerateLayers(uint32_t& count, VkLayerProperties* props);

		VkPhysicalDevice m_device;
		std::unordered_map<std::string, bool> m_availableExt;
		std::unordered_map<std::string, bool> m_availableLayers;
	};
};
