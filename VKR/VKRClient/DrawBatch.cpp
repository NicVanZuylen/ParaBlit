#include "DrawBatch.h"
#include "Mesh.h"

VertexPool::VertexPool(PB::IRenderer* renderer, PB::u32 poolSize, PB::u32 vertexStride)
{
    m_renderer = renderer;
    m_vertexStride = vertexStride;

    PB::BufferObjectDesc poolBufferDesc;
    poolBufferDesc.m_bufferSize = poolSize;
    poolBufferDesc.m_usage = PB::EBufferUsage::COPY_DST | PB::EBufferUsage::UNIFORM; // TODO: Analyse the performance of storage buffers (read-only if possible) vs uniform. Use the faster approach.
    poolBufferDesc.m_options = 0;
    m_poolBuffer = m_renderer->AllocateBuffer(poolBufferDesc);

    m_view = m_poolBuffer->GetViewAsUniformBuffer();
}

VertexPool::~VertexPool()
{
    m_renderer->FreeBuffer(m_poolBuffer);
    m_poolBuffer = nullptr;
    m_view = 0;

    m_currentPoolOffset = 0;
    m_currentWriteBlockLength = 0;
}

PB::u8* VertexPool::AllocateAndBeginWrite(PB::u32 size, PB::u32& firstVertex)
{
    firstVertex = m_currentPoolOffset / m_vertexStride;
    m_currentWriteBlockLength = size;
    return m_poolBuffer->BeginPopulate();
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

PB::UniformBufferView VertexPool::GetView()
{
    return m_view;
}

DrawBatch::DrawBatch(PB::IRenderer* renderer, ObjectDispatchList* dispatchList, VertexPool* vertexPool, PB::u32 maxIndexCount)
{
    m_renderer = renderer;
    m_dispatchList = dispatchList;

    PB::BufferObjectDesc indexBufferDesc;
    indexBufferDesc.m_bufferSize = sizeof(PB::u32) * maxIndexCount;
    indexBufferDesc.m_options = 0;
    indexBufferDesc.m_usage = PB::EBufferUsage::INDEX | PB::EBufferUsage::STORAGE;
    m_dynamicIndexBuffer = renderer->AllocateBuffer(indexBufferDesc);

    PB::BufferObjectDesc instanceBufferDesc;
    instanceBufferDesc.m_bufferSize = 0;
    instanceBufferDesc.m_options = 0;
    instanceBufferDesc.m_usage = PB::EBufferUsage::UNIFORM | PB::EBufferUsage::COPY_DST;
    m_instanceBuffer = renderer->AllocateBuffer(instanceBufferDesc);

    AddToDispatchList(vertexPool);
}

DrawBatch::~DrawBatch()
{
    m_dispatchList->RemoveDispatchObject(m_dispatchHandle);

    m_renderer->FreeBuffer(m_instanceBuffer);
    m_renderer->FreeBuffer(m_dynamicIndexBuffer);
}

DrawBatch::DrawBatchInstance DrawBatch::AddInstance(PBClient::Mesh* mesh, float* modelMatrix, PB::ResourceView* textures, uint32_t textureCount, PB::ResourceView sampler)
{
    constexpr const PB::u32 WorkGroupInvocationCount = 64 * 64;

    uint32_t freeIndex = m_dynamicUpdateQueue.Count();
    if (m_dynamicUpdateQueueFreeList.Count() > 0)
        freeIndex = m_dynamicUpdateQueueFreeList.Back();
    else
        m_dynamicUpdateQueue.PushBack();
    DynamicIndexUpdate& entry = m_dynamicUpdateQueue[freeIndex];
    m_updateEntryUsingFinalInstance = &entry;

    entry.m_inputIndexBufferIndex = mesh->GetIndexBuffer()->GetViewAsStorageBuffer();
    entry.m_instanceIndex = m_instanceCount++;
    entry.m_firstVertex = mesh->FirstVertex();
    entry.m_startIndex = 0;
    entry.m_indexCount = mesh->IndexCount();
    m_totalUpdateWorkgroupCount += entry.m_workGroupCount = (entry.m_indexCount / WorkGroupInvocationCount) + (entry.m_indexCount % WorkGroupInvocationCount > 0 ? 1 : 0);

    DrawBatchInstanceData& instanceData = GetInstanceData()[entry.m_instanceIndex];
    memcpy(instanceData.m_modelMatrix, modelMatrix, sizeof(instanceData.m_modelMatrix));
    memcpy(instanceData.m_textures, textures, textureCount * sizeof(PB::ResourceView));
    instanceData.m_sampler = sampler;

    return freeIndex;
}

void DrawBatch::RemoveInstance(DrawBatchInstance instance)
{
    DynamicIndexUpdate& entryToRemove = m_dynamicUpdateQueue[instance];
    
    // Swap the entry-to-remove's instance with the final instance in the instance buffer to eliminate the gap in valid instance data.
    DrawBatchInstanceData* instanceData = GetInstanceData();
    memcpy(&instanceData[entryToRemove.m_instanceIndex], &instanceData[m_instanceCount - 1], sizeof(DrawBatchInstanceData));
    --m_instanceCount; // Decrement count to remove the duplicate instance.

    // Remove entry workgroups from total count.
    m_totalUpdateWorkgroupCount -= entryToRemove.m_workGroupCount;

    // Update the instance index in the entry who owns the final instance.
    m_updateEntryUsingFinalInstance->m_instanceIndex = entryToRemove.m_instanceIndex;
    m_updateEntryUsingFinalInstance = nullptr;

    m_dynamicUpdateQueueFreeList.PushBack(instance);
}

void DrawBatch::UpdateIndices(PB::ICommandContext* cmdContext)
{
    PB::u8* data = m_dynamicIndexUpdateBuffer->BeginPopulate();
    memcpy(data, m_dynamicUpdateQueue.Data(), sizeof(DynamicIndexUpdate) * m_dynamicUpdateQueue.Count());
    m_dynamicIndexBuffer->EndPopulate();

    m_dynamicUpdateQueue.Clear();

    cmdContext->CmdBindPipeline(m_batchPipeline);

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

    cmdContext->CmdDispatch(m_totalUpdateWorkgroupCount, 1, 1);
}

void DrawBatch::AddToDispatchList(VertexPool* vertexPool)
{
    PB::DrawIndexedIndirectParams drawParams{};
    PB::BindingLayout bindingLayout{};

    PB::UniformBufferView uniformBuffers[] = { vertexPool->GetView(), m_instanceBuffer->GetViewAsUniformBuffer() };
    bindingLayout.m_uniformBufferCount = _countof(uniformBuffers);
    bindingLayout.m_uniformBuffers = uniformBuffers;

    PB::SamplerDesc colorSamplerDesc;
    colorSamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
    colorSamplerDesc.m_mipFilter = PB::ESamplerFilter::NEAREST;
    colorSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::REPEAT;

    PB::ResourceView resources[] =
    {
        m_renderer->GetSampler(colorSamplerDesc),
    };
    bindingLayout.m_resourceCount = _countof(resources);
    bindingLayout.m_resourceViews = resources;

    m_dispatchHandle = m_dispatchList->AddObject(m_batchPipeline, nullptr, m_dynamicIndexBuffer, bindingLayout, drawParams, m_instanceBuffer);
}
