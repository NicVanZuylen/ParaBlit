#include "VulkanInstance.h"
#include "VKRDebug.h"
#include "DynamicArray.h"
#include "glfw3.h"

namespace VKR 
{
	VulkanInstance::VulkanInstance()
	{
		m_instance = VK_NULL_HANDLE;
	}

	VulkanInstance::~VulkanInstance()
	{
		if (m_instance)
			Destroy();
	}

	void VulkanInstance::EnableExtensions()  
	{
		m_instanceExtensionManager.PrintAvailableExtensions();
	}

	void VulkanInstance::EnableLayers()
	{
		m_instanceExtensionManager.PrintAvailableLayers();
	}

	void VulkanInstance::Create(const char** requiredExtensions, uint32_t extCount)
	{
		VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr };
		appInfo.apiVersion = VK_MAKE_VERSION(1, 1, VK_HEADER_VERSION);
		appInfo.applicationVersion = 1;
		appInfo.engineVersion = 1;
		appInfo.pApplicationName = "VKR";
		appInfo.pEngineName = "VKR";

		VkInstanceCreateInfo cInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr };
		cInfo.flags = 0;
		cInfo.pApplicationInfo = &appInfo;

		// Query for available instance extensions & layers.
		m_instanceExtensionManager.Query();

		// Setup extensions & layers for this instance.
		VKR_ASSERT(requiredExtensions != nullptr || extCount == 0, "Valid extension name array not provided.");
		for (uint32_t i = 0; i < extCount; ++i)
			m_instanceExtensionManager.EnableExtension(requiredExtensions[i]);

		EnableExtensions();
		EnableLayers();

		m_instanceExtensionManager.PrintEnabledExtensions();
		m_instanceExtensionManager.PrintEnabledLayers();

		DynamicArray<const char*> enabledExtensions = m_instanceExtensionManager.GetEnabledExtensions();
		cInfo.enabledExtensionCount = enabledExtensions.Count();
		cInfo.ppEnabledExtensionNames = enabledExtensions.Data();

		DynamicArray<const char*> enabledLayers = m_instanceExtensionManager.GetEnabledLayers();
		cInfo.enabledLayerCount = enabledLayers.Count();
		cInfo.ppEnabledLayerNames = enabledLayers.Data();

		VKR_ERROR_CHECK(vkCreateInstance(&cInfo, nullptr, &m_instance), "Failed to create Vulkan instance!");
		VKR_BREAK_ON_ERROR;
	}

	void VulkanInstance::Destroy()
	{
		if(m_instance != VK_NULL_HANDLE) 
		{
			vkDestroyInstance(m_instance, nullptr);
			m_instance = VK_NULL_HANDLE;
		}
	}
}
