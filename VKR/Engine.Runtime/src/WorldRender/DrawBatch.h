#pragma once
#include "Engine.ParaBlit/IRenderer.h"
#include "Engine.ParaBlit/IResourcePool.h"
#include "Engine.ParaBlit/ICommandContext.h"
#include "Engine.AssetEncoder/AssetDatabaseReader.h"
#include "ObjectDispatcher.h"
#include "ManagedInstanceBuffer.h"
#include "Bounds.h"

namespace Eng
{
	class Mesh;
	class AssetStreamer;
	class StreamingBatch;

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

		struct CreateDesc
		{
			PB::IRenderer* m_renderer;
			CLib::Allocator* m_allocator;
			AssetStreamer* m_streamer;
		};

		DrawBatch(const CreateDesc& desc);
		~DrawBatch();

		void AddToDispatchList(ObjectDispatchList* list, PB::Pipeline drawBatchPipeline, PB::BindingLayout bindings);
		void DispatchFrustrumCull(PB::ICommandContext* cmdContext, PB::UniformBufferView viewConstantsView, bool cullTasks);
		void DrawCulledGeometry(PB::ICommandContext* cmdContext, const PB::BindingLayout& bindings);
		void DrawAllGeometry(PB::ICommandContext* cmdContext, const PB::BindingLayout& bindings);
		void EXPERIMENTAL_DrawAllMeshShader(PB::ICommandContext* cmdContext, const PB::BindingLayout& bindings, PB::UniformBufferView viewConstantsView);

		void AddInstance(AssetEncoder::AssetID meshID, const float* modelMatrix, const Bounds& bounds, AssetEncoder::AssetID* textureIDs, uint32_t textureCount, PB::ResourceView sampler);

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
		static constexpr const PB::u32 MaxUpdateWorkGroupsPerBatch = 4096; // Maximum workgroups allowed in a single update dispatch.
		static constexpr const PB::u32 MaxDrawCount = 8;
		static constexpr const PB::u32 MeshletIndexCount = 32 * 3;
		static constexpr const PB::u32 MeshletsPerUploadWorkgroup = 128;

		// Contains the formatted update information for a single drawbatch instance.
		struct IndexUploadMetadata
		{
			PB::u32 m_instanceIndex;
			PB::u32 m_firstVertex;
			PB::u32 m_startIndex;
			PB::u32 m_indexCount;
			PB::u32 m_workGroupCount;
			PB::u32 m_pad[1];
		};

		struct MeshletUploadMetadata
		{
			float m_meshletTransform[16];
			uint32_t m_firstWorkgroup;
			uint32_t m_workgroupCount;
			uint32_t m_meshletCount;
			uint32_t m_startIndex;
		};

		struct DrawBatchInstanceData
		{
			static constexpr const PB::u32 MaxTextures = 6;

			// Per-object model transformation matrix.
			float m_modelMatrix[16];

			// Textures are kept in instance data as a way to pass on per-object textures to the fragment shader via stage IO.
			union
			{
				struct
				{
					PB::ResourceView m_textures[MaxTextures];
					PB::ResourceView m_vertexBuffer;
					PB::ResourceView m_sampler;
				};
				PB::ResourceView m_bindings[MaxTextures + 2];
			};
		};
		static_assert(sizeof(DrawBatchInstanceData) % 16 == 0);

		struct MeshletBoundData
		{
			glm::vec4 m_normal;
			glm::vec3 m_origin;
			uint32_t m_index;
			glm::vec3 m_extents;
			uint32_t m_normalDataPacked;
		};

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

		struct MeshletDrawParamsBuffer
		{
			PB::DrawMeshTasksIndirectParams m_drawMeshTasksParams[MaxDrawCount];
			uint32_t m_drawCount;
		};

		struct MeshletDrawRangesBuffer
		{
			glm::uvec2 m_drawRanges[MaxDrawCount];
		};

		struct DispatchInfo
		{
			ObjectDispatchList* m_dispatchList = nullptr;
			ObjectDispatchList::DispatchObjectHandle m_dispatchHandle;
		};

		void CreateUpdateResources();

		CLib::Vector<DispatchInfo> m_dispatchInfos;

		PB::IRenderer* m_renderer = nullptr;
		CLib::Allocator* m_allocator = nullptr;
		AssetStreamer* m_streamer = nullptr;

		// ----------------------------------------------------------------------------------------
		// Compute uploads

		PB::Pipeline m_batchIndexUpdatePipeline = 0;
		PB::Pipeline m_batchMeshletUpdatePipeline = 0;

		// Indices
		PB::IBufferObject* m_batchIndexUploadBuffer = nullptr;
		PB::IBufferObject* m_indexSrcViewBuffer = nullptr;
		PB::IBufferObject* m_batchIndexBuffer = nullptr;

		StreamingBatch* m_indexSrcBufferBatch = nullptr;

		// Meshlets
		PB::IBufferObject* m_batchMeshletUploadBuffer = nullptr;
		PB::IBufferObject* m_meshletSrcViewBuffer = nullptr;
		PB::IBufferObject* m_batchMeshletBuffer = nullptr;

		PB::IBufferObject* m_cullConstantsBuffer = nullptr;
		PB::IBufferObject* m_drawParamsBuffer = nullptr;
		PB::IBufferObject* m_drawMeshletParamsBuffer = nullptr;
		PB::IBufferObject* m_drawRangesBuffer = nullptr;

		StreamingBatch* m_meshletSrcBufferBatch = nullptr;

		// ----------------------------------------------------------------------------------------

		ManagedInstanceBuffer<DrawBatchInstanceData, MaxObjects, MaxObjects> m_instanceBuffer;
		uint32_t m_batchIndexCount = 0;
		uint32_t m_batchInstanceCount = 0;
		uint32_t m_meshletWorkgroupCount = 0;
		bool m_useMeshShaders = false;
		bool m_indicesUpToDate = false;
		Bounds m_bounds;

		CLib::Vector<StreamingBatch*, 0, 32> m_instanceStreamingBatches;
		CLib::Vector<Bounds, 0, 32> m_instanceBoundData;
		CLib::Vector<IndexUploadMetadata, 0, 32> m_indexUploadMetadata;
		CLib::Vector<MeshletUploadMetadata, 0, 32> m_meshletUploadMetadata;
	};

};