#include "Device.h"
#include "DynamicArray.h"
#include "VKRDebug.h"

namespace VKR 
{
	Device::Device()
	{

	}

	Device::~Device()
	{
		if (m_device)
		{
			vkDestroyDevice(m_device, nullptr);
			m_device = VK_NULL_HANDLE;
		}
	}

	VKR_API void Device::Init(VkInstance instance)
	{
		VKR_ASSERT(instance, "Attempted to get device using null instance.");
		m_instance = instance;
		EnumDevice();
		CreateLogicalDevice();
	}

	void Device::EnumDevice()
	{
		u32 deviceCount = 0;
		vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

		DynamicArray<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.Data());

		u32 highestScoreIdx = 0;
		u64 highestScore = 0;
		for (u32 i = 0; i < deviceCount; ++i)
		{
			const VkPhysicalDevice& device = devices[i];

			// Get device features & properties, used to calculate a suitablility score.
			VkPhysicalDeviceFeatures deviceFeatures;
			vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(device, &deviceProperties);

			u64 score = GetDeviceScore(deviceFeatures, deviceProperties);

			// Track the device with the best score.
			if (score > highestScore)
			{
				highestScoreIdx = i;
				highestScore = score;
				m_physDeviceFeatures = deviceFeatures;
				m_physDeviceProperties = deviceProperties;
			}
		}

		// Make sure devices with enough score were found, if not we can't continue.
		VKR_ASSERT(highestScore > 0, "No suitable physical devices found.");

		// Pick the device with the best score.
		m_physicalDevice = devices[highestScoreIdx];
	}

	inline VKR_API u64 Device::GetDeviceScore(const VkPhysicalDeviceFeatures& features, const VkPhysicalDeviceProperties& properties)
	{
		u64 score = 0;

		// Add device general limits to score.
		score += properties.limits.maxImageDimension1D;
		score += properties.limits.maxImageDimension2D;
		score += properties.limits.maxImageDimension3D;
		score += properties.limits.maxFramebufferWidth;
		score += properties.limits.maxFramebufferHeight;
		score += properties.limits.maxMemoryAllocationCount;
		score += (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) * 10000ULL;

		// Required device features for this renderer.
		VkBool32 suitable = VK_TRUE; 
		suitable &= features.geometryShader;
		suitable &= features.tessellationShader;
		suitable &= features.shaderSampledImageArrayDynamicIndexing;
		suitable &= features.shaderStorageImageArrayDynamicIndexing;
		suitable &= features.shaderStorageBufferArrayDynamicIndexing;
		suitable &= (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU || m_physDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);

		if (suitable == VK_FALSE)
			score = 0;

		return score;
	}

	VKR_API void Device::CreateQueues()
	{
		VKR_ASSERT(m_physicalDevice);

		u32 queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);

		VKR_ASSERT(queueFamilyCount > 0, "No queue families found on device.");

		DynamicArray<VkQueueFamilyProperties> queueFamilyProps(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilyProps.Data());

		for (u32 i = 0; i < queueFamilyCount; ++i)
		{
			const VkQueueFamilyProperties& props = queueFamilyProps[i];

			// Fow now, we are only looking for a graphics queue.
			if (props.queueCount > 0 && m_graphicsFamilyIndex == -1 && props.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				m_graphicsFamilyIndex = i;
		}

		VKR_ASSERT(m_graphicsFamilyIndex > -1, "Could not find suitable graphics queue family.");
	}

	VKR_API void Device::EnableExtensions(ExtensionManager& extManager)
	{
		extManager.PrintAvailableExtensions();

		// Enable swapchain extension, we are useless without this.
		VKR_ASSERT(extManager.EnableExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME), "Could not enable swapchain extension.");
	}

	VKR_API void Device::EnableLayers(ExtensionManager& extManager)
	{
		extManager.PrintAvailableLayers();
	}

	VKR_API void Device::CreateLogicalDevice()
	{
		VkDeviceCreateInfo createInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, nullptr };
		createInfo.flags = 0;

		CreateQueues();

		VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr };
		queueInfo.flags = 0;
		queueInfo.queueFamilyIndex = m_graphicsFamilyIndex;
		queueInfo.queueCount = 1;
		float queuePriority = 1.0f;
		queueInfo.pQueuePriorities = &queuePriority;

		createInfo.queueCreateInfoCount = 1;
		createInfo.pQueueCreateInfos = &queueInfo;
		createInfo.pEnabledFeatures = &m_physDeviceFeatures;

		// Query and enable extensions & validation layers for the device.
		ExtensionManager extManager(m_physicalDevice);
		extManager.Query();
		EnableExtensions(extManager);
		EnableLayers(extManager);

		extManager.PrintEnabledExtensions();
		extManager.PrintAvailableLayers();

		auto enabledExt = extManager.GetEnabledExtensions();
		auto enabledLayers = extManager.GetEnabledLayers();

		createInfo.enabledExtensionCount = enabledExt.Count();
		createInfo.ppEnabledExtensionNames = enabledExt.Data();
		createInfo.enabledLayerCount = enabledLayers.Count();
		createInfo.ppEnabledLayerNames = enabledLayers.Data();

		VKR_ERROR_CHECK(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device), "Could not create logical device.");
		VKR_ASSERT(m_device);
	}
}