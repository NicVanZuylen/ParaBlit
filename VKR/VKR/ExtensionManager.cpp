#include "ExtensionManager.h"
#include "VKRLog.h"
#include "VKRDebug.h"

namespace VKR
{
	ExtensionManager::ExtensionManager(VkPhysicalDevice device)
	{
		m_device = device;
	}

	ExtensionManager::~ExtensionManager()
	{

	}

	void ExtensionManager::Query()
	{
		{
			uint32_t extensionCount = 0;
			EnumerateExt(extensionCount, nullptr);

			VKR_ASSERT(extensionCount > 0, "No extensions found!");
			DynamicArray<VkExtensionProperties> extProps(extensionCount);
			extProps.SetCount(extProps.GetSize());
			EnumerateExt(extensionCount, extProps.Data());

			for(uint32_t i = 0; i < extProps.Count(); ++i) 
			{
				m_availableExt[extProps[i].extensionName] = false;
			}
		}

		{
			uint32_t layerCount = 0;
			EnumerateLayers(layerCount, nullptr);

			if(layerCount > 0) 
			{
				DynamicArray<VkLayerProperties> layerProps(layerCount);
				layerProps.SetCount(layerProps.GetSize());
				EnumerateLayers(layerCount, layerProps.Data());

				for (uint32_t i = 0; i < layerProps.Count(); ++i)
				{
					m_availableLayers[layerProps[i].layerName] = false;
				}
			}
		}
	}

	bool ExtensionManager::EnableExtension(const char* name)
	{
		if (ExtensionAvailable(name)) 
		{
			m_availableExt[name] = true;
			return true;
		}

		return false;
	}

	bool ExtensionManager::EnableExtension(const std::initializer_list<const char*>& names)
	{
		bool allEnabled = true;

		for (auto& name : names)
			allEnabled &= EnableExtension(name);

		return allEnabled;
	}

	bool ExtensionManager::EnableLayer(const char* name)
	{
		if (LayerAvailable(name)) 
		{
			m_availableLayers[name] = true;
			return true;
		}

		return false;
	}

	bool ExtensionManager::EnableLayer(const std::initializer_list<const char*>& names)
	{
		bool allEnabled = true;

		for (auto& name : names)
			allEnabled &= EnableLayer(name);

		return allEnabled;
	}

	bool ExtensionManager::ExtensionAvailable(const char* name)
	{
		return m_availableExt.find(name) != m_availableExt.end();
	}

	bool ExtensionManager::ExtensionEnabled(const char* name)
	{
		return m_availableExt[name];
	}

	bool ExtensionManager::LayerAvailable(const char* name)
	{
		return m_availableLayers.find(name) != m_availableLayers.end();
	}

	bool ExtensionManager::LayerEnabled(const char* name)
	{
		return m_availableLayers[name];
	}

	void ExtensionManager::PrintAvailableExtensions()
	{
		if (m_device)
		{
			VKR_LOG("-------------------- AVAILABLE DEVICE EXTENSIONS --------------------");
		}
		else
		{
			VKR_LOG("-------------------- AVAILABLE INSTANCE EXTENSIONS --------------------");
		}

		for (auto& ext : m_availableExt)
		{
			VKR_LOG(ext.first.c_str());
		}

		VKR_LOG("---------------------------------------------------------------------");
	}

	void ExtensionManager::PrintEnabledExtensions()
	{
		if (m_device) 
		{
			VKR_LOG("-------------------- ENABLED DEVICE EXTENSIONS --------------------");
		}
		else  
		{
			VKR_LOG("-------------------- ENABLED INSTANCE EXTENSIONS --------------------");
		}

		for (auto& ext : m_availableExt)
		{
			if (ext.second)
			{
				VKR_LOG(ext.first.c_str());
			}
		}

		VKR_LOG("---------------------------------------------------------------------");
	}

	void ExtensionManager::PrintAvailableLayers()
	{
		if (m_device)
		{
			VKR_LOG("-------------------- AVAILABLE DEVICE LAYERS --------------------");
		}
		else
		{
			VKR_LOG("-------------------- AVAILABLE INSTANCE LAYERS --------------------");
		}

		for (auto& layer : m_availableLayers)
		{
			if (layer.second)
			{
				VKR_LOG(layer.first.c_str());
			}
		}

		VKR_LOG("---------------------------------------------------------------------");
	}

	void ExtensionManager::PrintEnabledLayers()
	{
		if (m_device)
		{
			VKR_LOG("-------------------- ENABLED DEVICE LAYERS --------------------");
		}
		else
		{
			VKR_LOG("-------------------- ENABLED INSTANCE LAYERS --------------------");
		}

		for (auto& layer : m_availableLayers)
		{
			if (layer.second)
			{
				VKR_LOG(layer.first.c_str());
			}
		}

		VKR_LOG("---------------------------------------------------------------------");
	}

	DynamicArray<const char*> ExtensionManager::GetEnabledExtensions()
	{
		DynamicArray<const char*> output(m_availableExt.size());

		for (auto& ext : m_availableExt) 
		{
			// Only push enabled.
			if(ext.second)
				output.Push(ext.first.c_str());
		}

		return output;
	}

	DynamicArray<const char*> ExtensionManager::GetEnabledLayers()
	{
		DynamicArray<const char*> output(m_availableLayers.size());

		for (auto& ext : m_availableLayers)
		{
			// Only push enabled.
			if(ext.second)
				output.Push(ext.first.c_str());
		}

		return output;
	}

	void ExtensionManager::EnumerateExt(uint32_t& count, VkExtensionProperties* props)
	{
		// Enumerate device if a device was provided, otherwise enumerate instance.
		if (m_device)
			vkEnumerateDeviceExtensionProperties(m_device, nullptr, &count, props);
		else
			vkEnumerateInstanceExtensionProperties(nullptr, &count, props);
	}

	void ExtensionManager::EnumerateLayers(uint32_t& count, VkLayerProperties* props)
	{
		// Enumerate device if a device was provided, otherwise enumerate instance.
		if (m_device)
			vkEnumerateDeviceLayerProperties(m_device, &count, props);
		else
			vkEnumerateInstanceLayerProperties(&count, props);
	}
};
