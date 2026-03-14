#pragma once
#include "Engine.ParaBlit/ParaBlitDefs.h"
#include "WorldRender/DynamicDrawPool.h"

#include <list>

namespace Eng
{
	class RenderDefinition;

	class StaticObjectRenderer
	{
	public:

		StaticObjectRenderer(PB::IRenderer* renderer, CLib::Allocator* allocator);
		StaticObjectRenderer() = default;
		~StaticObjectRenderer();

		void Init(PB::IRenderer* renderer, CLib::Allocator* allocator);

		void AddObject(TObjectPtr<RenderDefinition> objectRenderDefinition);
		void RemoveObject(TObjectPtr<RenderDefinition> objectRenderDefinition);

		void ForceUpdate() { m_updateRequired = true; };

		void UpdateComputeGPU(PB::ICommandContext* commandContext, PB::UniformBufferView cullViewPlanes, PB::UniformBufferView lodViewPlanes = 0, bool waitForPreviousDraw = false);
		void UpdateTLASInstances
		(
			PB::ICommandContext* commandContext, 
			PB::IBufferObject* dstBuffer, 
			PB::IBufferObject* dstInstanceIndexBuffer, 
			PB::u32 dstIndex, 
			PB::UniformBufferView cullConstants, 
			uint32_t& outInstanceCount
		);
		uint32_t GetDrawPoolSize() const { return m_drawPool.GetBatchCount(); }
		void Draw(PB::ICommandContext* commandContext, PB::UniformBufferView viewConstants, PB::UniformBufferView cullConstants) const;

	private:

		CLib::Allocator* m_allocator = nullptr;
		std::list<TObjectPtr<RenderDefinition>> m_streamingObjects;
		DynamicDrawPool m_drawPool;
		bool m_updateRequired = false;
	};
};