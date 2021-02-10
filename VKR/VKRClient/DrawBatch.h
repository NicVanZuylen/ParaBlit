#pragma once
#include "IRenderer.h"
#include "ObjectDispatcher.h"

namespace PBClient
{
	class Mesh;
}

class VertexPool
{
public:

	VertexPool(PB::IRenderer* renderer, PB::u32 poolSize, PB::u32 vertexStride);
	~VertexPool();

	PB::u8* AllocateAndBeginWrite(PB::u32 size, PB::u32& firstVertex);

	void EndWrite();

	PB::IBufferObject* GetPoolBuffer();

	PB::UniformBufferView GetView();

private:

	PB::IRenderer* m_renderer = nullptr;
	PB::IBufferObject* m_poolBuffer = nullptr;
	PB::UniformBufferView m_view = 0;
	PB::u32 m_vertexStride = 0;
	PB::u32 m_currentPoolOffset = 0;
	PB::u32 m_currentWriteBlockLength = 0;
};

class DrawBatch
{
public:

	using DrawBatchInstance = PB::u32;

	DrawBatch(PB::IRenderer* renderer, ObjectDispatchList* dispatchList, VertexPool* vertexPool, PB::u32 maxIndexCount = ((~PB::u32(0) << 8) >> 8));
	~DrawBatch();

	DrawBatchInstance AddInstance(PBClient::Mesh* mesh, float* modelMatrix, PB::ResourceView* textures, uint32_t textureCount, PB::ResourceView sampler);
	void RemoveInstance(DrawBatchInstance instance);
	void UpdateIndices(PB::ICommandContext* cmdContext);

private:

	// Size of compute work groups for dynamic index buffer updates.
	static constexpr const PB::u32 WorkGroupSizeW = 128;
	static constexpr const PB::u32 WorkGroupSizeH = 128;

	// Contains the formatted update information for a single drawbatch instance.
	struct DynamicIndexUpdate
	{
		PB::u32 m_inputIndexBufferIndex;
		PB::u32 m_instanceIndex;
		PB::u32 m_firstVertex;
		PB::u32 m_startIndex;
		PB::u32 m_indexCount;
		PB::u32 m_workGroupCount;
	};

	struct DrawBatchInstanceData
	{
		static constexpr const PB::u32 MaxTextures = 7;

		// Per-object model transformation matrix.
		float m_modelMatrix[16];

		// Textures are kept in instance data as a way to pass on per-object textures to the fragment shader via stage IO.
		PB::ResourceView m_textures[MaxTextures];
		PB::ResourceView m_sampler;
	};
	static_assert(sizeof(DrawBatchInstanceData) % 16 == 0);

	inline void AddToDispatchList(VertexPool* vertexPool);

	inline DrawBatchInstanceData* GetInstanceData()
	{
		if (!m_mappedInstanceData)
			m_mappedInstanceData = reinterpret_cast<DrawBatchInstanceData*>(m_instanceBuffer->BeginPopulate());
		return m_mappedInstanceData;
	}

	ObjectDispatchList* m_dispatchList = nullptr;
	ObjectDispatchList::DispatchObjectHandle m_dispatchHandle;
	PB::Pipeline m_batchPipeline = 0;
	PB::IRenderer* m_renderer = nullptr;
	PB::IBufferObject* m_dynamicIndexUpdateBuffer = nullptr;
	PB::IBufferObject* m_dynamicIndexBuffer = nullptr;
	PB::IBufferObject* m_instanceBuffer = nullptr;
	DrawBatchInstanceData* m_mappedInstanceData = nullptr;
	uint32_t m_instanceCount = 0;
	uint32_t m_totalUpdateWorkgroupCount = 0;
	CLib::Vector<DynamicIndexUpdate> m_dynamicUpdateQueue;
	DynamicIndexUpdate* m_updateEntryUsingFinalInstance = nullptr;
	CLib::Vector<uint32_t> m_dynamicUpdateQueueFreeList;
};