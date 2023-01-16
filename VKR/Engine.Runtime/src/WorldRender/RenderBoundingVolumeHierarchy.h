#pragma once
#include "CLib/Allocator.h"
#include "Camera.h"
#include "Bounds.h"
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

	class RenderBoundingVolumeHierarchy
	{
	public:

		struct CreateDesc
		{
			uint32_t m_desiredMaxDepth = 0;

			float m_toleranceDistanceX = 0.1f;
			float m_toleranceDistanceY = 0.1f;
			float m_toleranceDistanceZ = 0.1f;

			float m_toleranceStepX = 0.1f;
			float m_toleranceStepY = 0.1f;
			float m_toleranceStepZ = 0.1f;
		};

		struct ObjectData
		{
			Eng::Mesh* m_mesh = nullptr;
			Eng::Material* m_material = nullptr;
			glm::mat4 m_transform;
		};

		struct BuildNode;

		RenderBoundingVolumeHierarchy(PB::IRenderer* renderer, CLib::Allocator* allocator, const CreateDesc& desc);

		~RenderBoundingVolumeHierarchy();

		void BuildBottomUp(CLib::Vector<ObjectData>& objects);

		BuildNode* BuildSubTree(CLib::Vector<ObjectData>& objects);

		void RebuildTest();

		void RebuildSubTree(BuildNode* subtreeRoot);

		void BakeBatches(PB::ICommandContext* commandContext);

		void DebugDraw(const Camera* camera, DebugLinePass* lines, uint32_t depth, bool drawObjectBounds);

		void DebugDrawBatchTree(const Camera* camera, DebugLinePass* lines);

		void CullBatches(const Camera::CameraFrustrum& cameraFrustrum, BatchDispatcher* dispatcher, const PB::BindingLayout& globalBindings) const;

		Bounds GetBounds() const { return m_sourceRoot ? m_sourceRoot->m_bounds : Bounds(); }

	private:

		static constexpr const uint32_t ChildSoftLimit = 4;
		static_assert(ChildSoftLimit > 1);

		struct ProjectedRange
		{
			float m_min;
			float m_max;
		};

		enum class EProjectedAxis
		{
			X,
			Y,
			Z
		};

		using Cluster = CLib::Vector<BuildNode*>;
		using ClusterArray = CLib::Vector<Cluster*, 128, 64>;
		using NodeWave = std::pair<CLib::Vector<BuildNode*>*, float>;
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
			static constexpr uint32_t REMOVE_MaxObjects = 5;

			PB::Pipeline m_pipeline;
			DrawBatch* m_batch;
			size_t m_bakedNodeIndex; // Index of baked node corresponding to the drawbatch.
		};

		using NodeChildren = CLib::Vector<BuildNode*, ChildSoftLimit>;
		using NodeDrawBatches = CLib::Vector<PipelineDrawbatch, 4, 4>;
		struct BuildNode
		{
			bool IsLeaf() { return m_children.Count() == 0; }
			ProjectedRange GetRange(EProjectedAxis axis) const
			{
				switch (axis)
				{
				case RenderBoundingVolumeHierarchy::EProjectedAxis::X:
					return { m_bounds.m_origin.x, m_bounds.MaxX() };
					break;
				case RenderBoundingVolumeHierarchy::EProjectedAxis::Y:
					return { m_bounds.m_origin.y, m_bounds.MaxY() };
					break;
				case RenderBoundingVolumeHierarchy::EProjectedAxis::Z:
					return { m_bounds.m_origin.z, m_bounds.MaxZ() };
					break;
				default:
					return { m_bounds.m_origin.x, m_bounds.MaxX() };
					break;
				}
			}
			void RemoveChild(BuildNode* child);

			Bounds m_bounds{};
			ObjectData m_objectData{};
			BuildNode* m_parent = nullptr;
			BuildNode* m_sourceNode = nullptr;
			NodeChildren m_children{};
			NodeDrawBatches m_drawBatches{};
			uint32_t m_depth = 0;
			bool m_isObject = false;
		};

		using BakedNodes = std::vector<BakedNode>;
		struct BatchHierarchy
		{
			CLib::Vector<size_t> m_bakedNodeUpdateIndices; // Indices of baked nodes who's batches need their indices updated.
			Cluster m_buildNodes{};
			BakedNodes m_bakedNodes{};
		};

		static uint64_t GetScore(const Bounds& bounds);

		float GetClusterScore(const BuildNode* a, std::vector<BuildNode*>& pool);

		void GenClusters(CLib::Vector<BuildNode*>& nodes, CLib::Vector<Cluster*>& outClusters, EProjectedAxis axis);

		void AxisSplitClusters(ClusterArray& clusters, EProjectedAxis axis);

		BuildNode* BuildBottomUpInternal(CLib::Vector<BuildNode*>& nodes);

		BuildNode* RecursiveBuildBottomUp(const CLib::Vector<BuildNode*>& nodes, CLib::Vector<NodeWave>& waves, uint32_t waveIdx, uint32_t passCount);

		void AssignDepth(BuildNode* node, uint32_t depth);

		void BuildDrawBatch(PB::Pipeline pipeline, BuildNode* node, BatchObjects& objects);

		void RecursiveSearchDownForDrawbatches(BuildNode* node, size_t& highestBakedNodeIndex);

		void RecursiveSearchUpForDrawbatches(BuildNode* node, BuildNode*& highestFound, std::set<PB::Pipeline>& pipelinesToFind, size_t& highestBakedNodeIndex);

		void RebuildDrawBatchesForSubtree(BuildNode* subtree, BuildNode* oldSubtree, CLib::Vector<ObjectData>& objectData);

		void BuildDrawBatchesExclusive(BuildNode* root, BatchMap& batchMap, std::set<PB::Pipeline>& excludedPipelines);

		void RebuildDrawBatchesInclusive(BuildNode* root, BatchMap& batchMap, std::set<PB::Pipeline> includedPipelines);

		void GetBakedBatchBuildNodes(const BakedNode& node);

		void GetBatchCandidates(BatchMap& batchMap, BuildNode* node);

		void RecursiveGetObjectData(CLib::Vector<ObjectData>& outObjectData, BuildNode* node);

		void RecursiveFreeNode(BuildNode* node, bool keepBatches = false);

		void BakeHierarchies(BuildNode* start);

		void RecursiveBakeHierarchy(BakedNodes& bakedNodes, BuildNode* node, size_t parentIndex, size_t baseIndex = 0);

		inline bool IsInFrontOfPlane(const Camera::CameraFrustrum::Plane& plane, const Bounds& bounds) const;

		inline bool FrustrumTest(const Camera::CameraFrustrum& frustrum, const Bounds& bounds) const;

		void RecursiveBakeBatches(PB::ICommandContext* cmdContext, BakedNode& node);

		void RecursiveCullBatches(BatchDispatcher* dispatcher, const PB::BindingLayout& globalBindings, const Camera::CameraFrustrum& frustrum, const BakedNode& node) const;

		void RecursiveDebugDrawBakedTree(DebugLinePass* lines, const Camera::CameraFrustrum& frustrum, const BakedNode& node);

		void RecursiveDebugDrawNode(DebugLinePass* lines, const Camera::CameraFrustrum& frustrum, BuildNode* node, uint32_t depth, bool drawObjectBounds);

		void DebugDrawCube(DebugLinePass* lines, const glm::vec3& origin, const glm::vec3& extents, const glm::vec3& lineColor);

		PB::IRenderer* m_renderer = nullptr;
		CLib::Allocator* m_allocator = nullptr;
		BuildNode* m_sourceRoot = nullptr;
		BatchHierarchy m_globalBatchHierarchy{};
		uint32_t m_desiredMaxDepth = 0;
		uint32_t m_totalNodeCount = 0;
		float m_toleranceDistance[uint32_t(EProjectedAxis::Z) + 1];
		float m_toleranceStep[uint32_t(EProjectedAxis::Z) + 1];
	};

};