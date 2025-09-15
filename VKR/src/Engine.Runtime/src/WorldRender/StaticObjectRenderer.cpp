#pragma once
#include "Entity/Component/RenderDefinition.h"
#include "StaticObjectRenderer.h"

namespace Eng
{
	StaticObjectRenderer::StaticObjectRenderer(PB::IRenderer* renderer, CLib::Allocator* allocator)
	{
		Init(renderer, allocator);
	}

	StaticObjectRenderer::~StaticObjectRenderer()
	{
	}

	void StaticObjectRenderer::Init(PB::IRenderer* renderer, CLib::Allocator* allocator)
	{
		DynamicDrawPool::Desc drawPoolDesc{};
		m_drawPool.Init(drawPoolDesc, renderer, allocator);
	}

	void StaticObjectRenderer::AddObject(TObjectPtr<RenderDefinition> objectRenderDefinition)
	{
		objectRenderDefinition->CommitRenderEntity(&m_drawPool);
		m_streamingObjects.push_back(objectRenderDefinition);
		m_updateRequired = true;
	}

	void StaticObjectRenderer::RemoveObject(TObjectPtr<RenderDefinition> objectRenderDefinition)
	{
		objectRenderDefinition->UncommitRenderEntity();
		m_updateRequired = true;
	}

	void StaticObjectRenderer::UpdateComputeGPU(PB::ICommandContext* commandContext, PB::UniformBufferView cullConstants)
	{
		auto it = m_streamingObjects.begin();
		while (it != m_streamingObjects.end())
		{
			auto& obj = *it;
			const StreamingBatch* batch = obj->GetStreamingBatch();

			if (batch == nullptr)
			{
				it = m_streamingObjects.erase(it);
				continue;
			}

			if (batch->GetStatus() == StreamingBatch::EStreamingStatus::IDLE)
			{
				obj->UpdateStaticRenderEntity();
				it = m_streamingObjects.erase(it);
				
				m_updateRequired = true;
			}
			else
			{
				++it;
			}
		}

		m_drawPool.UpdateComputeGPU(commandContext, cullConstants, !m_updateRequired);
		m_updateRequired = false;
	}

	void StaticObjectRenderer::UpdateTLASInstances
	(
		PB::ICommandContext* commandContext, 
		PB::IBufferObject* dstBuffer, 
		PB::IBufferObject* dstInstanceIndexBuffer, 
		PB::u32 dstIndex, 
		PB::UniformBufferView cullConstants, 
		uint32_t& outInstanceCount
	)
	{
		m_drawPool.UpdateTLASInstances(commandContext, dstBuffer, dstInstanceIndexBuffer, dstIndex, cullConstants, outInstanceCount);
	}

	void StaticObjectRenderer::Draw(PB::ICommandContext* commandContext, PB::UniformBufferView viewConstants, PB::UniformBufferView cullConstants) const
	{
		m_drawPool.Draw(commandContext, viewConstants, cullConstants);
	}
};