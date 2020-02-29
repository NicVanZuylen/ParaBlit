#pragma once
#include "VKRApi.h"
#include <vulkan/vulkan.h>
#include "DynamicArray.h"

namespace VKR
{
	class PassModule
	{
	public:

		virtual VKR_API void RecordPass() = 0;

	protected:

		enum EPassModuleLevel 
		{
			PASS_MODULE_FULLPASS,
			PASS_MODULE_SUBPASS
		};

		enum EPassModuleType 
		{
			PASS_MODULE_TYPE_GENERIC, // Used to draw just about anything.
			PASS_MODULE_TYPE_OBJECT // Specialized towards drawing scenes full object objects e.g. (G-Buffer pass, Shadow map pass).
		};

		VkCommandBuffer m_cmdBuffer;
		EPassModuleLevel m_level;
		DynamicArray<PassModule*> m_subpassModules;
	};
};

