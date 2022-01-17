#include "VulkanInstance.h"
#include "ParaBlitDebug.h"
#include "CLib/Vector.h"
#include "CommandContext.h"

#define VK_LAYER_KHRONOS_VALIDATION_NAME "VK_LAYER_KHRONOS_validation"
#define VK_LAYER_LUNARG_VALIDATION_NAME "VK_LAYER_LUNARG_standard_validation"

#ifndef PB_USE_DEBUG_MESSENGER
#define PB_USE_DEBUG_MESSENGER 1
#endif

#define PB_SHOW_VALIDATION_ERRORS 1 && PB_USE_DEBUG_MESSENGER
#define PB_SHOW_VALIDATION_WARNINGS 1 && PB_USE_DEBUG_MESSENGER
#define PB_SHOW_VALIDATION_INFO 0 && PB_USE_DEBUG_MESSENGER

namespace PB 
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

		m_instanceExtensionManager.EnableExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		m_instanceExtensionManager.EnableExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback
	(
		VkDebugUtilsMessageSeverityFlagBitsEXT severity,
		VkDebugUtilsMessageTypeFlagsEXT type,
		const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
		void* userData
	)
	{
		switch (severity)
		{
#if PB_SHOW_VALIDATION_ERRORS
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			PB_LOG_FORMAT("VALIDATION ERROR: %s", callbackData->pMessage);
			break;
#endif
#if PB_SHOW_VALIDATION_WARNINGS
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			PB_LOG_FORMAT("VALIDATION WARNING: %s", callbackData->pMessage);
			break;
#endif
#if PB_SHOW_VALIDATION_INFO
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			PB_LOG_FORMAT("VALIDATION INFO: %s", callbackData->pMessage);
			break;
#endif
		default:
			break;
		}

		return VK_FALSE;
	}

	void VulkanInstance::EnableLayers()
	{
		m_instanceExtensionManager.PrintAvailableLayers();

#if PARABLIT_ENABLE_API_VALIDATION
		// Enable Khronos validation if available, otherwise use LunarG validation.
		if (!m_instanceExtensionManager.EnableLayer(VK_LAYER_KHRONOS_VALIDATION_NAME))
			PB_ASSERT_MSG(m_instanceExtensionManager.EnableLayer(VK_LAYER_LUNARG_VALIDATION_NAME), "Could not enable a suitable validation layer.");
#endif
	}

	void VulkanInstance::Create(const char** requiredExtensions, uint32_t extCount)
	{
		VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr };
		appInfo.apiVersion = VK_MAKE_VERSION(1, 2, VK_HEADER_VERSION);
		appInfo.applicationVersion = 1;
		appInfo.engineVersion = 1;
		appInfo.pApplicationName = "ParaBlitClient_debug";
		appInfo.pEngineName = "ParaBlit";

		VkInstanceCreateInfo cInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr };
		cInfo.flags = 0;
		cInfo.pApplicationInfo = &appInfo;

		// Query for available instance extensions & layers.
		m_instanceExtensionManager.Query();

		// Setup extensions & layers for this instance.
		PB_ASSERT_MSG(requiredExtensions != nullptr || extCount == 0, "Valid extension name array not provided.");
		for (uint32_t i = 0; i < extCount; ++i)
			m_instanceExtensionManager.EnableExtension(requiredExtensions[i]);

		EnableExtensions();
		EnableLayers();

		m_instanceExtensionManager.PrintEnabledExtensions();
		m_instanceExtensionManager.PrintEnabledLayers();

		CLib::Vector<const char*> enabledExtensions = m_instanceExtensionManager.GetEnabledExtensions();
		cInfo.enabledExtensionCount = enabledExtensions.Count();
		cInfo.ppEnabledExtensionNames = enabledExtensions.Data();

		CLib::Vector<const char*> enabledLayers = m_instanceExtensionManager.GetEnabledLayers();
		cInfo.enabledLayerCount = enabledLayers.Count();
		cInfo.ppEnabledLayerNames = enabledLayers.Data();

		PB_ERROR_CHECK(vkCreateInstance(&cInfo, nullptr, &m_instance));
		PB_BREAK_ON_ERROR;

		CreateDebugMessenger();
		CommandContext::GetExtensionFunctions(m_instance);
	}

	void VulkanInstance::Destroy()
	{
		if(m_instance != VK_NULL_HANDLE) 
		{
			DestroyDebugMessenger();
			vkDestroyInstance(m_instance, nullptr);
			m_instance = VK_NULL_HANDLE;
		}
	}

	VkInstance VulkanInstance::GetHandle()
	{
		return m_instance;
	}

	void VulkanInstance::CreateDebugMessenger()
	{
#if PB_USE_DEBUG_MESSENGER
		VkDebugUtilsMessengerCreateInfoEXT messengerInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT, nullptr };
		messengerInfo.flags = 0;
		messengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | 
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		messengerInfo.pfnUserCallback = &DebugCallback;
		messengerInfo.pUserData = nullptr;

		auto createFunc = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
		PB_ASSERT(createFunc);
		PB_ASSERT(m_instance);

		createFunc(m_instance, &messengerInfo, nullptr, &m_messenger);
		PB_ASSERT(m_messenger);
#endif
	}

	void VulkanInstance::DestroyDebugMessenger()
	{
#if PB_USE_DEBUG_MESSENGER
		auto destroyFunc = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
		PB_ASSERT(destroyFunc);

		destroyFunc(m_instance, m_messenger, nullptr);
		m_messenger = VK_NULL_HANDLE;
#endif
	}
}
