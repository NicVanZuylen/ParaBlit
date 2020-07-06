#pragma once
#include "ParaBlitApi.h"
#include "ParaBlitInterface.h"
#include "ParaBlitDefs.h"

namespace PB
{
	// TODO: Add more paramaters and update the == operator as more are needed.
	struct PipelineDesc
	{
		PB::RenderPass m_renderPass = 0;
		u64 m_subpass = 0;
		PB::ShaderModule m_shaderModules[PB_SHADER_STAGE_COUNT] = {};

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