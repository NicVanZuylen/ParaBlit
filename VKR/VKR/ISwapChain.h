#pragma once
#include "ParaBlitInterface.h"

namespace PB 
{
	enum EPresentMode : u16
	{
		VKR_PRESENT_MODE_IMMEDIATE,
		VKR_PRESENT_MODE_MAILBOX,
		VKR_PRESENT_MODE_FIFO,
		VKR_PRESENT_MODE_FIFO_RELAXED,
		VKR_PRESENT_MODE_END_RANGE,
	};

	struct SwapChainDesc
	{
		u32 m_width = 0;                                      // Leave as zero to use the surface dimension.
		u32 m_height = 0;                                     // Leave as zero to use the surface dimension.
		EPresentMode m_presentMode = VKR_PRESENT_MODE_FIFO;
		u16 m_imageCount = 3;
	};
}