#include "Device.h"
#include "ParaBlitDebug.h"

namespace PB 
{
	Device::Device()
	{

	}

	Device::~Device()
	{
		m_tempStagingBufferAllocator.Destroy();

		if (m_device)
		{
			m_allocator.Destroy();
			vkDestroyDevice(m_device, nullptr);
			m_device = VK_NULL_HANDLE;
		}
	}

	void Device::Init(VkInstance instance, Renderer* renderer, bool enableSwapchainExtension)
	{
		PB_ASSERT_MSG(instance, "Attempted to get device using null instance.");
		m_instance = instance;
		m_renderer = renderer;

		EnumDevice();
		CreateLogicalDevice(enableSwapchainExtension);
		m_allocator.Init(this);
		m_tempStagingBufferAllocator.Create(this);

		{
			static constexpr uint32_t BufferPoolSize = 1024 * 1024 * 8; // 8MB pools.
			static constexpr uint32_t BufferPoolMinAlignment = 256; // Common alignment requirement for Vulkan buffers.
			CLib::Vector<uint32_t> bufferAllocatorSegments =
			{
				0,
				128,
				256,
				512,
				1024,
				4096,
				8192,
				16384,
				1024 * 64,
				1024 * 256,
				1024 * 512,
				1024 * 1024,
				1024 * 1024 * 8,
				0
			};
			m_deviceBufferAllocator.Init(m_renderer, EMemoryType::DEVICE_LOCAL, BufferPoolSize, BufferPoolMinAlignment, bufferAllocatorSegments);
			m_hostBufferAllocator.Init(m_renderer, EMemoryType::HOST_VISIBLE, BufferPoolSize, BufferPoolMinAlignment, bufferAllocatorSegments);
		}

		{
			static constexpr uint32_t TexturePoolSize = 1024 * 1024 * 32; // 32MB pools.
			static constexpr uint32_t TexturePoolMinAlignment = 1024; // Common alignment requirement for Vulkan images.
			CLib::Vector<uint32_t> texAllocatorSegments =
			{
				0,
				512,
				1024 * 4,
				1024 * 16,
				1024 * 64,
				1024 * 128,
				1024 * 512,
				1024 * 1024,
				1024 * 1024 * 4,
				1024 * 1024 * 16,
				1024 * 1024 * 32,
				0
			};
			m_deviceTextureAllocator.Init(m_renderer, EMemoryType::DEVICE_LOCAL, TexturePoolSize, TexturePoolMinAlignment, texAllocatorSegments);
			m_hostTextureAllocator.Init(m_renderer, EMemoryType::HOST_VISIBLE, TexturePoolSize, TexturePoolMinAlignment, texAllocatorSegments);
		}
	}

	int Device::GetGraphicsQueueFamilyIndex()
	{
		return m_graphicsFamilyIndex;
	}

	VkDevice Device::GetHandle()
	{
		return m_device;
	}

	VkPhysicalDevice Device::GetPhysicalDevice()
	{
		return m_physicalDevice;
	}

	inline VkMemoryPropertyFlags GetVkMemPropertyFlags(const EMemoryType& memType)
	{
		switch (memType)
		{
		case EMemoryType::DEVICE_LOCAL:
			return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			break;
		case EMemoryType::HOST_VISIBLE:
			return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			break;
		case EMemoryType::END_RANGE:
			PB_ASSERT_MSG(false, "Invalid memory type provided.");
			return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			break;
		default:
			PB_NOT_IMPLEMENTED;
			return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			break;
		}
		return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	}

	u32 Device::FindMemoryTypeIndex(const u32& typeFilter, const EMemoryType& memType)
	{
		VkMemoryPropertyFlags propertyFlags = GetVkMemPropertyFlags(memType);
		for (u32 i = 0; i < m_memoryProperties.memoryTypeCount; ++i)
		{
			// Ensure the type is within the type filter and has the correct memory properties.
			auto& type = m_memoryProperties.memoryTypes[i];
			if (typeFilter & (1 << i) && (type.propertyFlags & propertyFlags) == propertyFlags)
				return i;
		}
		PB_ASSERT_MSG(false, "Requested memory requirements are not supported.");
		return 0;
	}

	DeviceAllocator& Device::GetDeviceAllocator()
	{
		return m_allocator;
	}

	TempBufferAllocator& Device::GetTempBufferAllocator()
	{
		return m_tempStagingBufferAllocator;
	}

	const VkPhysicalDeviceLimits* Device::GetDeviceLimits()
	{
		return &m_physDeviceProperties.properties.limits;
	}

	const VkPhysicalDeviceDescriptorIndexingProperties* Device::GetDescriptorIndexingProperties()
	{
		return &m_physDeviceDescIndexingProps;
	}

	const VkPhysicalDeviceDynamicRenderingFeaturesKHR* Device::GetDynamicRenderingFeatures()
	{
		return &m_physDeviceDynamicRenderingFeatures;
	}

	const VkPhysicalDeviceVulkan12Features* Device::GetVulkan12Features()
	{
		return &m_physDeviceVulkan12Features;
	}

	void Device::EnumDevice()
	{
		u32 deviceCount = 0;
		vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

		CLib::Vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.Data());

		u32 highestScoreIdx = 0;
		u64 highestScore = 0;
		for (u32 i = 0; i < deviceCount; ++i)
		{
			const VkPhysicalDevice& device = devices[i];

			// Get device features & properties, used to calculate a suitablility score.
			VkPhysicalDeviceFeatures2 deviceFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
			m_physDeviceDynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
			m_physDeviceVulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
			m_physDeviceVulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
			m_physDeviceMeshShaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;

			deviceFeatures.pNext = &m_physDeviceDynamicRenderingFeatures;
			m_physDeviceDynamicRenderingFeatures.pNext = &m_physDeviceVulkan11Features;
			m_physDeviceVulkan11Features.pNext = &m_physDeviceVulkan12Features;
			m_physDeviceVulkan12Features.pNext = &m_physDeviceMeshShaderFeatures;

			vkGetPhysicalDeviceFeatures(device, &deviceFeatures.features);
			vkGetPhysicalDeviceFeatures2(device, &deviceFeatures);

			VkPhysicalDeviceProperties2 deviceProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
			m_physDeviceDescIndexingProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;
			m_physDeviceMeshShaderProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;

			deviceProperties.pNext = &m_physDeviceDescIndexingProps;
			m_physDeviceDescIndexingProps.pNext = &m_physDeviceMeshShaderProperties;
			vkGetPhysicalDeviceProperties(device, &deviceProperties.properties);
			vkGetPhysicalDeviceProperties2(device, &deviceProperties);

			VkPhysicalDeviceMemoryProperties memoryProperties;
			vkGetPhysicalDeviceMemoryProperties(device, &memoryProperties);

			u64 score = GetDeviceScore(deviceFeatures.features, deviceProperties.properties);

			// Track the device with the best score.
			if (score > highestScore)
			{
				highestScoreIdx = i;
				highestScore = score;
				m_physDeviceFeatures = deviceFeatures;
				m_physDeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
				m_physDeviceProperties = deviceProperties;
				m_memoryProperties = memoryProperties;
			}
		}

		// Print device information.
		PB_LOG_FORMAT("Chosen Physical Device: %s", m_physDeviceProperties.properties.deviceName);

		u32 majorVersion = VK_VERSION_MAJOR(m_physDeviceProperties.properties.apiVersion);
		u32 minorVersion = VK_VERSION_MINOR(m_physDeviceProperties.properties.apiVersion);
		u32 patchVersion = VK_VERSION_PATCH(m_physDeviceProperties.properties.apiVersion);
		std::cout << "PARABLIT LOG: Device API Version: " << majorVersion << "." << minorVersion << "." << patchVersion << "\n";
		switch (m_physDeviceProperties.properties.deviceType)
		{
		case VK_PHYSICAL_DEVICE_TYPE_CPU:
			PB_LOG("Physical Device Type: CPU");
			break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
			PB_LOG("Physical Device Type: DISCRETE GPU");
			break;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
			PB_LOG("Physical Device Type: INTEGRATED GPU");
			break;
		case VK_PHYSICAL_DEVICE_TYPE_OTHER:
			PB_LOG("Physical Device Type: UNKNOWN");
			break;
		default:
			PB_LOG("Physical Device Type: UNKNOWN");
			break;
		}

		// Make sure devices with enough score were found, if not we can't continue.
		PB_ASSERT(highestScore > 0 && "No suitable physical devices found.");

		// Pick the device with the best score.
		m_physicalDevice = devices[highestScoreIdx];
	}

	inline u64 Device::GetDeviceScore(const VkPhysicalDeviceFeatures& features, const VkPhysicalDeviceProperties& properties)
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
		suitable &= (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU || m_physDeviceProperties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);

		if (suitable == VK_FALSE)
			score = 0;

		return score;
	}

	void Device::CreateQueues()
	{
		PB_ASSERT(m_physicalDevice);

		u32 queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);

		PB_ASSERT_MSG(queueFamilyCount > 0, "No queue families found on device.");

		CLib::Vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilyProps.Data());

		for (u32 i = 0; i < queueFamilyCount; ++i)
		{
			const VkQueueFamilyProperties& props = queueFamilyProps[i];

			// Fow now, we are only looking for a graphics queue.
			if (props.queueCount > 0 && m_graphicsFamilyIndex == -1 && props.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				m_graphicsFamilyIndex = i;
		}

		PB_ASSERT_MSG(m_graphicsFamilyIndex > -1, "Could not find suitable graphics queue family.");
	}

	void Device::EnableExtensions(ExtensionManager& extManager, bool enableSwapchainExtension)
	{
		extManager.PrintAvailableExtensions();

		if (enableSwapchainExtension)
		{
			PB_ASSERT_MSG(extManager.EnableExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME), "Could not enable swapchain extension.");
		}

		// Descriptor indexing is in the core, but this will shut up validation which complains about shaders requiring it.
		PB_ASSERT_MSG(extManager.EnableExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME), "Could not enable descriptor indexing extension.");

		PB_ASSERT_MSG(extManager.EnableExtension(VK_EXT_MESH_SHADER_EXTENSION_NAME), "Could not enable mesh shaders extension.");
		// Can be used to create dynamic render passes which do not include subpasses and the caveats of tile-based rendering approaches.
		// Currently disabled while VK_KHR_DYNAMIC_RENDERING is still in development.
		//PB_ASSERT_MSG(extManager.EnableExtension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME), "Could not enable dynamic rendering extension.");
	}

	void Device::EnableLayers(ExtensionManager& extManager)
	{
		extManager.PrintAvailableLayers();
	}

	void Device::SetFeatureEnables()
	{
		m_physDeviceFeatures.features.wideLines = VK_TRUE;
		m_physDeviceFeatures.features.largePoints = VK_FALSE;
		m_physDeviceFeatures.features.multiViewport = VK_FALSE;
		m_physDeviceFeatures.features.pipelineStatisticsQuery = VK_FALSE;

		// Used to enable DrawID/InstanceID parameters in shaders.
		m_physDeviceVulkan11Features.shaderDrawParameters = VK_TRUE;

		m_physDeviceVulkan12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
		m_physDeviceVulkan12Features.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
		m_physDeviceVulkan12Features.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
		m_physDeviceVulkan12Features.descriptorBindingStorageTexelBufferUpdateAfterBind = VK_FALSE;
		m_physDeviceVulkan12Features.descriptorBindingUniformBufferUpdateAfterBind = VK_FALSE;
		m_physDeviceVulkan12Features.descriptorBindingUniformTexelBufferUpdateAfterBind = VK_FALSE;
		m_physDeviceVulkan12Features.descriptorBindingUpdateUnusedWhilePending = VK_FALSE;

		m_physDeviceVulkan12Features.drawIndirectCount = VK_TRUE; // For GPU-driven frustrum culling.

		m_physDeviceDynamicRenderingFeatures.dynamicRendering = VK_FALSE; // TODO: Try this out when Vk_KHR_dynamic_rendering extension is complete.
	}

	void Device::CreateLogicalDevice(bool enableSwapchainExtension)
	{
		VkDeviceCreateInfo createInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, nullptr };
		createInfo.flags = 0;

		CreateQueues();
		SetFeatureEnables();

		VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr };
		queueInfo.flags = 0;
		queueInfo.queueFamilyIndex = m_graphicsFamilyIndex;
		queueInfo.queueCount = 1;
		float queuePriority = 1.0f;
		queueInfo.pQueuePriorities = &queuePriority;

		createInfo.queueCreateInfoCount = 1;
		createInfo.pQueueCreateInfos = &queueInfo;
		createInfo.pEnabledFeatures = nullptr;
		createInfo.pNext = &m_physDeviceFeatures;

		// Query and enable extensions & validation layers for the device.
		ExtensionManager extManager(m_physicalDevice);
		extManager.Query();
		EnableExtensions(extManager, enableSwapchainExtension);
		EnableLayers(extManager);
		
		extManager.PrintEnabledExtensions();
		extManager.PrintAvailableLayers();

		auto enabledExt = extManager.GetEnabledExtensions();
		auto enabledLayers = extManager.GetEnabledLayers();

		createInfo.enabledExtensionCount = enabledExt.Count();
		createInfo.ppEnabledExtensionNames = enabledExt.Data();
		createInfo.enabledLayerCount = enabledLayers.Count();
		createInfo.ppEnabledLayerNames = enabledLayers.Data();

		PB_ERROR_CHECK(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(m_device);
	}
}