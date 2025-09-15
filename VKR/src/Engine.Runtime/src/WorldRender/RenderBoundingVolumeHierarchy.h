#pragma once
#include "Resource/Mesh.h"
#include "Utility/BoundingVolumeHierarchy.h"
#include "Engine.ParaBlit/IRenderer.h"
#include "Engine.ParaBlit/ICommandContext.h"

#include <map>
#include <set>

class DebugLinePass;

namespace Eng
{
	class Mesh;
	class Material;

	class DrawBatch;
	class BatchDispatcher;

	class AssetStreamer;

	class RenderBoundingVolumeHierarchy : public BoundingVolumeHierarchy
	{
	public:

		struct ObjectData : public BoundingVolumeHierarchy::ObjectData
		{
			AssetEncoder::AssetID m_meshID = 0;
			AssetPipeline::MeshCacheData m_meshData;
			Eng::Material* m_material = nullptr;
			Matrix4 m_transform;
		};

		RenderBoundingVolumeHierarchy() = default;
		RenderBoundingVolumeHierarchy(PB::IRenderer* renderer, CLib::Allocator* extAllocator, AssetStreamer* streamer, const CreateDesc& desc);

		~RenderBoundingVolumeHierarchy();

		void Init(PB::IRenderer* renderer, CLib::Allocator* extAllocator, AssetStreamer* streamer, const CreateDesc& desc);

		void Destroy() override;

		void RebuildTest();

		void BakeBatches(PB::ICommandContext* commandContext);

		void DebugDrawBatchTree(const Camera* camera, DebugLinePass* lines, uint32_t desiredDepth = ~uint32_t(0));

		void CullBatches(const Camera::CameraFrustrum& cameraFrustrum, BatchDispatcher* dispatcher, const PB::BindingLayout& globalBindings) const;

	private:

		using BatchObjects = CLib::Vector<BuildNode*>;
		using BatchMap = std::map<PB::Pipeline, CLib::Vector<BuildNode*>>;

		struct PipelineDrawbatch
		{
			PB::Pipeline m_pipeline;
			DrawBatch* m_batch;
		};

		using NodeDrawBatches = CLib::Vector<PipelineDrawbatch, 4, 4>;
		struct NodeData : public BoundingVolumeHierarchy::NodeData
		{
			NodeDrawBatches m_drawBatches{};
		};

		BuildNode* BuildBottomUp(InputObjects& objects, uint32_t baseDepth) override;

		void RebuildSubTree(BuildNode* subtreeRoot) override;

		void AllocateNodeData(BuildNode* node) override;

		void FreeNodeData(BuildNode* node) override;

		void BuildDrawBatch(PB::Pipeline pipeline, BuildNode* node, BatchObjects& objects);

		void RecursiveSearchDownForDrawbatches(BuildNode* node, std::set<PB::Pipeline>& pipelinesToFind);

		void RecursiveSearchUpForDrawbatches(BuildNode* node, BuildNode*& highestBatchNode, std::set<PB::Pipeline>& pipelinesToFind);

		void RebuildDrawBatchesForSubtree(BuildNode* subtree, BuildNode* oldSubtree, InputObjects& objectData);

		void BuildDrawBatchesExclusive(BuildNode* root, BatchMap& batchMap, std::set<PB::Pipeline>& excludedPipelines);

		void RebuildDrawBatchesExclusive(BuildNode* root, BatchMap& batchMap, std::set<PB::Pipeline> excludedPipelines);

		void GetBatchCandidates(BatchMap& batchMap, BuildNode* node);

		void RecursiveFreeNode(BuildNode* node, bool keepBatches = false);

		void RecursiveCullBatchesByNode(BatchDispatcher* dispatcher, const PB::BindingLayout& globalBindings, const Camera::CameraFrustrum& frustrum, const BuildNode* node) const;

		void RecursiveDebugDrawBatches(DebugLinePass* lines, const Camera::CameraFrustrum& frustrum, const BuildNode* node, uint32_t desiredDepth, uint32_t depth = 0);

		PB::IRenderer* m_renderer = nullptr;
		CLib::Allocator* m_extAllocator = nullptr;
		AssetStreamer* m_streamer = nullptr;
		CLib::Vector<DrawBatch*, 8, 64> m_drawBatchesToUpdate;
	};

};