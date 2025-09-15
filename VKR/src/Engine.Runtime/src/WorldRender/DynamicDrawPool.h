#pragma once
#include "DrawBatch.h"
#include "ManagedInstanceBuffer.h"

namespace Eng
{
	/* --------------- DynamicDrawPool --------------- */
	/* DynamicDrawPool is a GPU-driven solution to create drawbatches in real time using
	*  compute shaders to generate the necessary data to render geometry in batched draw calls.
	*  Object instances within the DynamicDrawPool can have any property modified in real time, such
	*  as model transforms, meshes, textures and samplers.
	*
	*  The pool will grow in size by a batch of 256 instances when more capacity is needed.
	*  Expansions will add additional draw calls in the worst-case scenario where many objects are not culled.
	*
	*  Any object instance added to the pool can also be disabled, enabled, or removed in real time
	*  with minimal cost.
	*/
	/* --------------- DynamicDrawPool --------------- */
	class DynamicDrawPool
	{
	public:

		using InstanceID = PB::u32;
		static constexpr InstanceID InvalidInstanceID = ~PB::u32(0);

		struct InstanceCullData
		{
			Vector3f m_origin;
			float m_radius;
		};

		enum class EMemoryMode
		{
			FAST,				// Keep non-essential buffers between updates to avoid frequent buffer allocations.
			MEMORY_SAVING,		// Discard non-essential buffers when not updating to save memory.
		};

		struct Desc
		{
			
		};

		DynamicDrawPool() = default;

		DynamicDrawPool(const Desc& desc, PB::IRenderer* renderer, CLib::Allocator* allocator)
		{
			Init(desc, renderer, allocator);
		}

		~DynamicDrawPool();

		void SetBatchCount(uint32_t count);
		uint32_t GetBatchCount() const { return m_batchCount; }
		void Init(const Desc& desc, PB::IRenderer* renderer, CLib::Allocator* allocator);
		InstanceID AddInstance();
		DrawBatch::DrawBatchInstanceData* GetInstanceData(InstanceID instance);
		InstanceCullData* GetInstanceCullingData(InstanceID instance);
		void RemoveInstance(InstanceID instance);
		void SetInstanceEnable(InstanceID instance, bool enable)
		{ 
			m_instanceBuffer.SetInstanceEnable(instance, enable);
		};
		void UpdateTLASInstances
		(
			PB::ICommandContext* commandContext, 
			PB::IBufferObject* dstBuffer, 
			PB::IBufferObject* dstInstanceIndexBuffer, 
			PB::u32 dstIndex,
			PB::UniformBufferView cullConstants, 
			uint32_t& outInstanceCount
		);
		void UpdateComputeGPU(PB::ICommandContext* commandContext, PB::UniformBufferView cullConstants, bool keepFrameInstanceData = false);
		void Draw(PB::ICommandContext* commandContext, PB::UniformBufferView viewConstants, PB::UniformBufferView cullConstants) const;

	private:

		struct DrawParamsBuffer
		{
			PB::DrawMeshTasksIndirectParams m_drawMeshTasksParams;
			uint32_t m_drawCount;
			uint32_t m_pad[4];
		};

		void FreeBuffers();

		Desc m_desc{};
		PB::IRenderer* m_renderer = nullptr;
		CLib::Allocator* m_allocator = nullptr;
		uint32_t m_batchCount = 0;
		PB::Pipeline m_batchSetupPipeline = 0;
		PB::Pipeline m_tlasUpdatePipeline = 0;
		ManagedInstanceBuffer m_instanceBuffer;
		InstanceCullData* m_cullData = nullptr;
		PB::IBufferObject* m_drawInstancesBuffer = nullptr;
		PB::IBufferObject* m_cullDataBuffer = nullptr;
		PB::IBufferObject* m_meshletRangesBuffer = nullptr;
		PB::IBufferObject* m_drawParamsBuffer = nullptr;
		PB::IBufferObject* m_drawRangesBuffer = nullptr;

		bool m_instanceDataNeedsUpload = false;
		bool m_cullDataNeedsUpload = false;
	};
}