#include "DynamicDrawPool.h"
#include "Resource/Shader.h"
#include "Resource/Mesh.h"

namespace Eng
{
	DynamicDrawPool::~DynamicDrawPool()
	{
		FreeBuffers();
	}

	void DynamicDrawPool::SetBatchCount(uint32_t count)
	{
		if (m_batchCount == count)
			return;

		// Set up cull data.
		uint32_t newCullDataSize = sizeof(InstanceCullData) * DrawBatch::MaxObjects * count;
		InstanceCullData* newCullData = reinterpret_cast<InstanceCullData*>(m_allocator->Alloc(newCullDataSize));
		std::memset(newCullData, 0, newCullDataSize);
		{
			if (m_cullData != nullptr)
			{
				uint32_t oldCullDataSize = sizeof(InstanceCullData) * DrawBatch::MaxObjects * m_batchCount;
				uint32_t minSize = std::min<uint32_t>(oldCullDataSize, newCullDataSize);
				std::memcpy(newCullData, m_cullData, minSize);
			}

			m_cullDataNeedsUpload = true;
		}

		if (m_batchCount > 0)
		{
			FreeBuffers();
		}
		m_batchCount = count;
		m_cullData = newCullData;

		m_instanceBuffer.Resize(DrawBatch::MaxObjects * m_batchCount);

		PB::BufferObjectDesc instancesBufferDesc;
		instancesBufferDesc.m_name = "DynamicDrawPool::instanceBuffer";
		instancesBufferDesc.m_bufferSize = m_batchCount * DrawBatch::MaxObjects * sizeof(DrawBatch::DrawBatchInstanceData);
		instancesBufferDesc.m_options = 0;
		instancesBufferDesc.m_usage = PB::EBufferUsage::STORAGE;
		m_drawInstancesBuffer = m_renderer->AllocateBuffer(instancesBufferDesc);

		PB::BufferObjectDesc cullDataBufferDesc;
		cullDataBufferDesc.m_name = "DynamicDrawPool::cullDataBuffer";
		cullDataBufferDesc.m_bufferSize = newCullDataSize;
		cullDataBufferDesc.m_options = 0;
		cullDataBufferDesc.m_usage = PB::EBufferUsage::STORAGE | PB::EBufferUsage::COPY_DST;
		m_cullDataBuffer = m_renderer->AllocateBuffer(cullDataBufferDesc);

		PB::BufferObjectDesc meshletRangesBufferDesc;
		meshletRangesBufferDesc.m_name = "DynamicDrawPool::meshletRangesBuffer";
		meshletRangesBufferDesc.m_bufferSize = m_batchCount * sizeof(Vector2u) * DrawBatch::MaxObjects;
		meshletRangesBufferDesc.m_options = 0;
		meshletRangesBufferDesc.m_usage = PB::EBufferUsage::STORAGE | PB::EBufferUsage::COPY_DST;
		m_meshletRangesBuffer = m_renderer->AllocateBuffer(meshletRangesBufferDesc);

		PB::BufferObjectDesc drawRangesBufferDesc;
		drawRangesBufferDesc.m_name = "DynamicDrawPool::drawRangesBuffer";
		drawRangesBufferDesc.m_bufferSize = m_batchCount * sizeof(Vector4u);
		drawRangesBufferDesc.m_options = 0;
		drawRangesBufferDesc.m_usage = PB::EBufferUsage::STORAGE;
		m_drawRangesBuffer = m_renderer->AllocateBuffer(drawRangesBufferDesc);

		PB::BufferObjectDesc drawParamsBufferDesc;
		drawParamsBufferDesc.m_name = "DynamicDrawPool::drawParamsBuffer";
		drawParamsBufferDesc.m_bufferSize = m_batchCount * sizeof(DrawBatch::MeshletDrawParamsBuffer);
		drawParamsBufferDesc.m_options = 0;
		drawParamsBufferDesc.m_usage = PB::EBufferUsage::STORAGE | PB::EBufferUsage::INDIRECT_PARAMS;
		m_drawParamsBuffer = m_renderer->AllocateBuffer(drawParamsBufferDesc);
	}

	void DynamicDrawPool::Init(const Desc& desc, PB::IRenderer* renderer, CLib::Allocator* allocator)
	{
		m_desc = desc;
		m_renderer = renderer;
		m_allocator = allocator;

		const uint32_t initialBatchCount = 1;

		AssetEncoder::ShaderPermutationTable permTable{};

		ManagedInstanceBuffer::Desc instanceDesc;
		instanceDesc.m_elementSize = sizeof(DrawBatch::DrawBatchInstanceData);
		instanceDesc.m_elementCapacity = initialBatchCount * DrawBatch::MaxObjects;
		instanceDesc.m_usage = PB::EBufferUsage::STORAGE;
		instanceDesc.m_cullShaderName = "Shaders/GLSL/cs_managedinstance_with_frustrum_cull";
		instanceDesc.m_cullPermutationKey = permTable.SetPermutation(AssetEncoder::EDefaultPermutationID::PERMUTATION_0, 0).GetKey();
		instanceDesc.m_populateShaderName = "Shaders/GLSL/cs_managedinstance_with_frustrum_cull";
		instanceDesc.m_populatePermutationKey = permTable.SetPermutation(AssetEncoder::EDefaultPermutationID::PERMUTATION_0, 1).GetKey();
		instanceDesc.m_autoSwapStagingOnFlush = false;
		instanceDesc.m_copyAll = false;

		m_instanceBuffer.Init(m_renderer, m_allocator, instanceDesc);

		SetBatchCount(initialBatchCount);

		PB::ComputePipelineDesc cullPipelineDesc{};
		cullPipelineDesc.m_computeModule = Eng::Shader(m_renderer, "Shaders/GLSL/cs_drawbatch_setup", 0, m_allocator, true).GetModule();
		m_batchSetupPipeline = m_renderer->GetPipelineCache()->GetPipeline(cullPipelineDesc);

		PB::ComputePipelineDesc tlasUpdatePipelineDesc{};
		tlasUpdatePipelineDesc.m_computeModule = Eng::Shader(m_renderer, "Shaders/GLSL/cs_update_top_level_acceleration_structure_instances", 0, m_allocator, true).GetModule();
		m_tlasUpdatePipeline = m_renderer->GetPipelineCache()->GetPipeline(tlasUpdatePipelineDesc);
	}

	void DynamicDrawPool::FreeBuffers()
	{
		if (m_cullData != nullptr)
		{
			m_allocator->Free(reinterpret_cast<void*>(m_cullData));
			m_cullData = nullptr;
		}

		if (m_drawInstancesBuffer)
		{
			m_renderer->FreeBuffer(m_drawInstancesBuffer);
			m_drawInstancesBuffer = nullptr;
		}
		if(m_cullDataBuffer != nullptr)
		{
			m_renderer->FreeBuffer(m_cullDataBuffer);
			m_cullDataBuffer = nullptr;
		}
		if (m_meshletRangesBuffer != nullptr)
		{
			m_renderer->FreeBuffer(m_meshletRangesBuffer);
			m_meshletRangesBuffer = nullptr;
		}
		if (m_drawRangesBuffer != nullptr)
		{
			m_renderer->FreeBuffer(m_drawRangesBuffer);
			m_drawRangesBuffer = nullptr;
		}
		if (m_drawParamsBuffer != nullptr)
		{
			m_renderer->FreeBuffer(m_drawParamsBuffer);
			m_drawParamsBuffer = nullptr;
		}
	}

	DynamicDrawPool::InstanceID DynamicDrawPool::AddInstance()
	{
		if (m_instanceBuffer.GetInstanceCount() + 1 > m_batchCount * DrawBatch::MaxObjects)
		{
			SetBatchCount(m_batchCount + 1);
		}

		return m_instanceBuffer.AddInstance();
	}

	DrawBatch::DrawBatchInstanceData* DynamicDrawPool::GetInstanceData(InstanceID instance)
	{
		return reinterpret_cast<DrawBatch::DrawBatchInstanceData*>(m_instanceBuffer.GetInstanceData(instance));
	}

	DynamicDrawPool::InstanceCullData* DynamicDrawPool::GetInstanceCullingData(InstanceID instance)
	{
		m_cullDataNeedsUpload = true; // Ensure any changes made to the returned data are uploaded.

		return m_cullData + instance;
	}

	void DynamicDrawPool::RemoveInstance(InstanceID instance)
	{
		return m_instanceBuffer.RemoveInstance(instance);
	}

	void DynamicDrawPool::UpdateTLASInstances
	(
		PB::ICommandContext* commandContext, 
		PB::IBufferObject* dstBuffer, 
		PB::IBufferObject* dstInstanceIndexBuffer, 
		PB::u32 dstIndex, 
		PB::UniformBufferView cullConstants, 
		uint32_t& outInstanceCount
	)
	{
		PB::BufferViewDesc dstIndexViewDesc{};
		dstIndexViewDesc.m_buffer = dstInstanceIndexBuffer;
		dstIndexViewDesc.m_offset = dstIndex * sizeof(PB::u32);
		dstIndexViewDesc.m_size = dstInstanceIndexBuffer->GetSize() - dstIndexViewDesc.m_offset;

		PB::ResourceView resources[] =
		{
			Mesh::GetMeshLibraryView(),
			dstInstanceIndexBuffer->GetViewAsStorageBuffer(dstIndexViewDesc)
		};

		PB::BindingLayout additionalFlushBindings;
		additionalFlushBindings.m_uniformBufferCount = 1;
		additionalFlushBindings.m_uniformBuffers = &cullConstants;
		additionalFlushBindings.m_resourceCount = _countof(resources);
		additionalFlushBindings.m_resourceViews = resources;

		ManagedInstanceBuffer::FlushDesc flushDesc;
		flushDesc.cullPipeline = 0;
		flushDesc.populatePipeline = m_tlasUpdatePipeline;
		flushDesc.additionalBindings = &additionalFlushBindings;
		flushDesc.dstBuffer = dstBuffer;
		flushDesc.dstBufferOffset = dstIndex * sizeof(PB::AccelerationStructureInstance);
		flushDesc.skipStagingUpload = false;

		m_instanceBuffer.FlushToBuffer(flushDesc, commandContext);
		outInstanceCount = m_instanceBuffer.GetInstanceCount();
	}

	void DynamicDrawPool::UpdateComputeGPU(PB::ICommandContext* commandContext, PB::UniformBufferView cullConstants, bool keepFrameInstanceData)
	{
		if (m_cullDataNeedsUpload == true)
		{
			uint32_t cullDataSize = sizeof(InstanceCullData) * DrawBatch::MaxObjects * m_batchCount;

			PB::u8* dstCullData = m_cullDataBuffer->BeginPopulate(cullDataSize);
			std::memcpy(dstCullData, m_cullData, cullDataSize);
			m_cullDataBuffer->EndPopulate();

			m_cullDataNeedsUpload = false;
		}

		PB::ResourceView cullDataView = m_cullDataBuffer->GetViewAsStorageBuffer();

		if (keepFrameInstanceData == false)
		{
			m_instanceBuffer.SwapStaging();
		}

		PB::ResourceView meshLibraryView = Mesh::GetMeshLibraryView();

		PB::ResourceView flushResources[]
		{
			meshLibraryView,
			cullDataView
		};

		PB::BindingLayout additionalFlushBindings;
		additionalFlushBindings.m_uniformBufferCount = 1;
		additionalFlushBindings.m_uniformBuffers = &cullConstants;
		additionalFlushBindings.m_resourceCount = _countof(flushResources);
		additionalFlushBindings.m_resourceViews = flushResources;
		m_instanceBuffer.FlushChanges(nullptr, keepFrameInstanceData, &additionalFlushBindings);

		// Setup/fill drawbatch buffers
		{
			commandContext->CmdBindPipeline(m_batchSetupPipeline);

			PB::ResourceView resources[]
			{
				m_instanceBuffer.GetBuffer()->GetViewAsStorageBuffer(),
				m_drawInstancesBuffer->GetViewAsStorageBuffer(),
				meshLibraryView,
				m_meshletRangesBuffer->GetViewAsStorageBuffer(),
				m_drawRangesBuffer->GetViewAsStorageBuffer(),
				m_drawParamsBuffer->GetViewAsStorageBuffer()
			};

			PB::BindingLayout bindings;
			bindings.m_uniformBufferCount = 0;
			bindings.m_uniformBuffers = nullptr;
			bindings.m_resourceCount = _countof(resources);
			bindings.m_resourceViews = resources;
			commandContext->CmdBindResources(bindings);
			commandContext->CmdDispatch(m_batchCount, 1, 1);
		}

		const PB::IBufferObject* drawParams = m_drawParamsBuffer;
		commandContext->CmdDrawIndirectBarrier(&drawParams, 1);
	}

	void DynamicDrawPool::Draw(PB::ICommandContext* commandContext, PB::UniformBufferView viewConstants, PB::UniformBufferView cullConstants) const
	{
		// Draw all batches. (Draw count will be zero if a batch is not filled by the above step.)
		{
			constexpr uint32_t DrawBatchInstanceBufferStride = DrawBatch::MaxObjects * sizeof(DrawBatch::DrawBatchInstanceData);
			constexpr uint32_t DrawBatchMeshletRangesBufferStride = DrawBatch::MaxObjects * sizeof(Vector2u);
			constexpr uint32_t DrawBatchDrawRangesBufferStride = sizeof(Vector4u);

			PB::BufferViewDesc instanceViewDesc;
			instanceViewDesc.m_buffer = m_drawInstancesBuffer;
			instanceViewDesc.m_size = DrawBatchInstanceBufferStride;

			PB::BufferViewDesc meshletRangesViewDesc;
			meshletRangesViewDesc.m_buffer = m_meshletRangesBuffer;
			meshletRangesViewDesc.m_size = DrawBatchMeshletRangesBufferStride;

			PB::BufferViewDesc drawRangesViewDesc;
			drawRangesViewDesc.m_buffer = m_drawRangesBuffer;
			drawRangesViewDesc.m_size = DrawBatchDrawRangesBufferStride;

			PB::UniformBufferView uniformBuffers[] = { viewConstants };

			PB::BindingLayout batchBindings{};
			batchBindings.m_uniformBufferCount = _countof(uniformBuffers);
			batchBindings.m_uniformBuffers = uniformBuffers;

			for (uint32_t i = 0; i < m_batchCount; ++i)
			{
				instanceViewDesc.m_offset = i * DrawBatchInstanceBufferStride;
				meshletRangesViewDesc.m_offset = i * DrawBatchMeshletRangesBufferStride;
				drawRangesViewDesc.m_offset = i * DrawBatchDrawRangesBufferStride;

				DrawBatch::DrawAllMeshShader
				(
					commandContext,
					batchBindings,
					m_drawInstancesBuffer->GetViewAsStorageBuffer(instanceViewDesc),
					m_meshletRangesBuffer->GetViewAsStorageBuffer(meshletRangesViewDesc),
					m_drawRangesBuffer->GetViewAsStorageBuffer(drawRangesViewDesc),
					Mesh::GetMeshLibraryView(),
					m_drawParamsBuffer,
					cullConstants,
					i * sizeof(DrawBatch::MeshletDrawParamsBuffer)
				);
			}
		}
	}
}
