#include "PBUtil.h"
#include "ParaBlitDebug.h"

namespace PB
{
	ETextureState ConvertImageLayouttoPBState(VkImageLayout layout)
	{
		switch (layout)
		{
        case VK_IMAGE_LAYOUT_UNDEFINED:
            return ETextureState::NONE;
        case VK_IMAGE_LAYOUT_GENERAL:
            return ETextureState::STORAGE;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return ETextureState::COLORTARGET;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return ETextureState::DEPTHTARGET;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return ETextureState::SAMPLED;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return ETextureState::COPY_SRC;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return ETextureState::COPY_DST;
        case VK_IMAGE_LAYOUT_PREINITIALIZED:
        case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
            PB_NOT_IMPLEMENTED;
            return ETextureState::NONE;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR:
            return ETextureState::PRESENT;
        case VK_IMAGE_LAYOUT_SHADING_RATE_OPTIMAL_NV:
        case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
            PB_NOT_IMPLEMENTED;
            return ETextureState::NONE;
        default:
            PB_NOT_IMPLEMENTED;
            return ETextureState::NONE;
		}
	}

    VkImageLayout ConvertPBStateToImageLayout(ETextureState state)
    {
        switch (state)
        {
        case ETextureState::NONE:
            return VK_IMAGE_LAYOUT_UNDEFINED;
        case ETextureState::STORAGE:
            return VK_IMAGE_LAYOUT_GENERAL;
        case ETextureState::COLORTARGET:
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case ETextureState::DEPTHTARGET:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case ETextureState::READ_ONLY_DEPTH_STENCIL:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        case ETextureState::SAMPLED:
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case ETextureState::COPY_SRC:
            return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case ETextureState::COPY_DST:
            return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case ETextureState::READBACK:
            return VK_IMAGE_LAYOUT_GENERAL;
        case ETextureState::PRESENT:
            return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        default:
            PB_NOT_IMPLEMENTED;
            return VK_IMAGE_LAYOUT_UNDEFINED;
            break;
        }
    }

	VkImageUsageFlags ConvertPBAvailableStatesToUsageFlags(TextureStateFlags availableStates)
	{
        VkImageUsageFlags vkFlags = 0;
        if (availableStates & ETextureState::STORAGE)
            vkFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
        if (availableStates & ETextureState::COLORTARGET)
            vkFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (availableStates & ETextureState::DEPTHTARGET)
            vkFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if (availableStates & ETextureState::READ_ONLY_DEPTH_STENCIL)
            vkFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if (availableStates & ETextureState::SAMPLED)
            vkFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        if (availableStates & ETextureState::COPY_SRC)
            vkFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if (availableStates & ETextureState::COPY_DST)
            vkFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (availableStates & ETextureState::READBACK)
            vkFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        return vkFlags;
	}

	VkBufferUsageFlags ConvertPBBufferUsageToVkBufferUsage(BufferUsageFlags usage)
	{
        VkBufferUsageFlags returnFlags = 0;
        if (usage & EBufferUsage::UNIFORM)
            returnFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if (usage & EBufferUsage::STORAGE)
            returnFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if (usage & EBufferUsage::COPY_SRC)
            returnFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        if (usage & EBufferUsage::COPY_DST)
            returnFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if (usage & EBufferUsage::VERTEX)
            returnFlags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if (usage & EBufferUsage::INDEX)
            returnFlags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (usage & EBufferUsage::INDIRECT_PARAMS)
            returnFlags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        if (usage & EBufferUsage::ACCELERATION_STRUCTURE_STORAGE)
            returnFlags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
		if (usage & EBufferUsage::MEMORY_ADDRESS_ACCESS)
			returnFlags |= (VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
		if (usage & EBufferUsage::SHADER_BINDING_TABLE)
			returnFlags |= (VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR);
        return returnFlags;
	}

    VkPipelineStageFlags GetSrcStatePipelineFlags(ETextureState srcState)
    {
        switch (srcState)
        {
        case ETextureState::NONE:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            break;
        case ETextureState::SAMPLED:
            return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            break;
        case ETextureState::COLORTARGET:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        case ETextureState::DEPTHTARGET:
        case ETextureState::READ_ONLY_DEPTH_STENCIL:
            return VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            break;
        case ETextureState::STORAGE:
            return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;
        case ETextureState::COPY_SRC:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case ETextureState::COPY_DST:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case ETextureState::READBACK:
            return VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT;
            break;
        case ETextureState::PRESENT:
            return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            break;
        default:
            PB_NOT_IMPLEMENTED;
            break;
        }
        return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }

    VkPipelineStageFlags GetDstStatePipelineFlags(ETextureState dstState)
    {
        switch (dstState)
        {
        case ETextureState::NONE:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            break;
        case ETextureState::SAMPLED:
            return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            break;
        case ETextureState::COLORTARGET:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        case ETextureState::DEPTHTARGET:
        case ETextureState::READ_ONLY_DEPTH_STENCIL:
            return VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            break;
        case ETextureState::STORAGE:
            return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;
        case ETextureState::COPY_SRC:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case ETextureState::COPY_DST:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case ETextureState::READBACK:
            return VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT;
            break;
        case ETextureState::PRESENT:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
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
        case ETextureState::NONE:
            return static_cast<VkAccessFlagBits>(0);
            break;
        case ETextureState::SAMPLED:
            return VK_ACCESS_SHADER_READ_BIT;
            break;
        case ETextureState::COLORTARGET:
            return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;
        case ETextureState::DEPTHTARGET:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;
        case ETextureState::READ_ONLY_DEPTH_STENCIL:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            break;
        case ETextureState::STORAGE:
            return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            break;
        case ETextureState::COPY_SRC:
            return VK_ACCESS_TRANSFER_READ_BIT;
            break;
        case ETextureState::COPY_DST:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
            break;
        case ETextureState::READBACK:
            return VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT;
            break;
        case ETextureState::PRESENT:
            return VK_ACCESS_MEMORY_READ_BIT;
            break;
        default:
            PB_NOT_IMPLEMENTED;
            break;
        }

        return 0;
    }

    VkAccessFlags GetDstAccessFlags(ETextureState dstState)
    {
        switch (dstState)
        {
        case ETextureState::NONE:
            return static_cast<VkAccessFlagBits>(0);
            break;
        case ETextureState::SAMPLED:
            return VK_ACCESS_SHADER_READ_BIT;
            break;
        case ETextureState::COLORTARGET:
            return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;
        case ETextureState::DEPTHTARGET:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;
        case ETextureState::READ_ONLY_DEPTH_STENCIL:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            break;
        case ETextureState::STORAGE:
            return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            break;
        case ETextureState::COPY_SRC:
            return VK_ACCESS_TRANSFER_READ_BIT;
            break;
        case ETextureState::COPY_DST:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
            break;
        case ETextureState::READBACK:
            return VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT;
            break;
        case ETextureState::PRESENT:
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
        case ETextureFormat::UNKNOWN:
            PB_NOT_IMPLEMENTED;
            break;
        case ETextureFormat::R8_UNORM:
            return VK_FORMAT_R8_UNORM;
        case ETextureFormat::R8G8_UNORM:
            return VK_FORMAT_R8G8_UNORM;
        case ETextureFormat::R8G8B8_UNORM:
            return VK_FORMAT_R8G8B8_UNORM;
        case ETextureFormat::R8G8B8A8_UNORM:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case ETextureFormat::B8G8R8A8_UNORM:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case ETextureFormat::A2R10G10B10_UNORM:
            return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
        case ETextureFormat::R8_SRGB:
            return VK_FORMAT_R8_SRGB;
        case ETextureFormat::R8G8_SRGB:
            return VK_FORMAT_R8G8_SRGB;
        case ETextureFormat::R8G8B8_SRGB:
            return VK_FORMAT_R8G8B8_SRGB;
        case ETextureFormat::R8G8B8A8_SRGB:
            return VK_FORMAT_R8G8B8A8_SRGB;
        case ETextureFormat::B8G8R8A8_SRGB:
            return VK_FORMAT_B8G8R8A8_SRGB;
        case ETextureFormat::R16_FLOAT:
            return VK_FORMAT_R16_SFLOAT;
        case ETextureFormat::R16G16_FLOAT:
            return VK_FORMAT_R16G16_SFLOAT;
        case ETextureFormat::R16G16B16_FLOAT:
            return VK_FORMAT_R16G16B16_SFLOAT;
        case ETextureFormat::R16G16B16A16_FLOAT:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case ETextureFormat::R32_FLOAT:
            return VK_FORMAT_R32_SFLOAT;
        case ETextureFormat::R32G32_FLOAT:
            return VK_FORMAT_R32G32_SFLOAT;
        case ETextureFormat::R32G32B32_FLOAT:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case ETextureFormat::R32G32B32A32_FLOAT:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case ETextureFormat::D16_UNORM:
            return VK_FORMAT_D16_UNORM;
        case ETextureFormat::D16_UNORM_S8_UINT:
            return VK_FORMAT_D16_UNORM_S8_UINT;
        case ETextureFormat::D24_UNORM_S8_UINT:
            return VK_FORMAT_D24_UNORM_S8_UINT;
        case ETextureFormat::D32_FLOAT:
            return VK_FORMAT_D32_SFLOAT;
        case ETextureFormat::D32_FLOAT_S8_UINT:
            return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case ETextureFormat::BC3_SRGB:
            return VK_FORMAT_BC3_SRGB_BLOCK;
        case ETextureFormat::BC5_UNORM:
            return VK_FORMAT_BC5_UNORM_BLOCK;
        case ETextureFormat::BC6H_RGB_U16F:
            return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case ETextureFormat::BC6H_RGB_S16F:
            return VK_FORMAT_BC6H_SFLOAT_BLOCK;
        case ETextureFormat::BC7_UNORM:
            return VK_FORMAT_BC7_UNORM_BLOCK;
        case ETextureFormat::BC7_SRGB:
            return VK_FORMAT_BC7_SRGB_BLOCK;
        default:
            PB_NOT_IMPLEMENTED;
            break;
        }
        return VK_FORMAT_UNDEFINED;
	}

    bool IsFormatBlockCompressed(ETextureFormat format)
    {
        return u16(format) >= BlockCompressedStart && u16(format) <= BlockCompressedEnd;
    }

    ETextureFormat ConvertVkFormatToPBFormat(VkFormat format)
    {
        switch (format)
        {
        case VK_FORMAT_UNDEFINED:
            PB_NOT_IMPLEMENTED;
            break;
        case VK_FORMAT_R8_UNORM:
            return ETextureFormat::R8_UNORM;
        case VK_FORMAT_R8G8_UNORM:
            return ETextureFormat::R8G8_UNORM;
        case VK_FORMAT_R8G8B8_UNORM:
            return ETextureFormat::R8G8B8_UNORM;
        case VK_FORMAT_R8G8B8A8_UNORM:
            return ETextureFormat::R8G8B8A8_UNORM;
        case VK_FORMAT_B8G8R8A8_UNORM:
            return ETextureFormat::B8G8R8A8_UNORM;
        case VK_FORMAT_B8G8R8A8_SRGB:
            return ETextureFormat::B8G8R8A8_SRGB;
        case VK_FORMAT_R32_SFLOAT:
            return ETextureFormat::R32_FLOAT;
        case VK_FORMAT_R32G32_SFLOAT:
            return ETextureFormat::R32G32_FLOAT;
        case VK_FORMAT_R32G32B32_SFLOAT:
            return ETextureFormat::R32G32B32_FLOAT;
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return ETextureFormat::R32G32B32A32_FLOAT;
        case VK_FORMAT_D16_UNORM:
            return ETextureFormat::D16_UNORM;
        case VK_FORMAT_D16_UNORM_S8_UINT:
            return ETextureFormat::D16_UNORM_S8_UINT;
        case VK_FORMAT_D24_UNORM_S8_UINT:
            return ETextureFormat::D24_UNORM_S8_UINT;
        case VK_FORMAT_D32_SFLOAT:
            return ETextureFormat::D32_FLOAT;
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return ETextureFormat::D32_FLOAT_S8_UINT;
        default:
            PB_NOT_IMPLEMENTED;
            break;
        }
        return ETextureFormat::UNKNOWN;
    }

    VkImageType PBImageDimensionToVKImageType(ETextureDimension dimension)
    {
        switch (dimension)
        {
        case PB::ETextureDimension::DIMENSION_NONE:
        {
            PB_NOT_IMPLEMENTED;
            return VK_IMAGE_TYPE_MAX_ENUM;
        }
        case PB::ETextureDimension::DIMENSION_1D:
            return VK_IMAGE_TYPE_1D;
        case PB::ETextureDimension::DIMENSION_2D:
            return VK_IMAGE_TYPE_2D;
        case PB::ETextureDimension::DIMENSION_3D:
        {
            PB_NOT_IMPLEMENTED;
            return VK_IMAGE_TYPE_3D;
        }
        case PB::ETextureDimension::DIMENSION_CUBE:
            return VK_IMAGE_TYPE_2D;
        default:
            PB_NOT_IMPLEMENTED;
            return VK_IMAGE_TYPE_MAX_ENUM;
        }
    }

    VkImageViewType PBTextureViewTypeToVkImageViewType(ETextureViewType type)
    {
        switch (type)
        {
        case PB::ETextureViewType::VIEW_TYPE_1D:
            PB_NOT_IMPLEMENTED;
        case PB::ETextureViewType::VIEW_TYPE_2D:
            return VK_IMAGE_VIEW_TYPE_2D;
            break;
        case PB::ETextureViewType::VIEW_TYPE_2D_ARRAY:
            return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            break;
        case PB::ETextureViewType::VIEW_TYPE_3D:
            PB_NOT_IMPLEMENTED;
            break;
        case PB::ETextureViewType::VIEW_TYPE_CUBE:
            return VK_IMAGE_VIEW_TYPE_CUBE;
        default:
            PB_NOT_IMPLEMENTED;
            break;
        }
        return VK_IMAGE_VIEW_TYPE_2D;
    }

	VkPipelineStageFlags ConvertPBAttachmentUsageToStageFlags(AttachmentUsageFlags usage)
	{
        switch ((EAttachmentUsage)usage)
        {
        case EAttachmentUsage::NONE:
            return 0;
        case EAttachmentUsage::COLOR:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case EAttachmentUsage::DEPTHSTENCIL:
            return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        case EAttachmentUsage::READ:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        default:
            return 0;
        }
	}

	VkShaderStageFlagBits ConvertPBShaderStageToVK(EGraphicsShaderStage stage)
	{
        switch (stage)
        {
        case EGraphicsShaderStage::VERTEX:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case EGraphicsShaderStage::TASK:
            return VK_SHADER_STAGE_TASK_BIT_EXT;
        case EGraphicsShaderStage::MESH:
            return VK_SHADER_STAGE_MESH_BIT_EXT;
        case EGraphicsShaderStage::FRAGMENT:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case EGraphicsShaderStage::GRAPHICS_STAGE_COUNT:
            PB_NOT_IMPLEMENTED;
            break;
        default:
            break;
        }

        return VK_SHADER_STAGE_VERTEX_BIT;
	}

	PARABLIT_API VkShaderStageFlagBits ConvertPBRayTracingShaderStageToVK(ERayTracingShaderStage stage)
	{
		switch (stage)
		{
		case ERayTracingShaderStage::RAY_GEN:
			return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		case ERayTracingShaderStage::MISS:
			return VK_SHADER_STAGE_MISS_BIT_KHR;
		case ERayTracingShaderStage::CLOSEST_HIT:
			return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		case ERayTracingShaderStage::ANY_HIT:
			return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
		case ERayTracingShaderStage::INTERSECTION:
			return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
		case ERayTracingShaderStage::RAYTRACING_STAGE_COUNT:
			PB_NOT_IMPLEMENTED;
			break;
		default:
			break;
		}

		return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	}

    VkFormat PBVertexTypeToVkFormat(EVertexAttributeType type)
    {
        switch (type)
        {
        case EVertexAttributeType::NONE:
            PB_ASSERT_MSG(false, "Invalid vertex attribute type provided.");
            break;
        case EVertexAttributeType::FLOAT:
            return VK_FORMAT_R32_SFLOAT;
        case EVertexAttributeType::FLOAT2:
            return VK_FORMAT_R32G32_SFLOAT;
        case EVertexAttributeType::FLOAT3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case EVertexAttributeType::FLOAT4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case EVertexAttributeType::MAT4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        default:
            PB_ASSERT_MSG(false, "Invalid vertex attribute type provided.");
            break;
        }
        return VK_FORMAT_R32_SFLOAT;
    }

    VkCompareOp PBCompareOPtoVKCompareOP(ECompareOP op)
    {
        switch (op)
        {
        case ECompareOP::ALWAYS:
            return VK_COMPARE_OP_ALWAYS;
        case ECompareOP::NEVER:
            return VK_COMPARE_OP_NEVER;
        case ECompareOP::LEQUAL:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case ECompareOP::LESS:
            return VK_COMPARE_OP_LESS;
        case ECompareOP::EQUAL:
            return VK_COMPARE_OP_EQUAL;
        case ECompareOP::GREQUAL:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case ECompareOP::GREATER:
            return VK_COMPARE_OP_GREATER;
        default:
            break;
        }
        return VK_COMPARE_OP_ALWAYS;
    }

	void MakeInternalContext(CommandContext& context, Renderer* renderer)
	{
        CommandContextDesc desc;
        desc.m_flags = ECommandContextFlags::PRIORITY;
        desc.m_usage = ECommandContextUsage::GRAPHICS;
        desc.m_renderer = reinterpret_cast<IRenderer*>(renderer);

        // Initialize and flag as internal.
        context.Init(desc);
        context.SetIsInternal();
	}

    void GetMemoryBarrierOfType(EMemoryBarrierType type, VkMemoryBarrier& outBarrier, VkPipelineStageFlags& srcStage, VkPipelineStageFlags& dstStage)
    {
		outBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		outBarrier.pNext = nullptr;

		switch (type)
		{
		    case GRAPHICS_ATTACHMENT_WRITE_TO_FRAGMENT_SHADER_READ:
		    {
		    	srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		    	dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		    	outBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		    	outBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		    	return;
		    }
		    default:
            {
                PB_NOT_IMPLEMENTED;
                break;
            }
		}
    }

    void GetImageMemoryBarrierOfType(EMemoryBarrierType type, VkImageMemoryBarrier& outBarrier, VkPipelineStageFlags& srcStage, VkPipelineStageFlags& dstStage, Texture* image, const SubresourceRange& subresources)
    {
		outBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		outBarrier.pNext = nullptr;

        outBarrier.image = image->GetImage();
        outBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        outBarrier.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        outBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        outBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		outBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		if (image->HasDepthPlane())
		{
			outBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		if (image->HasStencilPlane())
		{
			outBarrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}

        outBarrier.subresourceRange.baseArrayLayer = subresources.m_firstArrayElement;
        outBarrier.subresourceRange.baseMipLevel = subresources.m_baseMip;
        outBarrier.subresourceRange.layerCount = subresources.m_arrayCount;
        outBarrier.subresourceRange.levelCount = subresources.m_mipCount;

		switch (type)
		{
		    case GRAPHICS_ATTACHMENT_WRITE_TO_FRAGMENT_SHADER_READ:
		    {
		    	srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		    	dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		    	outBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		    	outBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		    	return;
		    }
		    default:
            {
                PB_NOT_IMPLEMENTED;
		    	break;
            }
		}
    }

	void GetBufferMemoryBarrierOfType(EMemoryBarrierType type, VkBuffer bufferHandle, u32 offset, u32 size, VkBufferMemoryBarrier& outBarrier, VkPipelineStageFlags& srcStage, VkPipelineStageFlags& dstStage)
	{
		outBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		outBarrier.pNext = nullptr;
		outBarrier.buffer = bufferHandle;
		outBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		outBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		outBarrier.offset = offset;
		outBarrier.size = (size == ~u32(0)) ? VK_WHOLE_SIZE : size;

		switch (type)
		{
			case COMPUTE_SHADER_WRITE_TO_COMPUTE_SHADER_READ:
			{
                srcStage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                dstStage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
				outBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				outBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				return;
			}
            case COMPUTE_SHADER_WRITE_TO_GRAPHICS_GEOMETRY_READ:
            {
                srcStage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                dstStage |= VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
				outBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				outBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                return;
            }
            case COMPUTE_SHADER_WRITE_TO_INDIRECT_PARAMS_READ:
			{
                srcStage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                dstStage |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
				outBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				outBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
				return;
			}
            case COMPUTE_SHADER_WRITE_TO_ACCELERATION_STRUCTURE_BUILD:
            {
                srcStage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                dstStage |= VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
				outBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				outBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
				return;
            }
            case GRAPHICS_GEOMETRY_READ_TO_COMPUTE_SHADER_WRITE:
            {
                srcStage |= VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
                dstStage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
				outBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
				outBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
				return;
            }
            case GRAPHICS_FRAGMENT_READ_TO_COMPUTE_SHADER_WRITE:
            {
                srcStage |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                dstStage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
				outBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
				outBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
				return;
            }
            case INDIRECT_PARAMS_READ_TO_COMPUTE_SHADER_WRITE:
            {
                srcStage |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
                dstStage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                outBarrier.srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
                outBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
                return;
            }
			default:
            {
                PB_NOT_IMPLEMENTED;
				break;
            }
		}
	}
}