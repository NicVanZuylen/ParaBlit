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

	enum class EFaceCullMode : u8
	{
		NONE,
		BACK,
		FRONT
	};

	// TODO: ONGOING: Add more paramaters and update the == operator as more are needed.
	struct PipelineDesc
	{
		static constexpr const u32 MaxVertexBuffers = 8;

		RenderPass m_renderPass = 0;
		Rect m_renderArea = {};
		ShaderModule m_shaderModules[static_cast<u32>(EShaderStage::PB_SHADER_STAGE_COUNT)] = {};
		PipelineVertexBufferDesc m_vertexBuffers[MaxVertexBuffers]{};
		VertexAttributeDesc m_vertexDesc{};
		ECompareOP m_depthCompareOP = ECompareOP::ALWAYS;
		bool m_stencilTestEnable = false;
		EFaceCullMode m_cullMode = EFaceCullMode::BACK;
		u8 m_subpass = 0;
		u32 m_attachmentCount = 0;

		bool operator == (const PipelineDesc& other) const;
	};
	static_assert(sizeof(PipelineDesc) % 16 == 0);

	class IPipelineCache
	{
	public:

		/*
		Description: Get or create a Graphics/Compute pipeline using the provided pipeline descriptor object.
		Param:
			const PipelineDesc& desc: The pipeline descriptor object containing parameters that are used to create the pipeline and identify it in the cache.
		*/
		PARABLIT_INTERFACE Pipeline GetPipeline(const PipelineDesc& desc) = 0;
	};
}