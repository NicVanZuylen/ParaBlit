#pragma once
#include "ParaBlitInterface.h"
#include "ParaBlitDefs.h"

namespace PB 
{
	class ITexture;

	enum EPresentMode : u16
	{
		PB_PRESENT_MODE_IMMEDIATE,
		PB_PRESENT_MODE_MAILBOX,
		PB_PRESENT_MODE_FIFO,
		PB_PRESENT_MODE_FIFO_RELAXED,
		PB_PRESENT_MODE_END_RANGE,
	};

	struct SwapChainDesc
	{
		u32 m_width = 0;                                      // Leave as zero to use the surface dimension.
		u32 m_height = 0;                                     // Leave as zero to use the surface dimension.
		EPresentMode m_presentMode = PB_PRESENT_MODE_FIFO;
		u8 m_imageCount = 3;
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
		Description: Get the width of the swapchain buffers.
		Return Type: u32
		*/
		PARABLIT_INTERFACE u32 GetWidth() = 0;

		/*
		Description: Get the height of the swapchain buffers.
		Return Type: u32
		*/
		PARABLIT_INTERFACE u32 GetHeight() = 0;

		/*
		Description: Get swapchain image count.
		Return Type: u32
		*/
		PARABLIT_INTERFACE u32 GetImageCount() = 0;

		/*
		Description: Get the format of the swapchain images.
		Return Type: ETextureFormat
		*/
		PARABLIT_INTERFACE ETextureFormat GetImageFormat() = 0;
	};
}