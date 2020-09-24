#pragma once
#include "ParaBlitApi.h"
#include "ParaBlitLog.h"
#include <iostream>
#include <cassert>

#pragma warning(push, 0)
#include "vulkan/vulkan.h"
#pragma warning(pop)

namespace PB
{
	extern VkResult errCheckRes;
};

#ifdef _DEBUG

#define PB_ERROR_CHECK(func) PB::errCheckRes = func; if(PB::errCheckRes) { printf("ParaBlit Vulkan Error: %i \n", PB::errCheckRes); }
#define PB_ASSERT(condition) assert(condition)
#define PB_ASSERT_MSG(condition, message) assert(condition && message);
#define PB_BREAK_ON_ERROR PB_ASSERT_MSG(PB::errCheckRes == VK_SUCCESS, "Vulkan Error Detected.")

#else

#define PB_ERROR_CHECK(func) func
#define PB_ASSERT(condition)
#define PB_ASSERT_MSG
#define PB_BREAK_ON_ERROR

#endif

#define PB_STATIC_ASSERT(condition, message) static_assert(condition && message);

