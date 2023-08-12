#include "BatchDispatcher.h"
#include "Resource/Shader.h"

namespace Eng
{

	BatchDispatcher::BatchDispatcher(PB::IRenderer* renderer, CLib::Allocator* allocator)
	{
		m_renderer = renderer;

		PB::ComputePipelineDesc cullPipelineDesc{};
		cullPipelineDesc.m_computeModule = Eng::Shader(m_renderer, "Shaders/GLSL/cs_drawbatch_cull", allocator, true).GetModule();
		m_batchCullPipeline = m_renderer->GetPipelineCache()->GetPipeline(cullPipelineDesc);

		m_useMeshShaders = m_renderer->GetDeviceLimitations()->m_supportMeshShader;
		if (m_useMeshShaders == true)
		{
			cullPipelineDesc.m_computeModule = Eng::Shader(m_renderer, "Shaders/GLSL/cs_drawbatch_cull_tasks", allocator, true).GetModule();
			m_batchCullTasksPipeline = m_renderer->GetPipelineCache()->GetPipeline(cullPipelineDesc);
		}
	}

	BatchDispatcher::~BatchDispatcher()
	{
	}

	void BatchDispatcher::AddBatch(DrawBatch* batch, PB::Pipeline batchDrawPipeline, const PB::BindingLayout& batchBindings)
	{
		m_batches.try_emplace(batchDrawPipeline);
		auto it = m_batches.find(batchDrawPipeline);
		assert(it != m_batches.end());
		it->second.PushBack({ batch, batchBindings });

		m_batchDrawParamQueue.PushBack(batch->GetDrawParametersBuffer());
		m_state = EDispatcherState::PRE_CULL;
	}

	void BatchDispatcher::Reset()
	{
		m_batches.clear();
		m_batchDrawParamQueue.Clear();
		m_state = EDispatcherState::CLEAR;
	}

	void BatchDispatcher::DispatchFrustrumCull(PB::ICommandContext* cmdContext, PB::UniformBufferView viewConstantsView, bool cullMeshlets)
	{
		if (m_batches.empty())
			return;

		assert(m_state == EDispatcherState::PRE_CULL);

		// Dispatch frustrum cull for all batches.
		cmdContext->CmdBindPipeline(cullMeshlets && m_useMeshShaders ? m_batchCullTasksPipeline : m_batchCullPipeline);
		for (auto& pipelineBatches : m_batches)
		{
			for (auto& batchState : pipelineBatches.second)
			{
				batchState.batch->DispatchFrustrumCull(cmdContext, viewConstantsView, cullMeshlets && m_useMeshShaders);
			}
		}

		// Issue memory barrier between dispatch and draw for all batch draw param buffers.
		cmdContext->CmdDrawIndirectBarrier(m_batchDrawParamQueue.Data(), m_batchDrawParamQueue.Count());
		m_state = EDispatcherState::PRE_DRAW;
	}

	void BatchDispatcher::DrawBatches(PB::ICommandContext* cmdContext, PB::UniformBufferView viewConstantsView, PB::Pipeline overridePipeline, bool drawExperimental)
	{
		if (m_batches.empty())
			return;

		assert(m_state == EDispatcherState::PRE_DRAW);

		if (overridePipeline != 0)
		{
			cmdContext->CmdBindPipeline(overridePipeline);
		}

		for (auto& pipelineBatches : m_batches)
		{
			if (overridePipeline == 0)
				cmdContext->CmdBindPipeline(pipelineBatches.first);

			for (auto& batchState : pipelineBatches.second)
			{
				if(drawExperimental == false || m_useMeshShaders == false)
					batchState.batch->DrawCulledGeometry(cmdContext, batchState.batchBindings);
				else
				{
					batchState.batch->DrawAllMeshShader(cmdContext, batchState.batchBindings, viewConstantsView);
				}
			}
		}

		Reset();
	}

};