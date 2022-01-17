#pragma once
#include "ParaBlitApi.h"
#include "ParaBlitInterface.h"
#include "ParaBlitDefs.h"

namespace PB
{
	struct VertexAttibute
	{
		u16 m_buffer = 0;
		EVertexAttributeType m_attribute = EVertexAttributeType::NONE;
	};

	struct VertexAttributeDesc
	{
		static constexpr const u32 MaxVertexAttributes = 8;
		VertexAttibute vertexAttributes[MaxVertexAttributes] = {};
	};

	enum class EVertexBufferType : u8
	{
		NONE,
		VERTEX,
		INSTANCE
	};

	struct PipelineVertexBufferDesc
	{
		u8 m_vertexSize = 0;
		EVertexBufferType m_type = EVertexBufferType::NONE;
	};

	enum class EBlendFactor : u8
	{
		ZERO = 0,
		ONE = 1,
		SRC_COLOR = 2,
		ONE_MINUS_SRC_COLOR = 3,
		DST_COLOR = 4,
		ONE_MINUS_DST_COLOR = 5,
		SRC_ALPHA = 6,
		ONE_MINUS_SRC_ALPHA = 7,
		DST_ALPHA = 8,
		ONE_MINUS_DST_ALPHA =  9,
		CONSTANT_COLOR = 10,
		ONE_MINUS_CONSTANT_COLOR = 11,
		CONSTANT_ALPHA = 12,
		ONE_MINUS_CONSTANT_ALPHA = 13,
		SRC_ALPHA_SATURATE = 14,
	};

	enum class EBlendOP : u8
	{
		ADD,
		SUBTRACT,
		REVERSE_SUBTRACT,
		MIN,
		MAX
	};

	struct AttachmentBlendState // This is tightly packed to use minimal memory for multiple attachments.
	{
		union
		{
			struct
			{
				bool m_enableBlending : 1;
				EBlendOP m_srcBlend : 7;
				EBlendOP m_dstBlend : 8;
				EBlendFactor m_srcColor : 4;
				EBlendFactor m_srcAlpha : 4;
				EBlendFactor m_dstColor : 4;
				EBlendFactor m_dstAlpha : 4;
			};
			u32 m_opaque = 0;
		};
	};
	static_assert(sizeof(AttachmentBlendState) == sizeof(u32));

	enum class EFaceCullMode : u8
	{
		NONE,
		BACK,
		FRONT
	};

	struct GraphicsPipelineDesc
	{
		static constexpr const u32 MaxVertexBuffers = 8;
		static constexpr const u32 MaxColorAttachments = 8;

		static AttachmentBlendState DefaultBlendState() // Set up for additive blending.
		{
			AttachmentBlendState state;
			state.m_enableBlending = true;
			state.m_srcBlend = EBlendOP::ADD;
			state.m_dstBlend = EBlendOP::ADD;
			state.m_srcColor = EBlendFactor::ONE;
			state.m_srcAlpha = EBlendFactor::ZERO;
			state.m_dstColor = EBlendFactor::ONE;
			state.m_dstAlpha = EBlendFactor::ZERO;
			return state;
		}

		RenderPass m_renderPass = 0;
		Rect m_renderArea = {};
		ShaderModule m_shaderModules[static_cast<u32>(EGraphicsShaderStage::GRAPHICS_STAGE_COUNT)] = {};
		PipelineVertexBufferDesc m_vertexBuffers[MaxVertexBuffers]{};
		VertexAttributeDesc m_vertexDesc{};
		AttachmentBlendState m_colorBlendStates[MaxColorAttachments]{};
		ECompareOP m_depthCompareOP = ECompareOP::ALWAYS;
		bool m_stencilTestEnable = false;
		EFaceCullMode m_cullMode = EFaceCullMode::BACK;
		u8 m_subpass = 0;
		u8 m_attachmentCount = 0;
		bool m_depthWriteEnable = true;
		u8 m_pad[2]{ 0, 0 };

		bool operator == (const GraphicsPipelineDesc& other) const;
	};
	static_assert(sizeof(GraphicsPipelineDesc) % 16 == 0);

	struct ComputePipelineDesc
	{
		ShaderModule m_computeModule{};

		bool operator == (const ComputePipelineDesc& other) const;
	};

	class IPipelineCache
	{
	public:

		/*
		Description: Get or create a graphics pipeline using the provided pipeline descriptor object.
		Param:
			const PipelineDesc& desc: The pipeline descriptor object containing parameters that are used to create the pipeline and identify it in the cache.
		*/
		PARABLIT_INTERFACE Pipeline GetPipeline(const GraphicsPipelineDesc& desc) = 0;

		/*
		Description: Get or create a compute pipeline using the provided pipeline descriptor object.
		Param:
			const PipelineDesc& desc: The pipeline descriptor object containing parameters that are used to create the pipeline and identify it in the cache.
		*/
		PARABLIT_INTERFACE Pipeline GetPipeline(const ComputePipelineDesc& desc) = 0;
	};
}