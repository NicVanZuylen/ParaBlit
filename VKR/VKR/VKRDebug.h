#pragma once
#include "VKRLog.h"
#include "vulkan/vulkan.h"
#include <iostream>
#include <assert.h>

namespace VKR
{
	extern VkResult errCheckRes;
};

#define VKR_ERROR_CHECK(func, message) VKR::errCheckRes = func; if(VKR::errCheckRes) { printf("VKR Vulkan Error: %i \n", VKR::errCheckRes); }
#define VKR_ASSERT(condition, message) assert(condition, message)
#define VKR_BREAK_ON_ERROR VKR_ASSERT(VKR::errCheckRes == 0, "VKR Vulkan Error Detected")