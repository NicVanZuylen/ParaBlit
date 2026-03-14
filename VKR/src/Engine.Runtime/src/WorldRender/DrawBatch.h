#pragma once
#include "Engine.ParaBlit/IRenderer.h"
#include "Engine.ParaBlit/IResourcePool.h"
#include "Engine.ParaBlit/ICommandContext.h"
#include "Engine.AssetEncoder/AssetBinaryDatabaseReader.h"
#include "Engine.AssetPipeline/MeshShared.h"
#include "ObjectDispatcher.h"
#include "ManagedInstanceBuffer.h"
#include "Bounds.h"

namespace Eng
{
	class Mesh;
	class AssetStreamer;
	class StreamingBatch;

	class DrawBatch
	{
	public:

		static constexpr const PB::u32 MaxObjects = 256;
		static constexpr const PB::u32 MaxDrawCount = 8;

		struct CreateDesc
		{
			PB::IRenderer* m_renderer;
			CLib::Allocator* m_allocator;
			AssetStreamer* m_streamer;
		};

		struct DrawBatchInstanceData
		{
			static constexpr const PB::u32 MaxTextures = 8;

			// Per-object model transformation matrix.
			float m_modelMatrix[16];
			float m_prevFrameModelMatrix[16];

			PB::u32 m_lodData;
			PB::ResourceView m_sampler;
			PB::u32 pad;

			// Filled via AssetStreamer:
			union
			{
				struct
				{
					PB::u32 m_meshID;
					PB::ResourceView m_textures[MaxTextures];
				};
				PB::ResourceView m_bindings[1 + MaxTextures];
			};
		};
		static_assert(sizeof(DrawBatchInstanceData) % 16 == 0);

		struct MeshletDrawParamsBuffer
		{
			PB::DrawMeshTasksIndirectParams m_drawMeshTasksParams[MaxDrawCount];
			uint32_t m_drawCount;
		};

		DrawBatch(const CreateDesc& desc);
		~DrawBatch();

		void DispatchFrustrumCull(PB::ICommandContext* cmdContext, PB::UniformBufferView viewConstantsView, bool cullTasks);

		static void DrawAllMeshShader
		(
			PB::ICommandContext* cmdContext, 
			const PB::BindingLayout& bindings, 
			PB::ResourceView instanceBuffer,
			PB::ResourceView meshletRangesBuffer,
			PB::ResourceView drawRangesBuffer,
			PB::ResourceView meshLibraryBuffer,
			PB::IBufferObject* drawParamsBuffer,
			PB::UniformBufferView cullConstantsView,
			PB::u32 drawParamsOffset = 0
		);
		void LocalDrawAllMeshShader(PB::ICommandContext* cmdContext, const PB::BindingLayout& bindings, PB::UniformBufferView viewConstantsView);

		void AddInstance(AssetEncoder::AssetID meshID, const float* modelMatrix, const Bounds& bounds, AssetEncoder::AssetID* textureIDs, uint32_t textureCount, PB::ResourceView sampler);

		void FinalizeUpdates();
		void UpdateIndices(PB::ICommandContext* cmdContext);
		void UpdateCullParams();

		Bounds GetBounds() { return m_bounds; }
		const PB::IBufferObject* GetDrawParametersBuffer() { return m_drawMeshletParamsBuffer; }

	private:

		// Per-instance data used for GPU-driven frustrum culling.
		struct CullObjectData
		{
			Vector3f m_boundOrigin;
			uint32_t m_partition;
			Vector3f m_boundExtents;
			uint32_t m_pad0;
			Vector2u m_drawRange; // Mesh index range of this object.
			Vector2u m_pad1;
		};

		struct BatchCullConstants
		{
			CullObjectData m_objects[256];
		};

		struct MeshletDrawRangesBuffer
		{
			Vector2u m_drawRanges[MaxDrawCount];
		};

		CLib::Vector<Vector2u, 32, 32> m_meshletRanges;

		PB::IRenderer* m_renderer = nullptr;
		CLib::Allocator* m_allocator = nullptr;
		AssetStreamer* m_streamer = nullptr;

		// ----------------------------------------------------------------------------------------
		// Compute uploads

		// Meshlets
		PB::IBufferObject* m_meshletRangesBuffer = nullptr;

		PB::IBufferObject* m_cullConstantsBuffer = nullptr;
		PB::IBufferObject* m_drawMeshletParamsBuffer = nullptr;
		PB::IBufferObject* m_drawRangesBuffer = nullptr;

		// ----------------------------------------------------------------------------------------

		ManagedInstanceBuffer m_instanceBuffer;
		uint32_t m_batchMeshletCount = 0;
		uint8_t m_batchInstanceCount = 0;
		bool m_meshShadersSupported = false;
		bool m_indicesUpToDate = false;
		Bounds m_bounds;

		CLib::Vector<StreamingBatch*, 0, 32> m_instanceStreamingBatches;
		CLib::Vector<Bounds, 0, 32> m_instanceBoundData;
	};

};