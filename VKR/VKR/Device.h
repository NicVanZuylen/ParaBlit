#pragma once
#include "VKRApi.h"
#include "ExtensionManager.h"

namespace VKR 
{
	class Device
	{
	public:

		VKR_API Device();

		VKR_API ~Device();

		/*
		Description: Find suitable physical device and create logical device with required extensions & features.
		*/
		VKR_API void Init(VkInstance instance);

		/*
		Description: Get the primary graphics queue family index.
		Return Type: int
		*/
		VKR_API int GetGraphicsQueueFamilyIndex();

		VKR_API VkDevice GetHandle();

		VKR_API VkPhysicalDevice GetPhysicalDevice();

	private:

		// Enumerate physical devices and use the most suitable one.
		inline VKR_API void EnumDevice();

		// Get a score rating for a physical device based upon it's available features & properties.
		inline VKR_API u64 GetDeviceScore(const VkPhysicalDeviceFeatures& features, const VkPhysicalDeviceProperties& properties);

		inline VKR_API void CreateQueues();

		// Query and enable necessary physical device extensions.
		inline VKR_API void EnableExtensions(ExtensionManager& extManager);

		// Query and enable device validation layers.
		inline VKR_API void EnableLayers(ExtensionManager& extManager);

		// Enable necessary device features.
		inline VKR_API void DisableUnecessaryFeatures();

		// Create logical device.
		inline VKR_API void CreateLogicalDevice();

		VkInstance m_instance = VK_NULL_HANDLE;
		VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
		VkPhysicalDeviceFeatures m_physDeviceFeatures = {};
		VkPhysicalDeviceProperties m_physDeviceProperties = {};
		VkDevice m_device = VK_NULL_HANDLE;

		int m_graphicsFamilyIndex = -1;
	};
}