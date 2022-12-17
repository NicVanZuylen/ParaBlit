#pragma once
#include "CLib/Allocator.h"
#include "Camera.h"
#include "Bounds.h"
#include "Engine.ParaBlit/IRenderer.h"
#include "Engine.ParaBlit/ICommandContext.h"

#include <map>

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

			const Camera* m_camera = nullptr;
		};

		struct ObjectData
		{
			Eng::Mesh* m_mesh = nullptr;
			Eng::Material* m_material = nullptr;
			glm::mat4 m_transform;
		};

		RenderBoundingVolumeHierarchy(PB::IRenderer* renderer, CLib::Allocator* allocator, const CreateDesc& desc);

		~RenderBoundingVolumeHierarchy();

		void BuildBottomUp(CLib::Vector<ObjectData>& objects);

		void DebugDraw(DebugLinePass* lines, uint32_t depth, bool drawObjectBounds);

		void BakeHierarchies(PB::ICommandContext* commandContext);

		void SetCamera(const Camera* camera) { m_camera = camera; };

		void CullBatches(const Camera* camera, BatchDispatcher* dispatcher, const PB::BindingLayout& globalBindings) const;

	private:

		static constexpr const uint32_t ChildSoftLimit = 4;
		static_assert(ChildSoftLimit > 1);

		struct BuildNode;

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
			DrawBatch* m_drawBatch = nullptr;
			ObjectData m_objectData{};
			CLib::Vector<BuildNode*, ChildSoftLimit> m_children{};
			uint32_t m_depth = 0;
			bool m_isObject = false;
		};

		struct BakedNode
		{
			const BakedNode& GetChild(uint32_t childIndex) const { return *(this + uintptr_t(childIndex + 1)); }
			uint32_t ChildCount() const { return (m_nextIndex - 1) - m_index; }

			Bounds m_bounds;
			uint32_t m_index;
			uint32_t m_nextIndex;
			DrawBatch* m_batch;
		};

		struct PipelineHierarchy
		{
			BuildNode* m_root = nullptr;
			bool m_batchesInitialized = false;
			CLib::Vector<DrawBatch*> m_batches;
			CLib::Vector<BakedNode> m_bakedHierarchy;
		};

		using Cluster = CLib::Vector<BuildNode*>;
		using NodeWave = std::pair<CLib::Vector<BuildNode*>*, float>;
		using BatchObjects = CLib::Vector<BuildNode*>;
		using BatchMap = std::map<PB::Pipeline, CLib::Vector<BuildNode*>>;

		static uint64_t GetScore(const Bounds& bounds);

		float GetClusterScore(const BuildNode* a, std::vector<BuildNode*>& pool);

		void GenClusters(CLib::Vector<BuildNode*>& nodes, CLib::Vector<Cluster*>& outClusters, EProjectedAxis axis);

		void AxisSplitClusters(CLib::Vector<Cluster*>& clusters, EProjectedAxis axis);

		BuildNode* BuildBottomUpInternal(CLib::Vector<BuildNode*>& nodes);

		BuildNode* RecursiveBuildBottomUp(CLib::Vector<BuildNode*>& nodes, CLib::Vector<NodeWave>& waves, uint32_t waveIdx, uint32_t passCount);

		void AssignDepth(BuildNode* node, uint32_t depth);

		void BuildDrawBatch(PB::Pipeline pipeline, BatchObjects& objects);

		void BuildDrawBatches(BuildNode* root, BatchMap& batchMap);

		void GetBatchCandidates(BatchMap& batchMap, BuildNode* node);

		void RecursiveBakeHierarchy(CLib::Vector<BakedNode>& bakedNodes, BuildNode* node);

		/*
		Description: Free provided node allocation and all of its children.
		Param:
			Node* node: Node to free (including its children).
		*/
		void RecursiveFreeNode(BuildNode* node);

		inline bool IsInFrontOfPlane(const Camera::CameraFrustrum::Plane& plane, const Bounds& bounds) const;

		inline bool FrustrumTest(const Camera::CameraFrustrum& frustrum, const Bounds& bounds) const;

		void RecursiveCullBatches(BatchDispatcher* dispatcher, PB::Pipeline pipeline, const PB::BindingLayout& globalBindings, const Camera::CameraFrustrum& frustrum, const BakedNode& node) const;

		void RecursiveDebugDrawNode(DebugLinePass* lines, const Camera::CameraFrustrum& frustrum, BuildNode* node, uint32_t depth, bool drawObjectBounds);

		void DebugDrawCube(DebugLinePass* lines, const glm::vec3& origin, const glm::vec3& extents, const glm::vec3& lineColor);

		PB::IRenderer* m_renderer = nullptr;
		CLib::Allocator* m_allocator = nullptr;
		std::map<PB::Pipeline, PipelineHierarchy> m_pipelineHierarchies;
		BuildNode* m_root = nullptr;
		uint32_t m_desiredMaxDepth = 0;
		uint32_t m_totalNodeCount = 0;
		float m_toleranceDistance[uint32_t(EProjectedAxis::Z) + 1];
		float m_toleranceStep[uint32_t(EProjectedAxis::Z) + 1];
		const Camera* m_camera = nullptr;
	};

};