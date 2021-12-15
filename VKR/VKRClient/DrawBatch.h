#pragma once
#include "IRenderer.h"
#include "IResourcePool.h"
#include "ObjectDispatcher.h"
#include "ManagedInstanceBuffer.h"

namespace PBClient
{
	class Mesh;
}

class VertexPool
{
public:

	VertexPool(PB::IRenderer* renderer, PB::u32 poolSize, PB::u32 vertexStride);
	~VertexPool();

	void GetNextVertexOffset(PB::u32 size, PB::u32& firstVertex);

	PB::IResourcePool* GetPool();

private:

	PB::IRenderer* m_renderer = nullptr;
	PB::IResourcePool* m_pool = nullptr;
	PB::u32 m_vertexStride = 0;
	PB::u32 m_currentPoolOffset = 0;
};

class DrawBatch
{
public:

	using DrawBatchInstance = PB::u32;

	DrawBatch(PB::IRenderer* renderer, CLib::Allocator* allocator, VertexPool* vertexPool, PB::u32 maxIndexCount = ((~PB::u32(0) << 8) >> 8));
	~DrawBatch();

	void AddToDispatchList(ObjectDispatchList* list, PB::Pipeline drawBatchPipeline, PB::BindingLayout bindings);

	DrawBatchInstance AddInstance(PBClient::Mesh* mesh, float* modelMatrix, PB::ResourceView* textures, uint32_t textureCount, PB::ResourceView sampler);
	void UpdateInstanceModelMatrix(DrawBatchInstance instance, float* modelMatrix);
	void RemoveInstance(DrawBatchInstance instance);

	void FinalizeUpdates();
	void UpdateIndices(PB::ICommandContext* cmdContext);

private:

	// Size of compute work groups for dynamic index buffer updates.
	static constexpr const PB::u32 WorkGroupCountInvalid = ~uint32_t(0);
	static constexpr const PB::u32 WorkGroupSizeW = 1024; // Compute shader work group size.
	static constexpr const PB::u32 WorkGroupSizeH = 1;
	static constexpr const PB::u32 IndexUpdatesPerInvocation = 16; // The amount of indices updating by a single compute shader invocation.
	static constexpr const PB::u32 MinWorkGroupsPerObject = 16; // Minimum workgroups assigned to updating a single object/mesh's indices.
	static constexpr const PB::u32 MaxUpdateWorkGroupsPerBatch = 4096 * 2; // Maximum workgroups allowed in a single update dispatch.
	static constexpr const PB::u32 MaxObjects = 256;

	// Contains the formatted update information for a single drawbatch instance.
	struct DynamicIndexUpdate
	{
		PB::u32 m_inputIndexBufferIndex;
		PB::u32 m_instanceIndex;
		PB::u32 m_firstVertex;
		PB::u32 m_startIndex;
		PB::u32 m_indexCount;
		PB::u32 m_workGroupCount;
		PB::u32 pad[2];
	};

	struct DrawBatchInstanceData
	{
		static constexpr const PB::u32 MaxTextures = 6;

		// Per-object model transformation matrix.
		float m_modelMatrix[16];

		// Textures are kept in instance data as a way to pass on per-object textures to the fragment shader via stage IO.
		PB::ResourceView m_textures[MaxTextures];
		PB::ResourceView m_vertexBuffer;
		PB::ResourceView m_sampler;
	};
	static_assert(sizeof(DrawBatchInstanceData) % 16 == 0);

	VertexPool* m_vertexPool = nullptr;
	struct DispatchInfo
	{
		ObjectDispatchList* m_dispatchList = nullptr;
		ObjectDispatchList::DispatchObjectHandle m_dispatchHandle;
	};
	CLib::Vector<DispatchInfo> m_dispatchInfos;
	PB::Pipeline m_batchUpdatePipeline = 0;
	PB::IRenderer* m_renderer = nullptr;
	CLib::Allocator* m_allocator = nullptr;
	PB::IBufferObject* m_dynamicIndexUpdateBuffer = nullptr;
	PB::IBufferObject* m_dynamicIndexBuffer = nullptr;
	ManagedInstanceBuffer<DrawBatchInstanceData, MaxObjects, MaxObjects> m_instanceBuffer;
	uint32_t m_instanceCount = 0;
	CLib::Vector<uint32_t> m_dynamicUpdateQueueFreeList;
	CLib::Vector<DynamicIndexUpdate> m_dynamicUpdateQueue;
};