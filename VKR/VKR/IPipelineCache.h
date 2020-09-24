#pragma once
#include "ParaBlitApi.h"
#include "ParaBlitInterface.h"
#include "ParaBlitDefs.h"

namespace PB
{
	struct VertexDesc
	{
		static constexpr const u32 MaxVertexAttributes = 7;
		u32 vertexSize = 0;
		EVertexAttributeType vertexAttributes[MaxVertexAttributes] = {};
	};

	// TODO: ONGOING: Add more paramaters and update the == operator as more are needed.
	struct PipelineDesc
	{
		RenderPass m_renderPass = 0;
		Rect m_renderArea = {};
		ShaderModule m_shaderModules[static_cast<u32>(EShaderStage::PB_SHADER_STAGE_COUNT)] = {};
		VertexDesc m_vertexDesc = {};
		ECompareOP m_depthCompareOP = ECompareOP::ALWAYS;
		bool m_stencilTestEnable = false;
		u16 m_subpass = 0;
		u32 m_pad1 = 0;

		bool operator == (const PipelineDesc& other) const;
	};

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