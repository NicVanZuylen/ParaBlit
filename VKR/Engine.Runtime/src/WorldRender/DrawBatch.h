#pragma once
#include "Engine.ParaBlit/IRenderer.h"
#include "Engine.ParaBlit/IResourcePool.h"
#include "Engine.ParaBlit/ICommandContext.h"
#include "ObjectDispatcher.h"
#include "ManagedInstanceBuffer.h"
#include "Bounds.h"

namespace Eng
{
	class Mesh;

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

		static constexpr const PB::u32 MaxObjects = 256;

		using DrawBatchInstance = PB::u32;

		struct CreateDesc
		{
			PB::IRenderer* m_renderer;
			CLib::Allocator* m_allocator;
		};

		DrawBatch(const CreateDesc& desc);
		~DrawBatch();

		void AddToDispatchList(ObjectDispatchList* list, PB::Pipeline drawBatchPipeline, PB::BindingLayout bindings);
		void DispatchFrustrumCull(PB::ICommandContext* cmdContext, PB::UniformBufferView viewConstantsView);
		void DrawCulledGeometry(PB::ICommandContext* cmdContext, const PB::BindingLayout& bindings);

		DrawBatchInstance AddInstance(Eng::Mesh* mesh, const float* modelMatrix, const Bounds& bounds, PB::ResourceView* textures, uint32_t textureCount, PB::ResourceView sampler);
		void UpdateInstanceModelMatrix(DrawBatchInstance instance, float* modelMatrix);

		void FinalizeUpdates();
		void UpdateIndices(PB::ICommandContext* cmdContext);
		void UpdateCullParams();

		Bounds GetBounds() { return m_bounds; }
		const PB::IBufferObject* GetDrawParametersBuffer() { return m_drawParamsBuffer; }

	private:

		// Size of compute work groups for dynamic index buffer updates.
		static constexpr const PB::u32 WorkGroupCountInvalid = ~uint32_t(0);
		static constexpr const PB::u32 WorkGroupSizeW = 1024; // Compute shader work group size.
		static constexpr const PB::u32 WorkGroupSizeH = 1;
		static constexpr const PB::u32 IndexUpdatesPerInvocation = 16; // The amount of indices updating by a single compute shader invocation.
		static constexpr const PB::u32 MinWorkGroupsPerObject = 16; // Minimum workgroups assigned to updating a single object/mesh's indices.
		static constexpr const PB::u32 MaxUpdateWorkGroupsPerBatch = 4096 * 2; // Maximum workgroups allowed in a single update dispatch.
		static constexpr const PB::u32 MaxDrawCount = 8;

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

		// Per-instance data used for GPU-driven frustrum culling.
		struct CullObjectData
		{
			glm::vec3 m_boundOrigin;
			uint32_t m_partition;
			glm::vec3 m_boundExtents;
			uint32_t m_pad0;
			glm::uvec2 m_drawRange; // Mesh index range of this object.
			glm::uvec2 m_pad1;
		};

		struct BatchCullConstants
		{
			CullObjectData m_objects[256];
		};

		struct DrawParamsBuffer
		{
			PB::DrawIndexedIndirectParams m_drawIndexedParams[MaxDrawCount];
			uint32_t m_drawCount;
		};

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
		PB::IBufferObject* m_cullConstantsBuffer = nullptr;
		PB::IBufferObject* m_drawParamsBuffer = nullptr;
		ManagedInstanceBuffer<DrawBatchInstanceData, MaxObjects, MaxObjects> m_instanceBuffer;
		uint32_t m_instanceCount = 0;
		uint32_t m_totalIndexCount = 0;
		Bounds m_bounds;
		CLib::Vector<Bounds> m_instanceBoundData;
		CLib::Vector<DynamicIndexUpdate> m_dynamicUpdateQueue;
	};

};