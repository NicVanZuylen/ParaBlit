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
			MeshCacheData m_meshData;
			Eng::Material* m_material = nullptr;
			glm::mat4 m_transform;
		};

		RenderBoundingVolumeHierarchy() = default;
		RenderBoundingVolumeHierarchy(PB::IRenderer* renderer, CLib::Allocator* allocator, AssetStreamer* streamer, const CreateDesc& desc);

		~RenderBoundingVolumeHierarchy();

		void Init(PB::IRenderer* renderer, CLib::Allocator* allocator, AssetStreamer* streamer, const CreateDesc& desc);

		void Destroy() override;

		void RebuildTest();

		void BakeBatches(PB::ICommandContext* commandContext);

		void DebugDrawBatchTree(const Camera* camera, DebugLinePass* lines);

		void CullBatches(const Camera::CameraFrustrum& cameraFrustrum, BatchDispatcher* dispatcher, const PB::BindingLayout& globalBindings) const;

	private:

		using BatchObjects = CLib::Vector<BuildNode*>;
		using BatchMap = std::map<PB::Pipeline, CLib::Vector<BuildNode*>>;

		struct BakedNode
		{
			BakedNode* NextSibling() { return this + m_strideToNext; }
			const BakedNode* NextSibling() const { return this + m_strideToNext; }

			Bounds m_bounds{};
			uint32_t m_childCount = 0;
			uint32_t m_strideToNext = 0;
			size_t m_parentIndex = 0;
			DrawBatch* m_batch = nullptr;
			PB::Pipeline m_batchPipeline = 0;
		};

		struct PipelineDrawbatch
		{
			static constexpr uint32_t REMOVE_MaxObjects = 256;

			PB::Pipeline m_pipeline;
			DrawBatch* m_batch;
			size_t m_bakedNodeIndex; // Index of baked node corresponding to the drawbatch.
		};

		using NodeDrawBatches = CLib::Vector<PipelineDrawbatch, 4, 4>;
		struct NodeData : public BoundingVolumeHierarchy::NodeData
		{
			BuildNode* m_sourceNode = nullptr;
			NodeDrawBatches m_drawBatches{};
		};

		using BakedNodes = std::vector<BakedNode>;
		struct BatchHierarchy
		{
			CLib::Vector<size_t> m_bakedNodeUpdateIndices; // Indices of baked nodes who's batches need their indices updated.
			Cluster m_buildNodes{};
			BakedNodes m_bakedNodes{};
		};

		BuildNode* BuildBottomUp(InputObjects& objects, uint32_t baseDepth) override;

		void RebuildSubTree(BuildNode* subtreeRoot) override;

		BoundingVolumeHierarchy::NodeData* AllocateNodeData() override;

		void FreeNodeData(BoundingVolumeHierarchy::NodeData* data) override;

		void BuildDrawBatch(PB::Pipeline pipeline, BuildNode* node, BatchObjects& objects);

		void RecursiveSearchDownForDrawbatches(BuildNode* node, size_t& highestBakedNodeIndex);

		void RecursiveSearchUpForDrawbatches(BuildNode* node, BuildNode*& highestFound, std::set<PB::Pipeline>& pipelinesToFind, size_t& highestBakedNodeIndex);

		void RebuildDrawBatchesForSubtree(BuildNode* subtree, BuildNode* oldSubtree, InputObjects& objectData);

		void BuildDrawBatchesExclusive(BuildNode* root, BatchMap& batchMap, std::set<PB::Pipeline>& excludedPipelines);

		void RebuildDrawBatchesInclusive(BuildNode* root, BatchMap& batchMap, std::set<PB::Pipeline> includedPipelines);

		void GetBakedBatchBuildNodes(const BakedNode& node);

		void GetBatchCandidates(BatchMap& batchMap, BuildNode* node);

		void RecursiveFreeNode(BuildNode* node, bool keepBatches = false);

		void BakeHierarchies(BuildNode* start);

		void RecursiveBakeHierarchy(BakedNodes& bakedNodes, BuildNode* node, size_t parentIndex, size_t baseIndex = 0);

		void RecursiveBakeBatches(PB::ICommandContext* cmdContext, BakedNode& node);

		void RecursiveCullBatches(BatchDispatcher* dispatcher, const PB::BindingLayout& globalBindings, const Camera::CameraFrustrum& frustrum, const BakedNode& node) const;

		void RecursiveDebugDrawBakedTree(DebugLinePass* lines, const Camera::CameraFrustrum& frustrum, const BakedNode& node);

		PB::IRenderer* m_renderer = nullptr;
		AssetStreamer* m_streamer = nullptr;
		BatchHierarchy m_globalBatchHierarchy{};
	};

};