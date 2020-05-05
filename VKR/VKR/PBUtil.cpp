#include "PBUtil.h"
#include "ParaBlitDebug.h"

namespace PB
{
	TextureState ConvertImageLayouttoPBState(VkImageLayout layout)
	{
		switch (layout)
		{
        case VK_IMAGE_LAYOUT_UNDEFINED:
            return PB_TEXTURE_STATE_NONE;
        case VK_IMAGE_LAYOUT_GENERAL:
            return PB_TEXTURE_STATE_RAW;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return PB_TEXTURE_STATE_RENDERTARGET;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return PB_TEXTURE_STATE_RENDERTARGET;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return PB_TEXTURE_STATE_SAMPLED;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return PB_TEXTURE_STATE_COPY_SRC;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return PB_TEXTURE_STATE_COPY_DST;
        case VK_IMAGE_LAYOUT_PREINITIALIZED:
        case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
            PB_NOT_IMPLEMENTED;
            return PB_TEXTURE_STATE_NONE;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR:
            return PB_TEXTURE_STATE_PRESENT;
        case VK_IMAGE_LAYOUT_SHADING_RATE_OPTIMAL_NV:
        case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
            PB_NOT_IMPLEMENTED;
            return PB_TEXTURE_STATE_NONE;
        default:
            PB_NOT_IMPLEMENTED;
            return PB_TEXTURE_STATE_NONE;
		}
	}

    VkImageLayout ConvertPBStateToImageLayout(TextureState state)
    {
        switch (state)
        {
        case PB::PB_TEXTURE_STATE_NONE:
            return VK_IMAGE_LAYOUT_UNDEFINED;
        case PB::PB_TEXTURE_STATE_RAW:
            return VK_IMAGE_LAYOUT_GENERAL;
        case PB::PB_TEXTURE_STATE_RENDERTARGET:
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case PB::PB_TEXTURE_STATE_DEPTHTARGET:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case PB::PB_TEXTURE_STATE_SAMPLED:
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case PB::PB_TEXTURE_STATE_COPY_SRC:
            return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case PB::PB_TEXTURE_STATE_COPY_DST:
            return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case PB::PB_TEXTURE_STATE_PRESENT:
            return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        default:
            PB_NOT_IMPLEMENTED;
            break;
        }
    }

    VkImageMemoryBarrier CreateImageBarrier(VkPipelineStageFlags& srcStageFlags, VkPipelineStageFlags& dstStageFlags, VkImage image, TextureState oldState, TextureState newState, VkImageAspectFlags aspectMask, u32 firstMip, u32 mipCount, u32 firstArrayElement, u32 arrayCount)
    {
        VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr };
        barrier.image = image;
        barrier.oldLayout = ConvertPBStateToImageLayout(oldState);
        barrier.newLayout = ConvertPBStateToImageLayout(newState);
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        // TODO: Implement transitions for multiple mips/array elements.
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = firstMip;
        barrier.subresourceRange.levelCount = mipCount;
        barrier.subresourceRange.baseArrayLayer = firstArrayElement;
        barrier.subresourceRange.layerCount = arrayCount;

        switch (oldState)
        {
        case PB::PB_TEXTURE_STATE_NONE:
            PB_NOT_IMPLEMENTED;
            break;
        case PB::PB_TEXTURE_STATE_SAMPLED:
            PB_NOT_IMPLEMENTED;
            break;
        case PB::PB_TEXTURE_STATE_RENDERTARGET:
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            srcStageFlags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        case PB::PB_TEXTURE_STATE_DEPTHTARGET:
            PB_NOT_IMPLEMENTED;
            break;
        case PB::PB_TEXTURE_STATE_RAW:
            PB_NOT_IMPLEMENTED;
            break;
        case PB::PB_TEXTURE_STATE_COPY_SRC:
            PB_NOT_IMPLEMENTED;
            break;
        case PB::PB_TEXTURE_STATE_COPY_DST:
            PB_NOT_IMPLEMENTED;
            break;
        case PB::PB_TEXTURE_STATE_PRESENT:
            PB_NOT_IMPLEMENTED;
            break;
        default:
            PB_NOT_IMPLEMENTED;
            break;
        }

        switch (newState)
        {
        case PB::PB_TEXTURE_STATE_NONE:
            PB_NOT_IMPLEMENTED;
            break;
        case PB::PB_TEXTURE_STATE_SAMPLED:
            PB_NOT_IMPLEMENTED;
            break;
        case PB::PB_TEXTURE_STATE_RENDERTARGET:
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dstStageFlags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        case PB::PB_TEXTURE_STATE_DEPTHTARGET:
            PB_NOT_IMPLEMENTED;
            break;
        case PB::PB_TEXTURE_STATE_RAW:
            PB_NOT_IMPLEMENTED;
            break;
        case PB::PB_TEXTURE_STATE_COPY_SRC:
            PB_NOT_IMPLEMENTED;
            break;
        case PB::PB_TEXTURE_STATE_COPY_DST:
            PB_NOT_IMPLEMENTED;
            break;
        case PB::PB_TEXTURE_STATE_PRESENT:
            PB_NOT_IMPLEMENTED;
            break;
        default:
            PB_NOT_IMPLEMENTED;
            break;
        }

        return barrier;
    }
}