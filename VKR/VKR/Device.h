#pragma once
#include "ParaBlitApi.h"
#include "ExtensionManager.h"
#include "ParaBlitDefs.h"
#include "DeviceAllocator.h"
#include "PoolAllocator.h"
#include "StagingBufferAllocator.h"

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

		/*
		Description: Get the memory type index for a specific memory type.
		Return Type: u32
		*/
		PARABLIT_API u32 FindMemoryTypeIndex(const u32& typeFilter, const EMemoryType& memType);

		/*
		Description: Gets the legacy memory allocator for allocating memory on this device.
		Return Type: DeviceAllocator&
		*/
		PARABLIT_API DeviceAllocator& GetDeviceAllocator();

		/*
		Description: Gets the memory allocator for allocating memory with the specified memory type.
		Return Type: PoolAllocator&
		Param:
			EMemoryType memoryType: The memory type used to get the correct allocator.
		*/
		PoolAllocator& GetBufferAllocator(EMemoryType memoryType) 
		{
			switch (memoryType)
			{
			case PB::EMemoryType::HOST_VISIBLE:
				return m_hostBufferAllocator;
			case PB::EMemoryType::DEVICE_LOCAL:
				return m_deviceBufferAllocator;
			case PB::EMemoryType::END_RANGE:
			default:
				PB_NOT_IMPLEMENTED;
				return m_hostBufferAllocator;
				break;
			}
		};

		/*
		Description: Gets the memory allocator for allocating memory on this device.
		Return Type: PoolAllocator&
		*/
		PoolAllocator& GetTextureAllocator() { return m_textureAllocator; };

		/*
		Description: Gets the temporary buffer allocator.
		Return Type: TempBufferAllocator&
		*/
		TempBufferAllocator& GetTempBufferAllocator();

		/*
		Description: Gets the limits structure of this device.
		Return Type const VkPhysicalDeviceLimits*
		*/
		const VkPhysicalDeviceLimits* GetDeviceLimits();

		/*
		Description: Gets the properties defining the descriptor indexing limites of the device.
		Return Type const VkPhysicalDeviceDescriptorIndexingProperties*
		*/
		const VkPhysicalDeviceDescriptorIndexingProperties* GetDescriptorIndexingProperties();

		/*
		Description: Gets the feature toggle for dynamic rendering.
		Return Type const VkPhysicalDeviceDynamicRenderingFeaturesKHR*
		*/
		const VkPhysicalDeviceDynamicRenderingFeaturesKHR* GetDynamicRenderingFeatures();

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
		VkPhysicalDeviceFeatures2 m_physDeviceFeatures = {};
		VkPhysicalDeviceDescriptorIndexingFeatures m_physDeviceDescIndexingFeatures = {};
		VkPhysicalDeviceDynamicRenderingFeaturesKHR m_physDeviceDynamicRenderingFeatures = {};
		VkPhysicalDeviceProperties2 m_physDeviceProperties = {};
		VkPhysicalDeviceDescriptorIndexingProperties m_physDeviceDescIndexingProps = {};
		VkPhysicalDeviceMemoryProperties m_memoryProperties = {};
		VkDevice m_device = VK_NULL_HANDLE;

		int m_graphicsFamilyIndex = -1;
		DeviceAllocator m_allocator;
		PoolAllocator m_deviceBufferAllocator;
		PoolAllocator m_hostBufferAllocator;
		PoolAllocator m_textureAllocator;
		TempBufferAllocator m_tempStagingBufferAllocator;
	};
}