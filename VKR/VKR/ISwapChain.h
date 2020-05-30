#pragma once
#include "ParaBlitInterface.h"

namespace PB 
{
	class ITexture;

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

	class ISwapChain
	{
	public:

		/*
		Description: Expose a swapchain image texture object. Get the amount of images available using GetImageCount().
		Param:
			u32 imageindex
		Return Type: ITexture*
		*/
		PARABLIT_INTERFACE ITexture* GetImage(u32 imageIndex) = 0;

		/*
		Description: Get swapchain image count.
		Return Type: u32
		*/
		PARABLIT_INTERFACE u32 GetImageCount() = 0;
	};
}