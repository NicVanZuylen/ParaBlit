#include "ExtensionManager.h"
#include "ParaBlitLog.h"
#include "ParaBlitDebug.h"

#define PB_PRINT_AVAILABLE_EXT 0
#define PB_PRINT_AVAILABLE_LAYERS 0

namespace PB
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

			PB_ASSERT(extensionCount > 0, "No extensions found!");
			CLib::Vector<VkExtensionProperties> extProps(extensionCount);
			extProps.SetCount(extProps.Capacity());
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
				CLib::Vector<VkLayerProperties> layerProps(layerCount);
				layerProps.SetCount(layerProps.Capacity());
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
#if PB_PRINT_AVAILABLE_EXT
		if (m_device)
		{
			PB_LOG("-------------------- AVAILABLE DEVICE EXTENSIONS --------------------");
		}
		else
		{
			PB_LOG("------------------- AVAILABLE INSTANCE EXTENSIONS -------------------");
		}

		for (auto& ext : m_availableExt)
		{
			PB_LOG(ext.first.c_str());
		}

		PB_LOG("---------------------------------------------------------------------");
#endif
	}

	void ExtensionManager::PrintEnabledExtensions()
	{
		if (m_device) 
		{
			PB_LOG("--------------------- ENABLED DEVICE EXTENSIONS ---------------------");
		}
		else  
		{
			PB_LOG("-------------------- ENABLED INSTANCE EXTENSIONS --------------------");
		}

		for (auto& ext : m_availableExt)
		{
			if (ext.second)
			{
				PB_LOG(ext.first.c_str());
			}
		}

		PB_LOG("---------------------------------------------------------------------");
	}

	void ExtensionManager::PrintAvailableLayers()
	{
#if PB_PRINT_AVAILABLE_LAYERS
		if (m_device)
		{
			PB_LOG("---------------------- AVAILABLE DEVICE LAYERS ----------------------");
		}
		else
		{
			PB_LOG("--------------------- AVAILABLE INSTANCE LAYERS ---------------------");
		}

		for (auto& layer : m_availableLayers)
		{
			PB_LOG(layer.first.c_str());
		}

		PB_LOG("---------------------------------------------------------------------");
#endif
	}

	void ExtensionManager::PrintEnabledLayers()
	{
		if (m_device)
		{
			PB_LOG("----------------------- ENABLED DEVICE LAYERS -----------------------");
		}
		else
		{
			PB_LOG("---------------------- ENABLED INSTANCE LAYERS ----------------------");
		}

		for (auto& layer : m_availableLayers)
		{
			if (layer.second)
			{
				PB_LOG(layer.first.c_str());
			}
		}

		PB_LOG("---------------------------------------------------------------------");
	}

	CLib::Vector<const char*> ExtensionManager::GetEnabledExtensions()
	{
		CLib::Vector<const char*> output(static_cast<uint32_t>(m_availableExt.size()));

		for (auto& ext : m_availableExt) 
		{
			// Only push enabled.
			if(ext.second)
				output.PushBack(ext.first.c_str());
		}

		return output;
	}

	CLib::Vector<const char*> ExtensionManager::GetEnabledLayers()
	{
		CLib::Vector<const char*> output(static_cast<uint32_t>(m_availableLayers.size()));

		for (auto& ext : m_availableLayers)
		{
			// Only push enabled.
			if(ext.second)
				output.PushBack(ext.first.c_str());
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
