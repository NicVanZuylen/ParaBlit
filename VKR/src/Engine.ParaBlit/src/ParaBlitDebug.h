#pragma once
#include "ParaBlitApi.h"
#include "ParaBlitLog.h"
#include <iostream>
#include <cassert>
#include <csignal>

#pragma warning(push, 0)
#include "vulkan/vulkan.h"
#pragma warning(pop)

// Dummy expression to allow termination of multi-line macros with a semi colon.
#define PB_TERMINATE_MACRO do {} while(false)

#if defined(_DEBUG) || !defined(NDEBUG)

#define PB_BREAK std::raise(SIGINT)

// Break if 'condition' is not satisfied.
#define PB_ASSERT(condition)															\
{																						\
	if(condition)																		\
	{																					\
	}																					\
	else																				\
	{																					\
		printf("PARABLIT ASSERTION FAILED. \n");										\
		PB_BREAK;																		\
	}																					\
}										 												\
PB_TERMINATE_MACRO																		\

// Break and print a message to the console if 'condition' is not satisfied.
#define PB_ASSERT_MSG(condition, message)												\
{																						\
	if(condition)																		\
	{																					\
	}																					\
	else																				\
	{																					\
		printf("PARABLIT ASSERTION FAILED: Line: %s\n", message);						\
		PB_BREAK;																		\
	}																					\
}																						\
PB_TERMINATE_MACRO																		\

namespace PB
{
	extern uint64_t errCheckRes; // Used for PB_ERROR_CHECK.
};

// Checks the result of an API call, and prints the error code if the result is not 0.
#define PB_ERROR_CHECK(func)															\
{																						\
	PB::errCheckRes = static_cast<uint64_t>(func);										\
	if (PB::errCheckRes != 0)															\
	{																					\
		printf("PARABLIT: API Error: %llu \n", (unsigned long long)PB::errCheckRes);	\
	}																					\
}																						\
PB_TERMINATE_MACRO																		\

// Triggers an assertion failure if the most recent PB_ERROR_CHECK returned an error code.
#define PB_BREAK_ON_ERROR						\
{												\
	PB_ASSERT(PB::errCheckRes == 0);			\
	PB::errCheckRes = 0;						\
}												\
PB_TERMINATE_MACRO								\

#else

#define PB_ERROR_CHECK(func) func
#define PB_ASSERT(condition)
#define PB_ASSERT_MSG
#define PB_BREAK_ON_ERROR

#endif

#define PB_STATIC_ASSERT(condition, message) static_assert(condition && message);

