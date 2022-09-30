#pragma once

#include "CLib/Vector.h"
#include <unordered_map>

#include "DrawBatch.h"

class BatchDispatcher
{
public:

	BatchDispatcher(PB::IRenderer* renderer, CLib::Allocator* allocator);

	~BatchDispatcher();

	void AddBatch(DrawBatch* batch, PB::Pipeline batchDrawPipeline, const PB::BindingLayout& bindings);

	void Reset();

	void DispatchFrustrumCull(PB::ICommandContext* cmdContext, PB::UniformBufferView viewConstantsView);

	void DrawBatches(PB::ICommandContext* cmdContext, PB::Pipeline overridePipeline = 0);

private:

	struct BatchState
	{
		DrawBatch* batch;
		PB::BindingLayout batchBindings;
	};

	enum class EDispatcherState
	{
		CLEAR,
		PRE_CULL,
		PRE_DRAW,
	};

	std::unordered_map<PB::Pipeline, CLib::Vector<BatchState, 64, 64>> m_batches;
	CLib::Vector<const PB::IBufferObject*, 64, 64> m_batchDrawParamQueue;
	PB::IRenderer* m_renderer = nullptr;
	PB::Pipeline m_batchCullPipeline = 0;
	EDispatcherState m_state = EDispatcherState::CLEAR;
};