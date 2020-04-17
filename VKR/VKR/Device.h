#pragma once
#include "ParaBlitApi.h"
#include "ExtensionManager.h"

namespace PB 
{
	class Device
	{
	public:

		PARABLIT_API Device();

		PARABLIT_API ~Device();

		/*
		Description: Find suitable physical device and create logical device with required extensions & features.
		*/
		PARABLIT_API void Init(VkInstance instance);

		/*
		Description: Get the primary graphics queue family index.
		Return Type: int
		*/
		PARABLIT_API int GetGraphicsQueueFamilyIndex();

		PARABLIT_API VkDevice GetHandle();

		PARABLIT_API VkPhysicalDevice GetPhysicalDevice();

	private:

		// Enumerate physical devices and use the most suitable one.
		inline PARABLIT_API void EnumDevice();

		// Get a score rating for a physical device based upon it's available features & properties.
		inline PARABLIT_API u64 GetDeviceScore(const VkPhysicalDeviceFeatures& features, const VkPhysicalDeviceProperties& properties);

		inline PARABLIT_API void CreateQueues();

		// Query and enable necessary physical device extensions.
		inline PARABLIT_API void EnableExtensions(ExtensionManager& extManager);

		// Query and enable device validation layers.
		inline PARABLIT_API void EnableLayers(ExtensionManager& extManager);

		// Enable necessary device features.
		inline PARABLIT_API void DisableUnecessaryFeatures();

		// Create logical device.
		inline PARABLIT_API void CreateLogicalDevice();

		VkInstance m_instance = VK_NULL_HANDLE;
		VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
		VkPhysicalDeviceFeatures m_physDeviceFeatures = {};
		VkPhysicalDeviceProperties m_physDeviceProperties = {};
		VkDevice m_device = VK_NULL_HANDLE;

		int m_graphicsFamilyIndex = -1;
	};
}