#include "PBUtil.h"
#include "ParaBlitDebug.h"

namespace PB
{
	ETextureState ConvertImageLayouttoPBState(VkImageLayout layout)
	{
		switch (layout)
		{
        case VK_IMAGE_LAYOUT_UNDEFINED:
            return PB_TEXTURE_STATE_NONE;
        case VK_IMAGE_LAYOUT_GENERAL:
            return PB_TEXTURE_STATE_RAW;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return PB_TEXTURE_STATE_COLORTARGET;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return PB_TEXTURE_STATE_DEPTHTARGET;
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

    VkImageLayout ConvertPBStateToImageLayout(ETextureState state)
    {
        switch (state)
        {
        case PB::PB_TEXTURE_STATE_NONE:
            return VK_IMAGE_LAYOUT_UNDEFINED;
        case PB::PB_TEXTURE_STATE_RAW:
            return VK_IMAGE_LAYOUT_GENERAL;
        case PB::PB_TEXTURE_STATE_COLORTARGET:
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
            return VK_IMAGE_LAYOUT_UNDEFINED;
            break;
        }
    }

	VkImageUsageFlags ConvertPBAvailableStatesToUsageFlags(ETextureStateFlags availableStates)
	{
        VkImageUsageFlags vkFlags = 0;
        if (availableStates & PB_TEXTURE_STATE_RAW)
            PB_NOT_IMPLEMENTED;
        if (availableStates & PB_TEXTURE_STATE_COLORTARGET)
            vkFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (availableStates & PB_TEXTURE_STATE_DEPTHTARGET)
            vkFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if (availableStates & PB_TEXTURE_STATE_SAMPLED)
            vkFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        if (availableStates & PB_TEXTURE_STATE_COPY_SRC)
            vkFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if (availableStates & PB_TEXTURE_STATE_COPY_DST)
            vkFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        return vkFlags;
	}

    VkPipelineStageFlags GetSrcStatePipelineFlags(ETextureState srcState)
    {
        switch (srcState)
        {
        case PB::PB_TEXTURE_STATE_NONE:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            break;
        case PB::PB_TEXTURE_STATE_SAMPLED:
            PB_NOT_IMPLEMENTED;
            break;
        case PB::PB_TEXTURE_STATE_COLORTARGET:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
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
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case PB::PB_TEXTURE_STATE_PRESENT:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            break;
        default:
            PB_NOT_IMPLEMENTED;
            break;
        }
        return PB_TEXTURE_STATE_NONE;
    }

    VkPipelineStageFlags GetDstStatePipelineFlags(ETextureState dstState)
    {
        switch (dstState)
        {
        case PB::PB_TEXTURE_STATE_NONE:
            PB_NOT_IMPLEMENTED;
            break;
        case PB::PB_TEXTURE_STATE_SAMPLED:
            PB_NOT_IMPLEMENTED;
            break;
        case PB::PB_TEXTURE_STATE_COLORTARGET:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
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
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case PB::PB_TEXTURE_STATE_PRESENT:
            return VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
            break;
        default:
            PB_NOT_IMPLEMENTED;
            break;
        }
        return 0;
    }

    VkAccessFlags GetSrcAccessFlags(ETextureState srcState)
    {
        switch (srcState)
        {
        case PB::PB_TEXTURE_STATE_NONE:
            return static_cast<VkAccessFlagBits>(0);
            break;
        case PB::PB_TEXTURE_STATE_SAMPLED:
            PB_NOT_IMPLEMENTED;
            break;
        case PB::PB_TEXTURE_STATE_COLORTARGET:
            return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;
        case PB::PB_TEXTURE_STATE_DEPTHTARGET:
            PB_NOT_IMPLEMENTED;
            break;
        case PB::PB_TEXTURE_STATE_RAW:
            PB_NOT_IMPLEMENTED;
            break;
        case PB::PB_TEXTURE_STATE_COPY_SRC:
            return VK_ACCESS_TRANSFER_READ_BIT;
            break;
        case PB::PB_TEXTURE_STATE_COPY_DST:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
            break;
        case PB::PB_TEXTURE_STATE_PRESENT:
            return VK_ACCESS_MEMORY_READ_BIT;
            break;
        default:
            PB_NOT_IMPLEMENTED;
            break;
        }

        return PB_TEXTURE_STATE_NONE;
    }

    VkAccessFlags GetDstAccessFlags(ETextureState dstState)
    {
        switch (dstState)
        {
        case PB::PB_TEXTURE_STATE_NONE:
            return static_cast<VkAccessFlagBits>(0);
            break;
        case PB::PB_TEXTURE_STATE_SAMPLED:
            PB_NOT_IMPLEMENTED;
            break;
        case PB::PB_TEXTURE_STATE_COLORTARGET:
            return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;
        case PB::PB_TEXTURE_STATE_DEPTHTARGET:
            PB_NOT_IMPLEMENTED;
            break;
        case PB::PB_TEXTURE_STATE_RAW:
            PB_NOT_IMPLEMENTED;
            break;
        case PB::PB_TEXTURE_STATE_COPY_SRC:
            return VK_ACCESS_TRANSFER_READ_BIT;
            break;
        case PB::PB_TEXTURE_STATE_COPY_DST:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
            break;
        case PB::PB_TEXTURE_STATE_PRESENT:
            return VK_ACCESS_MEMORY_READ_BIT;
            break;
        default:
            PB_NOT_IMPLEMENTED;
            break;
        }
        return 0;
    }

	VkFormat ConvertPBFormatToVkFormat(ETextureFormat format)
	{
        switch (format)
        {
        case PB_TEXTURE_FORMAT_UNKNOWN:
            PB_NOT_IMPLEMENTED;
            break;
        case PB_TEXTURE_FORMAT_R8_UNORM:
            return VK_FORMAT_R8_UNORM;
            break;
        case PB_TEXTURE_FORMAT_R8G8_UNORM:
            return VK_FORMAT_R8G8_UNORM;
            break;
        case PB_TEXTURE_FORMAT_R8G8B8_UNORM:
            return VK_FORMAT_R8G8B8_UNORM;
            break;
        case PB_TEXTURE_FORMAT_R8G8B8A8_UNORM:
            return VK_FORMAT_R8G8B8A8_UNORM;
            break;
        case PB_TEXTURE_FORMAT_B8G8R8A8_UNORM:
            return VK_FORMAT_B8G8R8A8_UNORM;
            break;
        default:
            PB_NOT_IMPLEMENTED;
            break;
        }
        return VK_FORMAT_UNDEFINED;
	}

	VkPipelineStageFlags ConvertPBAttachmentUsageToStageFlags(EAttachmentUsage usage)
	{
        switch (usage)
        {
        case PB_ATTACHMENT_USAGE_NONE:
            return 0;
        case PB_ATTACHMENT_USAGE_COLOR:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case PB_ATTACHMENT_USAGE_DEPTHSTENCIL:
            return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        case PB_ATTACHMENT_USAGE_READ:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        default:
            return 0;
        }
	}

	VkShaderStageFlagBits ConvertPBShaderStageToVK(EShaderStage stage)
	{
        switch (stage)
        {
        case PB_SHADER_STAGE_VERTEX:
            return VK_SHADER_STAGE_VERTEX_BIT;
            break;
        case PB_SHADER_STAGE_FRAGMENT:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
            break;
        case PB_SHADER_STAGE_COUNT:
            PB_NOT_IMPLEMENTED;
            break;
        default:
            break;
        }

        return VK_SHADER_STAGE_VERTEX_BIT;
	}

	void MakeInternalContext(CommandContext& context, Renderer* renderer)
	{
        CommandContextDesc desc;
        desc.m_flags = PB_COMMAND_CONTEXT_PRIORITY;
        desc.m_usage = PB_COMMAND_CONTEXT_USAGE_GRAPHICS;
        desc.m_renderer = reinterpret_cast<IRenderer*>(renderer);

        // Initialize and flag as internal.
        context.Init(desc);
        context.SetIsInternal();
	}
}