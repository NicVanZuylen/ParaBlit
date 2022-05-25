#include "DrawBatch.h"
#include "IBufferObject.h"
#include "Mesh.h"
#include "Shader.h"

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

DrawBatch::DrawBatch(PB::IRenderer* renderer, CLib::Allocator* allocator, VertexPool* vertexPool, PB::u32 maxIndexCount)
{
    m_renderer = renderer;
    m_allocator = allocator;
    m_vertexPool = vertexPool;

    PB::BufferObjectDesc indexBufferDesc;
    indexBufferDesc.m_bufferSize = sizeof(PB::u32) * maxIndexCount;
    indexBufferDesc.m_options = 0;
    indexBufferDesc.m_usage = PB::EBufferUsage::INDEX | PB::EBufferUsage::STORAGE;
    m_dynamicIndexBuffer = renderer->AllocateBuffer(indexBufferDesc);

    PB::BufferObjectDesc indexUpdateBufferDesc;
    indexUpdateBufferDesc.m_bufferSize = (sizeof(DynamicIndexUpdate) * MaxObjects) + ((MaxUpdateWorkGroupsPerBatch / IndexUpdatesPerInvocation) * sizeof(uint32_t));
    indexUpdateBufferDesc.m_options = 0;
    indexUpdateBufferDesc.m_usage = PB::EBufferUsage::STORAGE | PB::EBufferUsage::COPY_DST;
    m_dynamicIndexUpdateBuffer = renderer->AllocateBuffer(indexUpdateBufferDesc);

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
    updatePipelineDesc.m_computeModule = PBClient::Shader(m_renderer, "Shaders/GLSL/cs_populate_indices", allocator, true);
    m_batchUpdatePipeline = m_renderer->GetPipelineCache()->GetPipeline(updatePipelineDesc);
}

DrawBatch::~DrawBatch()
{
    for (auto& info : m_dispatchInfos)
        info.m_dispatchList->RemoveDispatchObject(info.m_dispatchHandle);

    m_renderer->FreeBuffer(m_dynamicIndexBuffer);
    m_renderer->FreeBuffer(m_dynamicIndexUpdateBuffer);

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

    auto dispatchHandle = list->AddObject(drawBatchPipeline, nullptr, m_dynamicIndexBuffer, finalBindingLayout, drawParams, nullptr);
    m_dispatchInfos.PushBack() = { list, dispatchHandle };
    m_allocator->Free(finalBindingLayout.m_resourceViews);
}

void DrawBatch::DispatchFrustrumCull(PB::ICommandContext* cmdContext, PB::UniformBufferView viewConstantsView)
{
    PB::ComputePipelineDesc cullPipelineDesc{};
    cullPipelineDesc.m_computeModule = PBClient::Shader(m_renderer, "Shaders/GLSL/cs_drawbatch_cull", m_allocator, true).GetModule();

    PB::Pipeline cullPipeline = m_renderer->GetPipelineCache()->GetPipeline(cullPipelineDesc);

    cmdContext->CmdBindPipeline(cullPipeline);

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

    cmdContext->CmdDrawIndirectBarrier(&m_drawParamsBuffer, 1);
}

void DrawBatch::DrawCulledGeometry(PB::ICommandContext* cmdContext, PB::Pipeline drawBatchPipeline, PB::BindingLayout bindings)
{
    //cmdContext->CmdBindPipeline(drawBatchPipeline);

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
    cmdContext->CmdBindVertexBuffers(nullptr, 0, m_dynamicIndexBuffer, PB::EIndexType::PB_INDEX_TYPE_UINT32);
    cmdContext->CmdDrawIndexedIndirectCount(m_drawParamsBuffer, 0, m_drawParamsBuffer, sizeof(DrawParamsBuffer::m_drawIndexedParams), MaxDrawCount, sizeof(PB::DrawIndexedIndirectParams) - sizeof(uint32_t));
}

DrawBatch::DrawBatchInstance DrawBatch::AddInstance(PBClient::Mesh* mesh, float* modelMatrix, glm::vec3 boundOrigin, glm::vec3 boundExtents, PB::ResourceView* textures, uint32_t textureCount, PB::ResourceView sampler)
{
    assert(mesh);
    assert(mesh->GetVertexPool() == m_vertexPool && "Mesh must be allocated from the same vertex pool assigned to this draw batch.");
    assert(modelMatrix);
    assert(textures || textureCount == 0);
    assert(textures || sampler == 0);

    constexpr const PB::u32 WorkGroupIndexCount = WorkGroupSizeW * WorkGroupSizeH * IndexUpdatesPerInvocation;

    uint32_t freeIndex = m_dynamicUpdateQueue.Count();
    if (m_dynamicUpdateQueueFreeList.Count() > 0)
        freeIndex = m_dynamicUpdateQueueFreeList.Back();
    else
    {
        m_dynamicUpdateQueue.PushBack();
        m_instanceCullData.PushBack();
    }
    DynamicIndexUpdate& entry = m_dynamicUpdateQueue[freeIndex];

    entry.m_inputIndexBufferIndex = mesh->GetIndexBuffer()->GetViewAsStorageBuffer();
    if(freeIndex != m_dynamicUpdateQueue.Count())
        entry.m_instanceIndex = m_instanceCount++;
    entry.m_firstVertex = 0;
    entry.m_startIndex = 0;
    entry.m_indexCount = mesh->IndexCount();
    entry.m_workGroupCount = (entry.m_indexCount / WorkGroupIndexCount) + ((entry.m_indexCount % WorkGroupIndexCount > 0) ? 1 : 0);

    // Account for minimum work group count. This means workGroupCount isn't the actual work group count, but the amount of indices used for these work groups.
    entry.m_workGroupCount = (entry.m_workGroupCount / MinWorkGroupsPerObject) + (entry.m_workGroupCount % MinWorkGroupsPerObject ? 1 : 0);

    DrawBatchInstanceData& instanceData = *reinterpret_cast<DrawBatchInstanceData*>(m_instanceBuffer.MapElement(entry.m_instanceIndex, 0, sizeof(DrawBatchInstanceData)));
    memcpy(instanceData.m_modelMatrix, modelMatrix, sizeof(instanceData.m_modelMatrix));
    memcpy(instanceData.m_textures, textures, textureCount * sizeof(PB::ResourceView));
    instanceData.m_vertexBuffer = mesh->GetVertexBuffer()->GetViewAsStorageBuffer();
    instanceData.m_sampler = sampler;

    LocalObjectCullData& cullData = m_instanceCullData[freeIndex];
    cullData.m_boundOrigin = boundOrigin;
    cullData.m_boundExtents = boundExtents;

    return freeIndex;
}

void DrawBatch::UpdateInstanceModelMatrix(DrawBatchInstance instance, float* modelMatrix)
{
    assert(modelMatrix);

    PB::u8* data = m_instanceBuffer.MapElement(m_dynamicUpdateQueue[instance].m_instanceIndex, 0, sizeof(float) * 16);
    memcpy(data, modelMatrix, sizeof(float) * 16);
}

void DrawBatch::RemoveInstance(DrawBatchInstance instance)
{
    DynamicIndexUpdate& entryToRemove = m_dynamicUpdateQueue[instance];
    entryToRemove.m_workGroupCount = WorkGroupCountInvalid; // Invalidate instance.
    
    if (instance == m_dynamicUpdateQueue.Count() - 1)
    {
        --m_instanceCount;
        m_dynamicUpdateQueue.PopBack();
        m_instanceCullData.PopBack();
    }
    else
        m_dynamicUpdateQueueFreeList.PushBack(instance);
}

void DrawBatch::FinalizeUpdates()
{
    m_instanceBuffer.FlushChanges();
}

void DrawBatch::UpdateIndices(PB::ICommandContext* cmdContext)
{
    assert(cmdContext);

    FinalizeUpdates();

    PB::u8* data = m_dynamicIndexUpdateBuffer->BeginPopulate();

    // Copy update data indices.

    uint32_t* indicesStart = reinterpret_cast<uint32_t*>(&data[sizeof(DynamicIndexUpdate) * MaxObjects]);
    uint32_t totalWorkGroupIndex = 0;
    uint32_t totalIndexCount = 0;
    for (uint32_t i = 0; i < m_dynamicUpdateQueue.Count(); ++i)
    {
        auto& updateData = m_dynamicUpdateQueue[i];
        if (updateData.m_workGroupCount == WorkGroupCountInvalid) // Skip invalid entries.
            continue;

        updateData.m_startIndex = totalIndexCount;
        totalIndexCount += updateData.m_indexCount;

        for (uint32_t j = 0; j < updateData.m_workGroupCount; ++j, ++totalWorkGroupIndex)
            indicesStart[totalWorkGroupIndex] = i;
    }

    // Copy update data.
    memcpy(data, m_dynamicUpdateQueue.Data(), sizeof(DynamicIndexUpdate) * m_dynamicUpdateQueue.Count());

    m_dynamicIndexUpdateBuffer->EndPopulate();

    // Update draw parameters with the new index count.
    PB::DrawIndexedIndirectParams drawParams{};
    drawParams.indexCount = totalIndexCount;
    drawParams.instanceCount = 1;
    for(auto& info : m_dispatchInfos)
        info.m_dispatchList->SetIndirectParams(info.m_dispatchHandle, drawParams);

    cmdContext->CmdBindPipeline(m_batchUpdatePipeline);

    PB::BindingLayout updateBindingLayout;
    updateBindingLayout.m_uniformBufferCount = 0;
    updateBindingLayout.m_uniformBuffers = nullptr;

    PB::ResourceView views[] =
    {
        m_dynamicIndexUpdateBuffer->GetViewAsStorageBuffer(),
        m_dynamicIndexBuffer->GetViewAsStorageBuffer()
    };
    updateBindingLayout.m_resourceCount = _countof(views);
    updateBindingLayout.m_resourceViews = views;
    cmdContext->CmdBindResources(updateBindingLayout);

    cmdContext->CmdDispatch(totalWorkGroupIndex * MinWorkGroupsPerObject, 1, 1);
}

void DrawBatch::UpdateCullParams()
{
    BatchCullConstants* cullData = reinterpret_cast<BatchCullConstants*>(m_cullConstantsBuffer->BeginPopulate());
    cullData->m_validObjectCount = m_instanceCount;

    uint32_t partitionSize = uint32_t(glm::ceil(glm::max(float(m_instanceCount) / MaxDrawCount, 1.0f)));
    uint32_t currentPartition = 0;
    uint32_t localPartitionIndex = 0;
    uint32_t partitionOffset = 0;
    uint32_t totalIndexCount = 0;

    cullData->m_partitionRanges[currentPartition][0] = 0;
    for (uint32_t i = 0; i < m_dynamicUpdateQueue.Count(); ++i)
    {
        auto& indexData = m_dynamicUpdateQueue[i];
        auto& localCullData = m_instanceCullData[i];
        auto& objectCullData = cullData->m_objects[i];

        objectCullData.m_boundOrigin = localCullData.m_boundOrigin;
        objectCullData.m_boundExtents = localCullData.m_boundExtents;
        objectCullData.m_drawRange[0] = totalIndexCount;
        objectCullData.m_drawRange[1] = totalIndexCount + indexData.m_indexCount;

        totalIndexCount += indexData.m_indexCount;

        if (localPartitionIndex == partitionSize)
        {
            cullData->m_partitionRanges[currentPartition][1] = partitionOffset + localPartitionIndex;
            ++currentPartition;
            cullData->m_partitionRanges[currentPartition][0] = partitionOffset + localPartitionIndex;
            partitionOffset += localPartitionIndex;
            localPartitionIndex = 0;
        }
        objectCullData.m_partition = currentPartition;

        ++localPartitionIndex;
    }
    cullData->m_partitionRanges[currentPartition][1] = partitionOffset + localPartitionIndex;

    m_cullConstantsBuffer->EndPopulate();
}
