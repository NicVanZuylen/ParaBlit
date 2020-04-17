#pragma once
#include "ParaBlitApi.h"
#include "DynamicArray.h"
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <string>
#include <vector>

namespace PB
{
	class ExtensionManager
	{
	public:

		PARABLIT_API ExtensionManager(VkPhysicalDevice device = VK_NULL_HANDLE);

		PARABLIT_API ~ExtensionManager();

		// Find available extensions & layers.
		PARABLIT_API void Query();

		// Enable an extension.
		PARABLIT_API bool EnableExtension(const char* name);

		// Enable a list of extensions.
		PARABLIT_API bool EnableExtension(const std::initializer_list<const char*>& names);
	
		// Enable a layer.
		PARABLIT_API bool EnableLayer(const char* name);

		// Enable a list of layers.
		PARABLIT_API bool EnableLayer(const std::initializer_list<const char*>& names);

		// Check if an extension with the provided name is available.
		PARABLIT_API bool ExtensionAvailable(const char* name);

		// Check if an extension with the provided name is enabled.
		PARABLIT_API bool ExtensionEnabled(const char* name);

		// Check if a layer with the provided name is available.
		PARABLIT_API bool LayerAvailable(const char* name);

		// Check if a layer with the provided name is enabled.
		PARABLIT_API bool LayerEnabled(const char* name);

		PARABLIT_API void PrintAvailableExtensions();
		PARABLIT_API void PrintEnabledExtensions();

		PARABLIT_API void PrintAvailableLayers();
		PARABLIT_API void PrintEnabledLayers();

		DynamicArray<const char*> GetEnabledExtensions();

		DynamicArray<const char*> GetEnabledLayers();

	private:

		PARABLIT_API void EnumerateExt(uint32_t& count, VkExtensionProperties* props);
		PARABLIT_API void EnumerateLayers(uint32_t& count, VkLayerProperties* props);

		VkPhysicalDevice m_device;
		std::unordered_map<std::string, bool> m_availableExt;
		std::unordered_map<std::string, bool> m_availableLayers;
	};
};
