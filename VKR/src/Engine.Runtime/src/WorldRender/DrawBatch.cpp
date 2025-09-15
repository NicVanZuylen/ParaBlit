#include "DrawBatch.h"
#include "Engine.ParaBlit/IBufferObject.h"
#include "Resource/Mesh.h"
#include "Resource/Shader.h"
#include "Resource/AssetStreamer.h"

namespace Eng
{
	DrawBatch::DrawBatch(const CreateDesc& desc)
    {
        m_renderer = desc.m_renderer;
        m_allocator = desc.m_allocator;
        m_streamer = desc.m_streamer;
        m_meshShadersSupported = m_renderer->GetDeviceLimitations()->m_supportMeshShader;
        m_bounds = Bounds();

		ManagedInstanceBuffer::Desc instanceBufferDesc;
		instanceBufferDesc.m_elementCapacity = MaxObjects;
		instanceBufferDesc.m_elementSize = sizeof(DrawBatchInstanceData);
		instanceBufferDesc.m_usage = PB::EBufferUsage::STORAGE;
		instanceBufferDesc.m_copyAll = true;

        m_instanceBuffer.Init(m_renderer, m_allocator, instanceBufferDesc);

        PB::BufferObjectDesc meshletRangesBufferDesc;
        meshletRangesBufferDesc.m_bufferSize = sizeof(Vector2u) * MaxObjects;
        meshletRangesBufferDesc.m_options = 0;
        meshletRangesBufferDesc.m_usage = PB::EBufferUsage::STORAGE | PB::EBufferUsage::COPY_DST;
        m_meshletRangesBuffer = m_renderer->AllocateBuffer(meshletRangesBufferDesc);

        PB::BufferObjectDesc cullConstantsBufferDesc;
        cullConstantsBufferDesc.m_bufferSize = sizeof(BatchCullConstants);
        cullConstantsBufferDesc.m_options = 0;
        cullConstantsBufferDesc.m_usage = PB::EBufferUsage::STORAGE | PB::EBufferUsage::COPY_DST;
        m_cullConstantsBuffer = m_renderer->AllocateBuffer(cullConstantsBufferDesc);

        PB::BufferObjectDesc drawRangesBufferDesc;
        drawRangesBufferDesc.m_bufferSize = sizeof(MeshletDrawRangesBuffer);
        drawRangesBufferDesc.m_options = 0;
        drawRangesBufferDesc.m_usage = PB::EBufferUsage::STORAGE;
        m_drawRangesBuffer = m_renderer->AllocateBuffer(drawRangesBufferDesc);

        PB::BufferObjectDesc drawParamsBufferDesc;
        drawParamsBufferDesc.m_bufferSize = sizeof(MeshletDrawParamsBuffer);
        drawParamsBufferDesc.m_options = 0;
        drawParamsBufferDesc.m_usage = PB::EBufferUsage::STORAGE | PB::EBufferUsage::INDIRECT_PARAMS;
        m_drawMeshletParamsBuffer = m_renderer->AllocateBuffer(drawParamsBufferDesc);
    }

    DrawBatch::~DrawBatch()
    {
        if (m_meshletRangesBuffer)
        {
            m_renderer->FreeBuffer(m_meshletRangesBuffer);
            m_meshletRangesBuffer = nullptr;
        }

        if (m_drawMeshletParamsBuffer != nullptr)
        {
            m_renderer->FreeBuffer(m_drawMeshletParamsBuffer);
        }
        
        m_renderer->FreeBuffer(m_cullConstantsBuffer);
        m_renderer->FreeBuffer(m_drawRangesBuffer);

        for (auto& batch : m_instanceStreamingBatches)
        {
            if (batch == nullptr)
                continue;

            batch->EndStreamingAndDelete();
        }
        m_instanceStreamingBatches.Clear();
    }

    void DrawBatch::DispatchFrustrumCull(PB::ICommandContext* cmdContext, PB::UniformBufferView viewConstantsView, bool cullTasks)
    {
        PB::UniformBufferView constants[]
        {
            viewConstantsView,
        };

        PB::ResourceView drawParamsView;
        PB::ResourceView drawCountView;

        PB::BufferViewDesc drawViewDesc{};
        drawViewDesc.m_buffer = m_drawMeshletParamsBuffer;
        drawViewDesc.m_offset = offsetof(MeshletDrawParamsBuffer, MeshletDrawParamsBuffer::m_drawMeshTasksParams);;
        drawViewDesc.m_size = sizeof(MeshletDrawParamsBuffer::m_drawMeshTasksParams);
        drawParamsView = m_drawMeshletParamsBuffer->GetViewAsStorageBuffer(drawViewDesc);

        drawViewDesc.m_offset = offsetof(MeshletDrawParamsBuffer, MeshletDrawParamsBuffer::m_drawCount);
        drawViewDesc.m_size = sizeof(MeshletDrawParamsBuffer::m_drawCount);
        drawCountView = m_drawMeshletParamsBuffer->GetViewAsStorageBuffer(drawViewDesc);

        PB::ResourceView resources[]
        {
            m_cullConstantsBuffer->GetViewAsStorageBuffer(),
            m_drawRangesBuffer->GetViewAsStorageBuffer(),
            drawParamsView,
            drawCountView
        };

        PB::BindingLayout bindings;
        bindings.m_uniformBufferCount = _countof(constants);
        bindings.m_uniformBuffers = constants;
        bindings.m_resourceCount = _countof(resources);
        bindings.m_resourceViews = resources;
        cmdContext->CmdBindResources(bindings);
        cmdContext->CmdDispatch(1, 1, 1);
    }

    void DrawBatch::DrawAllMeshShader
	(
		PB::ICommandContext* cmdContext, 
		const PB::BindingLayout& bindings, 
		PB::ResourceView instanceBuffer, 
		PB::ResourceView meshletRangesBuffer, 
		PB::ResourceView drawRangesBuffer, 
		PB::ResourceView meshLibraryBuffer,
		PB::IBufferObject* drawParamsBuffer, 
		PB::UniformBufferView viewConstantsView, 
		PB::u32 drawParamsOffset
	)
    {
		CLib::Vector<PB::UniformBufferView, 8, 8> uniformBindings = { viewConstantsView };

		for (uint32_t i = 0; i < bindings.m_uniformBufferCount; ++i)
			uniformBindings.PushBack(bindings.m_uniformBuffers[i]);

		PB::BindingLayout finalBindingLayout{};
		finalBindingLayout.m_uniformBuffers = uniformBindings.Data();
		finalBindingLayout.m_uniformBufferCount = uniformBindings.Count();

		PB::ResourceView batchResources[] =
		{
			meshletRangesBuffer,
			drawRangesBuffer,
			instanceBuffer,
			meshLibraryBuffer
		};
		finalBindingLayout.m_resourceCount = _countof(batchResources);
		finalBindingLayout.m_resourceViews = batchResources;

		constexpr uint32_t TaskWorkGroupMeshletCount = 32;

		cmdContext->CmdBindResources(finalBindingLayout);
		cmdContext->CmdDrawMeshTasksIndirectCount
		(
			drawParamsBuffer, 
			drawParamsOffset, 
			drawParamsBuffer, 
			drawParamsOffset + offsetof(MeshletDrawParamsBuffer, MeshletDrawParamsBuffer::m_drawCount), 
			MaxDrawCount, 
			sizeof(PB::DrawMeshTasksIndirectParams)
		);
    }

    void DrawBatch::LocalDrawAllMeshShader(PB::ICommandContext* cmdContext, const PB::BindingLayout& bindings, PB::UniformBufferView viewConstantsView)
    {
		assert(m_meshShadersSupported == true);

		// Ensure streaming of required resources is complete.
		{
			bool readyToDraw = true;
			for (StreamingBatch*& streamingBatch : m_instanceStreamingBatches)
			{
				bool batchReady = streamingBatch == nullptr || (streamingBatch->GetStatus() == StreamingBatch::EStreamingStatus::IDLE);
				if (streamingBatch && batchReady)
				{
					streamingBatch->EndStreamingAndDelete();
					streamingBatch = nullptr;
				}

				readyToDraw &= batchReady;
			}
			if (readyToDraw == false)
				return;

			if (m_instanceStreamingBatches.Count() > 0)
			{
				m_instanceBuffer.FlushChanges();
				m_instanceStreamingBatches.Clear();
			}
		}

		DrawAllMeshShader
		(
			cmdContext, 
			bindings, 
			m_instanceBuffer.GetBuffer()->GetViewAsStorageBuffer(),
			m_meshletRangesBuffer->GetViewAsStorageBuffer(),
			m_drawRangesBuffer->GetViewAsStorageBuffer(),
			Mesh::GetMeshLibraryView(),
			m_drawMeshletParamsBuffer, 
			viewConstantsView, 
			0
		);
    }

    void DrawBatch::AddInstance(AssetEncoder::AssetID meshID, const float* modelMatrix, const Bounds& bounds, AssetEncoder::AssetID* textureIDs, uint32_t textureCount, PB::ResourceView sampler)
    {
        assert(meshID != ~AssetEncoder::AssetID(0));
        assert(modelMatrix);
        assert(textureIDs || textureCount == 0);
        assert(textureIDs || sampler == 0);
        assert(textureCount <= DrawBatchInstanceData::MaxTextures);

        // Get the instance mesh's meshlet count.
        {
            AssetPipeline::MeshCacheData meshData;
            Mesh::GetMeshData(meshID, &meshData);

            m_meshletRanges.PushBack(Vector2u(m_batchMeshletCount, m_batchMeshletCount + meshData.m_meshletCount));
            m_batchMeshletCount += meshData.m_meshletCount;
        }

        // Upload model matrix and sampler here. The texture and vertex buffer ids will be updated when those assets are streamed in.
        DrawBatchInstanceData& instanceData = *reinterpret_cast<DrawBatchInstanceData*>(m_instanceBuffer.GetInstanceData(m_instanceBuffer.AddInstance()));
        memset(&instanceData, 0, sizeof(DrawBatchInstanceData));
        memcpy(instanceData.m_modelMatrix, modelMatrix, sizeof(instanceData.m_modelMatrix));
        instanceData.m_sampler = sampler;

		// Setup instance streaming.
		{
			StreamingBatch* instanceBatch = m_streamer->AllocStreamingBatch();

			instanceBatch->AddResource(StreamableHandle(meshID, EStreamableResourceType::MESH, StreamableHandle::EBindingType::STORAGE)); // Mesh library instance index.
			for (uint32_t i = 0; i < textureCount; ++i)
			{
				instanceBatch->AddResource(StreamableHandle(textureIDs[i], EStreamableResourceType::TEXTURE, StreamableHandle::EBindingType::SRV));
			}
			instanceBatch->SetOutputBindingLocation(instanceData.m_bindings);
			instanceBatch->BeginStreaming();

			m_instanceStreamingBatches.PushBack(instanceBatch);
		}

        m_instanceBoundData.PushBack(bounds);
        if (m_bounds.IsIdentity())
        {
            m_bounds = bounds;
        }
        else
        {
            m_bounds.Encapsulate(bounds);
        }

        ++m_batchInstanceCount;
    }

    void DrawBatch::FinalizeUpdates()
    {
        PB::u8* meshletRangesData = m_meshletRangesBuffer->BeginPopulate();
        memset(meshletRangesData, 0, sizeof(Vector2u) * MaxObjects);
        memcpy(meshletRangesData, m_meshletRanges.Data(), m_meshletRanges.Count() * sizeof(Vector2u));
        m_meshletRangesBuffer->EndPopulate();
    }

    void DrawBatch::UpdateIndices(PB::ICommandContext* cmdContext)
    {
        assert(cmdContext);

        FinalizeUpdates();
    }

    void DrawBatch::UpdateCullParams()
    {
        BatchCullConstants* cullData = reinterpret_cast<BatchCullConstants*>(m_cullConstantsBuffer->BeginPopulate());
        memset(cullData, 0, sizeof(BatchCullConstants));

        uint32_t totalMeshletCount = 0;
        for (uint32_t i = 0; i < m_instanceBoundData.Count(); ++i)
        {
            Vector2u& meshletData = m_meshletRanges[i];
            auto& bounds = m_instanceBoundData[i];
            auto& objectCullData = cullData->m_objects[i];

            uint32_t meshletCount = meshletData[1] - meshletData[0];

            objectCullData.m_boundOrigin = bounds.m_origin;
            objectCullData.m_boundExtents = bounds.m_extents;
            objectCullData.m_drawRange[0] = totalMeshletCount;
            objectCullData.m_drawRange[1] = totalMeshletCount + meshletCount;

            totalMeshletCount += meshletCount;
        }

        m_cullConstantsBuffer->EndPopulate();
    }

};