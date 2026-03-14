#pragma once
#include "ParaBlitApi.h"
#include "Texture.h"
#include "CommandContext.h"

namespace PB
{
	PARABLIT_API ETextureState ConvertImageLayouttoPBState(VkImageLayout layout);

	PARABLIT_API VkImageLayout ConvertPBStateToImageLayout(ETextureState state);

	PARABLIT_API VkImageUsageFlags ConvertPBAvailableStatesToUsageFlags(TextureStateFlags availableStates);

	PARABLIT_API VkBufferUsageFlags ConvertPBBufferUsageToVkBufferUsage(BufferUsageFlags usage);

	PARABLIT_API VkPipelineStageFlags GetSrcStatePipelineFlags(ETextureState srcState);

	PARABLIT_API VkPipelineStageFlags GetDstStatePipelineFlags(ETextureState dstState);

	PARABLIT_API VkAccessFlags GetSrcAccessFlags(ETextureState srcState);

	PARABLIT_API VkAccessFlags GetDstAccessFlags(ETextureState dstState);

	PARABLIT_API VkFormat ConvertPBFormatToVkFormat(ETextureFormat format);

	PARABLIT_API bool IsFormatBlockCompressed(ETextureFormat format);

	PARABLIT_API ETextureFormat ConvertVkFormatToPBFormat(VkFormat format);

	PARABLIT_API VkImageType PBImageDimensionToVKImageType(ETextureDimension dimension);

	PARABLIT_API VkImageViewType PBTextureViewTypeToVkImageViewType(ETextureViewType type);

	PARABLIT_API VkPipelineStageFlags ConvertPBAttachmentUsageToStageFlags(AttachmentUsageFlags usage);

	PARABLIT_API VkShaderStageFlagBits ConvertPBShaderStageToVK(EGraphicsShaderStage stage);

	PARABLIT_API VkShaderStageFlagBits ConvertPBRayTracingShaderStageToVK(ERayTracingShaderStage stage);

	PARABLIT_API VkFormat PBVertexTypeToVkFormat(EVertexAttributeType type);

	PARABLIT_API VkCompareOp PBCompareOPtoVKCompareOP(ECompareOP op);

	PARABLIT_API void MakeInternalContext(CommandContext& context, Renderer* renderer);

	PARABLIT_API void GetMemoryBarrierOfType(EMemoryBarrierType type, VkMemoryBarrier& outBarrier, VkPipelineStageFlags& srcStage, VkPipelineStageFlags& dstStage);

	PARABLIT_API void GetImageMemoryBarrierOfType(EMemoryBarrierType type, VkImageMemoryBarrier& outBarrier, VkPipelineStageFlags& srcStage, VkPipelineStageFlags& dstStage, Texture* image, const SubresourceRange& subresources);

	PARABLIT_API void GetBufferMemoryBarrierOfType(EMemoryBarrierType type, VkBuffer bufferHandle, u32 offset, u32 size, VkBufferMemoryBarrier& outBarrier, VkPipelineStageFlags& srcStage, VkPipelineStageFlags& dstStage);
}