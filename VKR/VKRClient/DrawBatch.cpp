#include "DrawBatch.h"
#include "Mesh.h"
#include "Shader.h"

VertexPool::VertexPool(PB::IRenderer* renderer, PB::u32 poolSize, PB::u32 vertexStride)
{
    m_renderer = renderer;
    m_vertexStride = vertexStride;

    PB::BufferObjectDesc poolBufferDesc;
    poolBufferDesc.m_bufferSize = poolSize;
    poolBufferDesc.m_usage = PB::EBufferUsage::COPY_DST | PB::EBufferUsage::VERTEX | PB::EBufferUsage::STORAGE;
    poolBufferDesc.m_options = 0;
    m_poolBuffer = m_renderer->AllocateBuffer(poolBufferDesc);

    m_storageBufferView = m_poolBuffer->GetViewAsStorageBuffer();
}

VertexPool::~VertexPool()
{
    m_renderer->FreeBuffer(m_poolBuffer);
    m_poolBuffer = nullptr;
    m_storageBufferView = 0;

    m_currentPoolOffset = 0;
    m_currentWriteBlockLength = 0;
}

PB::u8* VertexPool::AllocateAndBeginWrite(PB::u32 size, PB::u32& firstVertex)
{
    firstVertex = m_currentPoolOffset / m_vertexStride;
    m_currentWriteBlockLength = size;
    return m_poolBuffer->BeginPopulate(size);
}

void VertexPool::EndWrite()
{
    m_poolBuffer->EndPopulate(m_currentPoolOffset);
    m_currentPoolOffset += m_currentWriteBlockLength;
    m_currentWriteBlockLength = 0;
}

PB::IBufferObject* VertexPool::GetPoolBuffer()
{
    return m_poolBuffer;
}

PB::ResourceView VertexPool::GetViewAsStorageBuffer()
{
    return m_storageBufferView;
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

    PB::BufferObjectDesc instanceBufferDesc;
    instanceBufferDesc.m_bufferSize = sizeof(DrawBatchInstanceData) * MaxObjects;
    instanceBufferDesc.m_options = 0;
    instanceBufferDesc.m_usage = PB::EBufferUsage::STORAGE | PB::EBufferUsage::VERTEX | PB::EBufferUsage::COPY_DST;
    m_instanceBuffer = renderer->AllocateBuffer(instanceBufferDesc);

    PB::ComputePipelineDesc updatePipelineDesc;
    updatePipelineDesc.m_computeModule = PBClient::Shader(m_renderer, "TestAssets/Shaders/SPIR-V/cs_populate_indices.spv", allocator);
    m_batchUpdatePipeline = m_renderer->GetPipelineCache()->GetPipeline(updatePipelineDesc);
}

DrawBatch::~DrawBatch()
{
    m_dispatchList->RemoveDispatchObject(m_dispatchHandle);

    m_renderer->FreeBuffer(m_instanceBuffer);
    m_renderer->FreeBuffer(m_dynamicIndexBuffer);
    m_renderer->FreeBuffer(m_dynamicIndexUpdateBuffer);
}

void DrawBatch::AddToDispatchList(ObjectDispatchList* list, PB::Pipeline drawBatchPipeline, PB::BindingLayout bindings)
{
    assert(list);
    assert(drawBatchPipeline);

    m_dispatchList = list;

    PB::DrawIndexedIndirectParams drawParams{};

    // Input bindings with draw batch bindings appended.
    PB::BindingLayout finalBindingLayout{};
    finalBindingLayout.m_uniformBuffers = bindings.m_uniformBuffers;
    finalBindingLayout.m_uniformBufferCount = bindings.m_uniformBufferCount;

    PB::ResourceView batchResources[] =
    {
        m_vertexPool->GetViewAsStorageBuffer(),
        m_instanceBuffer->GetViewAsStorageBuffer()
    };
    finalBindingLayout.m_resourceCount = bindings.m_resourceCount + _countof(batchResources);

    finalBindingLayout.m_resourceViews = reinterpret_cast<PB::ResourceView*>(m_allocator->Alloc(finalBindingLayout.m_resourceCount * sizeof(PB::ResourceView)));
    if (bindings.m_resourceCount > 0)
    {
        assert(bindings.m_resourceViews);
        memcpy(finalBindingLayout.m_resourceViews, bindings.m_resourceViews, sizeof(PB::ResourceView) * bindings.m_resourceCount);
    }
    memcpy(&finalBindingLayout.m_resourceViews[bindings.m_resourceCount], batchResources, sizeof(PB::ResourceView) * _countof(batchResources));

    m_dispatchHandle = m_dispatchList->AddObject(drawBatchPipeline, nullptr, m_dynamicIndexBuffer, finalBindingLayout, drawParams, nullptr);
    m_allocator->Free(finalBindingLayout.m_resourceViews);
}

DrawBatch::DrawBatchInstance DrawBatch::AddInstance(PBClient::Mesh* mesh, float* modelMatrix, PB::ResourceView* textures, uint32_t textureCount, PB::ResourceView sampler)
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
        m_dynamicUpdateQueue.PushBack();
    DynamicIndexUpdate& entry = m_dynamicUpdateQueue[freeIndex];

    entry.m_inputIndexBufferIndex = mesh->GetIndexBuffer()->GetViewAsStorageBuffer();
    if(freeIndex != m_dynamicUpdateQueue.Count())
        entry.m_instanceIndex = m_instanceCount++;
    entry.m_firstVertex = mesh->FirstVertex();
    entry.m_startIndex = 0;
    entry.m_indexCount = mesh->IndexCount();
    entry.m_workGroupCount = (entry.m_indexCount / WorkGroupIndexCount) + ((entry.m_indexCount % WorkGroupIndexCount > 0) ? 1 : 0);

    // Account for minimum work group count. This means workGroupCount isn't the actual work group count, but the amount of indices used for these work groups.
    entry.m_workGroupCount = (entry.m_workGroupCount / MinWorkGroupsPerObject) + (entry.m_workGroupCount % MinWorkGroupsPerObject ? 1 : 0);

    DrawBatchInstanceData& instanceData = GetInstanceData()[entry.m_instanceIndex];
    memcpy(instanceData.m_modelMatrix, modelMatrix, sizeof(instanceData.m_modelMatrix));
    memcpy(instanceData.m_textures, textures, textureCount * sizeof(PB::ResourceView));
    instanceData.m_sampler = sampler;

    return freeIndex;
}

void DrawBatch::UpdateInstanceModelMatrix(DrawBatchInstance instance, float* modelMatrix)
{
    assert(modelMatrix);

    DrawBatchInstanceData& data = GetInstanceData()[m_dynamicUpdateQueue[instance].m_instanceIndex];
    memcpy(data.m_modelMatrix, modelMatrix, sizeof(float) * 16);
}

void DrawBatch::RemoveInstance(DrawBatchInstance instance)
{
    DynamicIndexUpdate& entryToRemove = m_dynamicUpdateQueue[instance];
    entryToRemove.m_workGroupCount = WorkGroupCountInvalid; // Invalidate instance.
    
    if (instance == m_dynamicUpdateQueue.Count() - 1)
        --m_instanceCount;
    else
        m_dynamicUpdateQueueFreeList.PushBack(instance);
}

void DrawBatch::FinalizeUpdates()
{
    if (m_mappedInstanceData == nullptr)
        return;

    m_instanceBuffer->EndPopulate();
    m_mappedInstanceData = nullptr;
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
    m_dispatchList->SetIndirectParams(m_dispatchHandle, drawParams);

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

void DrawBatch::AddToDispatchList(PB::UniformBufferView mvpView)
{
    
}
