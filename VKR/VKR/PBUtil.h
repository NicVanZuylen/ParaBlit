#pragma once
#include "ParaBlitApi.h"
#include "Texture.h"

namespace PB
{
	inline TextureState ConvertImageLayouttoPBState(VkImageLayout layout);

	inline VkImageLayout ConvertPBStateToImageLayout(TextureState state);

	inline VkImageMemoryBarrier CreateImageBarrier(VkPipelineStageFlags& srcStageFlags, VkPipelineStageFlags& dstStageFlags, VkImage image, TextureState oldState, TextureState newState, VkImageAspectFlags aspectMask, u32 firstMip = 0, u32 mipCount = 1, u32 firstArrayElement = 0, u32 arrayCount = 1);
}