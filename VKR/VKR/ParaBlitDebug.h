#pragma once
#include "ParaBlitApi.h"
#include "ParaBlitLog.h"
#include "vulkan/vulkan.h"
#include <iostream>
#include <assert.h>

namespace PB
{
	extern VkResult errCheckRes;
};

#define PB_ERROR_CHECK(func, message) PB::errCheckRes = func; if(PB::errCheckRes) { printf("ParaBlit Vulkan Error: %i \n", PB::errCheckRes); }
#define PB_ASSERT(condition, message) assert(condition, message)
#define PB_BREAK_ON_ERROR PB_ASSERT(PB::errCheckRes == 0, "ParaBlit Vulkan Error Detected")