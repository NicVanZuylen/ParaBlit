#pragma once
#include "CLib/Allocator.h"
#include "Camera.h"
#include "Bounds.h"

class DebugLinePass;

namespace Eng
{
	class BoundingVolumeHierarchy
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

		class BuildNode;
		
		struct ObjectData
		{
			Bounds m_bounds;
		};

		using InputObjects = CLib::Vector<const ObjectData*>;

		BoundingVolumeHierarchy() = default;
		BoundingVolumeHierarchy(CLib::Allocator* allocator, const CreateDesc& desc);

		~BoundingVolumeHierarchy();

		void Init(CLib::Allocator* allocator, const CreateDesc& desc);

		virtual void Destroy();

		void Build();

		void AddObject(const ObjectData* objectData);

		void DebugDraw(const Camera* camera, DebugLinePass* lines, uint32_t depth, bool drawObjectBounds) const;

		Bounds GetBounds() const { return m_root ? m_root->m_bounds : Bounds(); }

	protected:

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

		struct NodeData
		{

		};

		using Cluster = CLib::Vector<BuildNode*>;
		using ClusterArray = CLib::Vector<Cluster*, 128, 64>;
		using NodeWave = std::pair<Cluster*, float>;

		using NodeChildren = CLib::Vector<BuildNode*, ChildSoftLimit>;
		struct BuildNode
		{
			bool IsLeaf() const { return m_children.Count() == 0; }
			ProjectedRange GetRange(EProjectedAxis axis) const
			{
				switch (axis)
				{
				case BoundingVolumeHierarchy::EProjectedAxis::X:
					return { m_bounds.m_origin.x, m_bounds.MaxX() };
					break;
				case BoundingVolumeHierarchy::EProjectedAxis::Y:
					return { m_bounds.m_origin.y, m_bounds.MaxY() };
					break;
				case BoundingVolumeHierarchy::EProjectedAxis::Z:
					return { m_bounds.m_origin.z, m_bounds.MaxZ() };
					break;
				default:
					return { m_bounds.m_origin.x, m_bounds.MaxX() };
					break;
				}
			}
			void RemoveChild(BuildNode* child);

			Bounds m_bounds{};
			BuildNode* m_parent = nullptr;
			NodeChildren m_children{};
			const ObjectData* m_objectData = nullptr;
			NodeData* m_data = nullptr;
			uint32_t m_depth = 0;
			bool m_isObject = false;
		};

		virtual NodeData* AllocateNodeData() = 0;

		virtual void FreeNodeData(NodeData* data) = 0;

		BuildNode* AllocateBuildNode();

		void FreeBuildNode(BuildNode* node);

		static uint64_t GetScore(const Bounds& bounds);

		float GetClusterScore(const BuildNode* a, std::vector<BuildNode*>& pool);

		virtual BuildNode* BuildBottomUp(InputObjects& objects);

		virtual void RebuildSubTree(BuildNode* subtreeRoot);

		void GenClusters(CLib::Vector<BuildNode*>& nodes, CLib::Vector<Cluster*>& outClusters, EProjectedAxis axis);

		void AxisSplitClusters(ClusterArray& clusters, EProjectedAxis axis);

		BuildNode* BuildBottomUpInternal(CLib::Vector<BuildNode*>& nodes);

		BuildNode* RecursiveBuildBottomUp(const CLib::Vector<BuildNode*>& nodes, CLib::Vector<NodeWave>& waves, uint32_t waveIdx, uint32_t passCount);

		void AssignDepth(BuildNode* node, uint32_t depth);

		void RecursiveGetObjectData(CLib::Vector<const ObjectData*>& outObjectData, BuildNode* node);

		void RecursiveFreeNode(BuildNode* node);

		bool IsInFrontOfPlane(const Camera::CameraFrustrum::Plane& plane, const Bounds& bounds) const;

		bool FrustrumTest(const Camera::CameraFrustrum& frustrum, const Bounds& bounds) const;

		void RecursiveDebugDrawNode(DebugLinePass* lines, const Camera::CameraFrustrum& frustrum, BuildNode* node, uint32_t depth, bool drawObjectBounds) const;

		void DebugDrawCube(DebugLinePass* lines, const glm::vec3& origin, const glm::vec3& extents, const glm::vec3& lineColor) const;

		CLib::Allocator* m_allocator = nullptr;
		BuildNode* m_root = nullptr;
		uint32_t m_desiredMaxDepth = 0;
		uint32_t m_totalNodeCount = 0;
		float m_toleranceDistance[uint32_t(EProjectedAxis::Z) + 1]{};
		float m_toleranceStep[uint32_t(EProjectedAxis::Z) + 1]{};
		InputObjects m_input;
	};

};