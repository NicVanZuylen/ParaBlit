#pragma once
#include "ParaBlitApi.h"
#include "ParaBlitDefs.h"
#include "Texture.h"
#include "CommandContext.h"

namespace PB
{
	PARABLIT_API inline ETextureState ConvertImageLayouttoPBState(VkImageLayout layout);

	PARABLIT_API inline VkImageLayout ConvertPBStateToImageLayout(ETextureState state);

	PARABLIT_API inline VkImageUsageFlags ConvertPBAvailableStatesToUsageFlags(ETextureStateFlags availableStates);

	PARABLIT_API inline VkBufferUsageFlags ConvertPBBufferUsageToVkBufferUsage(BufferUsage usage);

	PARABLIT_API inline VkPipelineStageFlags GetSrcStatePipelineFlags(ETextureState srcState);

	PARABLIT_API inline VkPipelineStageFlags GetDstStatePipelineFlags(ETextureState dstState);

	PARABLIT_API inline VkAccessFlags GetSrcAccessFlags(ETextureState srcState);

	PARABLIT_API inline VkAccessFlags GetDstAccessFlags(ETextureState dstState);

	PARABLIT_API inline VkFormat ConvertPBFormatToVkFormat(ETextureFormat format);

	PARABLIT_API inline ETextureFormat ConvertVkFormatToPBFormat(VkFormat format);

	PARABLIT_API inline VkPipelineStageFlags ConvertPBAttachmentUsageToStageFlags(EAttachmentUsage usage);

	PARABLIT_API inline VkShaderStageFlagBits ConvertPBShaderStageToVK(EShaderStage stage);

	PARABLIT_API inline VkFormat PBVertexTypeToVkFormat(EVertexAttributeType type);

	PARABLIT_API inline VkCompareOp PBCompareOPtoVKCompareOP(ECompareOP op);

	PARABLIT_API inline void MakeInternalContext(CommandContext& context, Renderer* renderer);
}