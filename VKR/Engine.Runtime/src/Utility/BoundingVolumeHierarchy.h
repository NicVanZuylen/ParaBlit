#pragma once
#include "CLib/Allocator.h"
#include "Camera.h"
#include "Bounds.h"

#include <unordered_map>

class DebugLinePass;

namespace Eng
{
	class BoundingVolumeHierarchy
	{
	public:

		struct CreateDesc
		{
			float m_toleranceDistanceX = 0.1f;
			float m_toleranceDistanceY = 0.1f;
			float m_toleranceDistanceZ = 0.1f;

			float m_toleranceStepX = 0.1f;
			float m_toleranceStepY = 0.1f;
			float m_toleranceStepZ = 0.1f;
		};

		class BuildNode;

		struct NodeHandle
		{
			BuildNode* m_node = nullptr;
		};

		struct ObjectData
		{
			NodeHandle m_handle;
			Bounds m_bounds;
		};

		using InputObjects = CLib::Vector<const ObjectData*>;

		BoundingVolumeHierarchy() = default;
		BoundingVolumeHierarchy(CLib::Allocator* allocator, const CreateDesc& desc);

		~BoundingVolumeHierarchy();

		void Init(CLib::Allocator* allocator, const CreateDesc& desc);

		virtual void Destroy();

		void Build();

		/*
		Description: Add an object to be built into the BVH. The provided pointer is expected to point to a persistent external allocation, and the pointer itself will be used as a handle for further modifications.
		Param:
			const ObjectData* objectData: Provided object data & handle. ObjectData instance is expected to be a persistent external allocation until it is removed from the BVH.
		*/
		void AddObject(const ObjectData* objectData);

		void UpdateObject(const ObjectData* objectData);

		void RemoveObject(const ObjectData* objectData);

		void UpdateTree();

		void DebugDraw(const Camera* camera, DebugLinePass* lines, uint32_t depth, bool drawObjectBounds) const;

		Bounds GetBounds() const { return m_root ? m_root->m_bounds : Bounds(); }

		const ObjectData* RaycastGetObjectData(DebugLinePass* lines, const glm::vec3& rayOrigin, const glm::vec3& rayDirection);

	protected:

		static constexpr const uint32_t ChildSoftLimit = 4;
		static constexpr const float WaveScalePercentageThreshold = 0.2f;
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
					assert(false && "BVH: Invalid axis.");
					return { m_bounds.m_origin.x, m_bounds.MaxX() };
					break;
				}
			}
			float GetLargestScale() const;
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

		virtual BuildNode* BuildBottomUp(InputObjects& objects, uint32_t baseDepth);

		virtual void RebuildSubTree(BuildNode* subtreeRoot);

		void AxisSplitClusters(ClusterArray& clusters, EProjectedAxis axis);

		BuildNode* BuildBottomUpInternal(CLib::Vector<BuildNode*>& nodes);

		BuildNode* RecursiveBuildBottomUp(CLib::Vector<NodeWave>& waves, uint32_t passCount = 0);

		void AssignDepth(BuildNode* node, uint32_t depth);

		// Find the deepest parent node which encapsulates the provided object.
		BuildNode* FindNewObjectParent(const ObjectData* object);

		std::pair<BuildNode*, BuildNode*> RebuildOneOrBoth(BuildNode* a, BuildNode* b);

		// Get all build nodes which need rebuilding from a list of moved/removed/transformed objects.
		void GetRebuildCandidates(CLib::Vector<BuildNode*>& outNodes, InputObjects& objects, InputObjects& erasureObjects);

		void RecursiveGetObjectData(CLib::Vector<const ObjectData*>& outObjectData, BuildNode* node);

		std::pair<BuildNode*, float> RecursiveObjectRayIntersection(BuildNode* node, const glm::vec3& rayOrigin, const glm::vec3& rayDirection);

		void RecursiveFreeNode(BuildNode* node);

		bool IsInFrontOfPlane(const Camera::CameraFrustrum::Plane& plane, const Bounds& bounds) const;

		bool FrustrumTest(const Camera::CameraFrustrum& frustrum, const Bounds& bounds) const;

		void RecursiveDebugDrawNode(DebugLinePass* lines, const Camera::CameraFrustrum& frustrum, BuildNode* node, uint32_t depth, bool drawObjectBounds) const;

		void DebugDrawCube(DebugLinePass* lines, const glm::vec3& origin, const glm::vec3& extents, const glm::vec3& lineColor) const;

		CLib::Allocator* m_allocator = nullptr;
		BuildNode* m_root = nullptr;
		uint32_t m_totalNodeCount = 0;
		float m_origToleranceDistance[uint32_t(EProjectedAxis::Z) + 1]{};
		float m_toleranceDistance[uint32_t(EProjectedAxis::Z) + 1]{};
		float m_toleranceStep[uint32_t(EProjectedAxis::Z) + 1]{};
		InputObjects m_input;
		InputObjects m_erasureObjects;
		std::unordered_map<const ObjectData*, BuildNode*> m_objectNodeMap;
	};

};