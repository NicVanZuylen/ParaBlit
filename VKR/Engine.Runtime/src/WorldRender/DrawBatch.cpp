#include "DrawBatch.h"
#include "Engine.ParaBlit/IBufferObject.h"
#include "Resource/Mesh.h"
#include "Resource/Shader.h"
#include "Resource/AssetStreamer.h"

namespace Eng
{

    VertexPool::VertexPool(PB::IRenderer* renderer, PB::u32 poolSize, PB::u32 vertexStride)
    {
        m_renderer = renderer;
        m_vertexStride = vertexStride;

        PB::ResourcePoolDesc poolDesc{};
        poolDesc.m_size = poolSize;
        poolDesc.m_memoryType = PB::EMemoryType::DEVICE_LOCAL;
        m_pool = m_renderer->AllocateResourcePool(poolDesc);
    }

    VertexPool::~VertexPool()
    {
        m_renderer->FreeResourcePool(m_pool);
        m_pool = nullptr;

        m_currentPoolOffset = 0;
    }

    void VertexPool::GetNextVertexOffset(PB::u32 size, PB::u32& firstVertex)
    {
        firstVertex = m_currentPoolOffset / m_vertexStride;
        m_currentPoolOffset += size;
    }

    PB::IResourcePool* VertexPool::GetPool()
    {
        return m_pool;
    }

    DrawBatch::DrawBatch(const CreateDesc& desc)
    {
        m_renderer = desc.m_renderer;
        m_allocator = desc.m_allocator;
        m_streamer = desc.m_streamer;
        m_bounds = Bounds();

        CreateUpdateResources();

        m_instanceBuffer.Init(m_renderer, PB::EBufferUsage::STORAGE);

        PB::BufferObjectDesc cullConstantsBufferDesc;
        cullConstantsBufferDesc.m_bufferSize = sizeof(BatchCullConstants);
        cullConstantsBufferDesc.m_options = 0;
        cullConstantsBufferDesc.m_usage = PB::EBufferUsage::STORAGE | PB::EBufferUsage::COPY_DST;
        m_cullConstantsBuffer = m_renderer->AllocateBuffer(cullConstantsBufferDesc);

        PB::BufferObjectDesc drawParamsBufferDesc;
        drawParamsBufferDesc.m_bufferSize = sizeof(DrawParamsBuffer);
        drawParamsBufferDesc.m_options = 0;
        drawParamsBufferDesc.m_usage = PB::EBufferUsage::STORAGE | PB::EBufferUsage::INDIRECT_PARAMS;
        m_drawParamsBuffer = m_renderer->AllocateBuffer(drawParamsBufferDesc);

        PB::ComputePipelineDesc updatePipelineDesc;
        updatePipelineDesc.m_computeModule = Eng::Shader(m_renderer, "Shaders/GLSL/cs_populate_indices", m_allocator, true);
        m_batchUpdatePipeline = m_renderer->GetPipelineCache()->GetPipeline(updatePipelineDesc);
    }

    DrawBatch::~DrawBatch()
    {
        for (auto& info : m_dispatchInfos)
            info.m_dispatchList->RemoveDispatchObject(info.m_dispatchHandle);

        if (m_batchIndexBuffer)
        {
            m_renderer->FreeBuffer(m_batchIndexBuffer);
            m_batchIndexBuffer = nullptr;
        }
        if (m_batchIndexUploadBuffer)
        {
            m_renderer->FreeBuffer(m_batchIndexUploadBuffer);
            m_batchIndexUploadBuffer = nullptr;
        }
        if (m_indexSrcBufferBatch)
        {
            m_indexSrcBufferBatch->EndStreamingAndDelete();
            m_indexSrcBufferBatch = nullptr;
        }
        if (m_indexSrcViewBuffer)
        {
            m_renderer->FreeBuffer(m_indexSrcViewBuffer);
            m_indexSrcViewBuffer = nullptr;
        }
        
        m_renderer->FreeBuffer(m_cullConstantsBuffer);
        m_renderer->FreeBuffer(m_drawParamsBuffer);
    }

    void DrawBatch::AddToDispatchList(ObjectDispatchList* list, PB::Pipeline drawBatchPipeline, PB::BindingLayout bindings)
    {
        assert(list);
        assert(drawBatchPipeline);

        PB::DrawIndexedIndirectParams drawParams{};

        // Input bindings with draw batch bindings appended.
        PB::BindingLayout finalBindingLayout{};
        finalBindingLayout.m_uniformBuffers = bindings.m_uniformBuffers;
        finalBindingLayout.m_uniformBufferCount = bindings.m_uniformBufferCount;

        PB::ResourceView batchResources[] =
        {
            m_instanceBuffer.GetBuffer()->GetViewAsStorageBuffer()
        };
        finalBindingLayout.m_resourceCount = bindings.m_resourceCount + _countof(batchResources);

        finalBindingLayout.m_resourceViews = reinterpret_cast<PB::ResourceView*>(m_allocator->Alloc(finalBindingLayout.m_resourceCount * sizeof(PB::ResourceView)));
        if (bindings.m_resourceCount > 0)
        {
            assert(bindings.m_resourceViews);
            memcpy(finalBindingLayout.m_resourceViews, bindings.m_resourceViews, sizeof(PB::ResourceView) * bindings.m_resourceCount);
        }
        memcpy(&finalBindingLayout.m_resourceViews[bindings.m_resourceCount], batchResources, sizeof(PB::ResourceView) * _countof(batchResources));

        auto dispatchHandle = list->AddObject(drawBatchPipeline, nullptr, m_batchIndexBuffer, finalBindingLayout, drawParams, nullptr);
        m_dispatchInfos.PushBack() = { list, dispatchHandle };
        m_allocator->Free(finalBindingLayout.m_resourceViews);
    }

    void DrawBatch::DispatchFrustrumCull(PB::ICommandContext* cmdContext, PB::UniformBufferView viewConstantsView)
    {
        PB::UniformBufferView constants[]
        {
            viewConstantsView,
        };

        PB::ResourceView drawParamsView;
        PB::ResourceView drawCountView;

        PB::BufferViewDesc drawViewDesc{};
        drawViewDesc.m_buffer = m_drawParamsBuffer;
        drawViewDesc.m_offset = 0;
        drawViewDesc.m_size = sizeof(DrawParamsBuffer::m_drawIndexedParams);
        drawParamsView = m_drawParamsBuffer->GetViewAsStorageBuffer(drawViewDesc);

        drawViewDesc.m_offset = drawViewDesc.m_size;
        drawViewDesc.m_size = sizeof(DrawParamsBuffer::m_drawCount);
        drawCountView = m_drawParamsBuffer->GetViewAsStorageBuffer(drawViewDesc);

        PB::ResourceView resources[]
        {
            m_cullConstantsBuffer->GetViewAsStorageBuffer(),
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

    void DrawBatch::DrawCulledGeometry(PB::ICommandContext* cmdContext, const PB::BindingLayout& bindings)
    {
        for (StreamingBatch*& streamingBatch : m_instanceStreamingBatches)
        {
            streamingBatch->WaitStreamingComplete();
            streamingBatch->EndStreamingAndDelete();
        }
        m_instanceStreamingBatches.Clear();

        PB::BindingLayout finalBindingLayout{};
        finalBindingLayout.m_uniformBuffers = bindings.m_uniformBuffers;
        finalBindingLayout.m_uniformBufferCount = bindings.m_uniformBufferCount;

        PB::ResourceView batchResources[] =
        {
            m_instanceBuffer.GetBuffer()->GetViewAsStorageBuffer()
        };
        finalBindingLayout.m_resourceCount = _countof(batchResources);
        finalBindingLayout.m_resourceViews = batchResources;

        cmdContext->CmdBindResources(finalBindingLayout);
        cmdContext->CmdBindVertexBuffers(nullptr, 0, m_batchIndexBuffer, PB::EIndexType::PB_INDEX_TYPE_UINT32);
        cmdContext->CmdDrawIndexedIndirectCount(m_drawParamsBuffer, 0, m_drawParamsBuffer, sizeof(DrawParamsBuffer::m_drawIndexedParams), MaxDrawCount, sizeof(PB::DrawIndexedIndirectParams) - sizeof(uint32_t));
    }

    void DrawBatch::DrawAllGeometry(PB::ICommandContext* cmdContext, const PB::BindingLayout& bindings)
    {
        for (StreamingBatch*& streamingBatch : m_instanceStreamingBatches)
        {
            streamingBatch->WaitStreamingComplete();
            streamingBatch->EndStreamingAndDelete();
        }
        m_instanceStreamingBatches.Clear();

        PB::BindingLayout finalBindingLayout{};
        finalBindingLayout.m_uniformBuffers = bindings.m_uniformBuffers;
        finalBindingLayout.m_uniformBufferCount = bindings.m_uniformBufferCount;

        PB::ResourceView batchResources[] =
        {
            m_instanceBuffer.GetBuffer()->GetViewAsStorageBuffer()
        };
        finalBindingLayout.m_resourceCount = _countof(batchResources);
        finalBindingLayout.m_resourceViews = batchResources;

        cmdContext->CmdBindResources(finalBindingLayout);
        cmdContext->CmdBindVertexBuffers(nullptr, 0, m_batchIndexBuffer, PB::EIndexType::PB_INDEX_TYPE_UINT32);
        cmdContext->CmdDrawIndexed(m_batchIndexCount, m_batchInstanceCount);
    }

    void DrawBatch::CreateUpdateResources()
    {
        PB::BufferObjectDesc indexUpdateBufferDesc;
        indexUpdateBufferDesc.m_bufferSize = (sizeof(IndexUploadMetadata) * MaxObjects) + ((MaxUpdateWorkGroupsPerBatch * IndexUpdatesPerInvocation) * sizeof(uint32_t));
        indexUpdateBufferDesc.m_options = 0;
        indexUpdateBufferDesc.m_usage = PB::EBufferUsage::STORAGE | PB::EBufferUsage::COPY_DST;

        if(m_batchIndexUploadBuffer == nullptr)
            m_batchIndexUploadBuffer = m_renderer->AllocateBuffer(indexUpdateBufferDesc);

        indexUpdateBufferDesc.m_bufferSize = sizeof(PB::ResourceView) * MaxObjects;
        if(m_indexSrcViewBuffer == nullptr)
            m_indexSrcViewBuffer = m_renderer->AllocateBuffer(indexUpdateBufferDesc);

        if (m_indexSrcBufferBatch == nullptr)
        {
            m_indexSrcBufferBatch = m_streamer->AllocStreamingBatch();
            m_indexSrcBufferBatch->SetOutputBindingLocation(m_indexSrcViewBuffer, 0);
        }
    }

    void DrawBatch::AddInstance(AssetEncoder::AssetID meshID, const float* modelMatrix, const Bounds& bounds, AssetEncoder::AssetID* textureIDs, uint32_t textureCount, PB::ResourceView sampler)
    {
        assert(meshID != ~AssetEncoder::AssetID(0));
        assert(modelMatrix);
        assert(textureIDs || textureCount == 0);
        assert(textureIDs || sampler == 0);
        assert(textureCount <= DrawBatchInstanceData::MaxTextures);

        if (m_indicesUpToDate == true)
        {
            CreateUpdateResources();
            m_indicesUpToDate = false;
        }

        // Setup drawbatch index buffer update.
        {
            constexpr const PB::u32 WorkGroupIndexCount = WorkGroupSizeW * WorkGroupSizeH * IndexUpdatesPerInvocation;

            MeshCacheData meshData;
            Mesh::GetMeshData(meshID, &meshData);

            IndexUploadMetadata& entry = m_indexUploadMetadata.PushBack();
            entry.m_instanceIndex = m_batchInstanceCount;
            entry.m_firstVertex = 0;
            entry.m_startIndex = 0;
            entry.m_indexCount = PB::u32(meshData.m_indexCount);
            entry.m_workGroupCount = (entry.m_indexCount / WorkGroupIndexCount) + ((entry.m_indexCount % WorkGroupIndexCount > 0) ? 1 : 0);

            m_batchIndexCount += entry.m_indexCount;
        }

        // Setup index buffer streaming.
        {
            m_indexSrcBufferBatch->AddResource(StreamableHandle(meshID, EStreamableResourceType::MESH, StreamableHandle::EBindingType::STORAGE, 1));
        }

        // Setup instance streaming.
        {
            StreamingBatch* instanceTextureBatch = m_streamer->AllocStreamingBatch();

            for (uint32_t i = 0; i < textureCount; ++i)
            {
                instanceTextureBatch->AddResource(StreamableHandle(textureIDs[i], EStreamableResourceType::TEXTURE, StreamableHandle::EBindingType::SRV));
            }
            instanceTextureBatch->SetOutputBindingLocation(m_instanceBuffer.GetBuffer(), (sizeof(DrawBatchInstanceData) * m_batchInstanceCount) + offsetof(DrawBatchInstanceData, DrawBatchInstanceData::m_bindings));
            instanceTextureBatch->BeginStreaming();
            m_instanceStreamingBatches.PushBack(instanceTextureBatch);

            // TODO: Add a way to add stride objects to output bindings, to prevent needing another streaming batch to stride.
            StreamingBatch* instanceVertexBufferBatch = m_streamer->AllocStreamingBatch();
            instanceVertexBufferBatch->AddResource(StreamableHandle(meshID, EStreamableResourceType::MESH, StreamableHandle::EBindingType::STORAGE, 0)); // Vertex buffer only.
            instanceVertexBufferBatch->SetOutputBindingLocation(m_instanceBuffer.GetBuffer(), (sizeof(DrawBatchInstanceData) * m_batchInstanceCount) + offsetof(DrawBatchInstanceData, DrawBatchInstanceData::m_vertexBuffer));
            instanceVertexBufferBatch->BeginStreaming();
            m_instanceStreamingBatches.PushBack(instanceVertexBufferBatch);
        }

        // Upload model matrix and sampler here. The texture and vertex buffer ids will be updated when those assets are streamed in.
        DrawBatchInstanceData& instanceData = *reinterpret_cast<DrawBatchInstanceData*>(m_instanceBuffer.MapElement(m_batchInstanceCount, 0, sizeof(DrawBatchInstanceData)));
        memcpy(instanceData.m_modelMatrix, modelMatrix, sizeof(instanceData.m_modelMatrix));
        instanceData.m_sampler = sampler;

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
        m_instanceBuffer.FlushChanges();
    }

    void DrawBatch::UpdateIndices(PB::ICommandContext* cmdContext)
    {
        if (m_indicesUpToDate == true)
            return;

        assert(cmdContext);
        assert(m_indexSrcBufferBatch);

        m_indexSrcBufferBatch->BeginStreaming();
        m_indexSrcBufferBatch->WaitStreamingComplete();
        m_indexSrcBufferBatch->EndStreamingAndDelete();
        m_indexSrcBufferBatch = nullptr;

        FinalizeUpdates();

        PB::u8* data = m_batchIndexUploadBuffer->BeginPopulate();

        // Copy update data indices.

        uint32_t* indicesStart = reinterpret_cast<uint32_t*>(&data[sizeof(IndexUploadMetadata) * MaxObjects]);
        uint32_t totalWorkGroupIndex = 0;
        uint32_t totalIndexCount = 0;
        for (uint32_t i = 0; i < m_indexUploadMetadata.Count(); ++i)
        {
            auto& uploadData = m_indexUploadMetadata[i];
            if (uploadData.m_workGroupCount == WorkGroupCountInvalid) // Skip invalid entries.
                continue;

            uploadData.m_startIndex = totalIndexCount;
            totalIndexCount += uploadData.m_indexCount;

            for (uint32_t j = 0; j < uploadData.m_workGroupCount; ++j, ++totalWorkGroupIndex)
                indicesStart[totalWorkGroupIndex] = i;
        }

        // Copy update data.
        memcpy(data, m_indexUploadMetadata.Data(), sizeof(IndexUploadMetadata) * m_indexUploadMetadata.Count());

        m_batchIndexUploadBuffer->EndPopulate();

        if (m_batchIndexBuffer == nullptr)
        {
            PB::BufferObjectDesc indexBufferDesc;
            indexBufferDesc.m_bufferSize = sizeof(PB::u32) * totalIndexCount;
            indexBufferDesc.m_options = 0;
            indexBufferDesc.m_usage = PB::EBufferUsage::INDEX | PB::EBufferUsage::STORAGE;
            m_batchIndexBuffer = m_renderer->AllocateBuffer(indexBufferDesc);
        }

        // Update draw parameters with the new index count.
        PB::DrawIndexedIndirectParams drawParams{};
        drawParams.indexCount = totalIndexCount;
        drawParams.instanceCount = 1;
        for (auto& info : m_dispatchInfos)
            info.m_dispatchList->SetIndirectParams(info.m_dispatchHandle, drawParams);

        cmdContext->CmdBindPipeline(m_batchUpdatePipeline);

        PB::BindingLayout updateBindingLayout;
        updateBindingLayout.m_uniformBufferCount = 0;
        updateBindingLayout.m_uniformBuffers = nullptr;

        PB::ResourceView views[] =
        {
            m_indexSrcViewBuffer->GetViewAsStorageBuffer(),
            m_batchIndexUploadBuffer->GetViewAsStorageBuffer(),
            m_batchIndexBuffer->GetViewAsStorageBuffer()
        };
        updateBindingLayout.m_resourceCount = _countof(views);
        updateBindingLayout.m_resourceViews = views;
        cmdContext->CmdBindResources(updateBindingLayout);

        cmdContext->CmdDispatch(totalWorkGroupIndex * MinWorkGroupsPerObject, 1, 1);

        // Free src buffers as they will no longer be needed.
        m_renderer->FreeBuffer(m_batchIndexUploadBuffer);
        m_batchIndexUploadBuffer = nullptr;
        m_renderer->FreeBuffer(m_indexSrcViewBuffer);
        m_indexSrcViewBuffer = nullptr;

        m_indicesUpToDate = true;
    }

    void DrawBatch::UpdateCullParams()
    {
        BatchCullConstants* cullData = reinterpret_cast<BatchCullConstants*>(m_cullConstantsBuffer->BeginPopulate());
        memset(cullData, 0, sizeof(BatchCullConstants));

        uint32_t totalIndexCount = 0;
        for (uint32_t i = 0; i < m_indexUploadMetadata.Count(); ++i)
        {
            auto& indexData = m_indexUploadMetadata[i];
            auto& bounds = m_instanceBoundData[i];
            auto& objectCullData = cullData->m_objects[i];

            objectCullData.m_boundOrigin = bounds.m_origin;
            objectCullData.m_boundExtents = bounds.m_extents;
            objectCullData.m_drawRange[0] = totalIndexCount;
            objectCullData.m_drawRange[1] = totalIndexCount + indexData.m_indexCount;

            totalIndexCount += indexData.m_indexCount;
        }

        m_cullConstantsBuffer->EndPopulate();
    }

};