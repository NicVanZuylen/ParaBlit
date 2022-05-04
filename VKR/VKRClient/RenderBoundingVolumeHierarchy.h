#pragma once
#include "CLib/Allocator.h"

#pragma warning(push, 0)
#define GLM_FORCE_CTOR_INIT // Required to ensure glm constructors actually initialize vectors/matrices etc.
#include "glm/glm.hpp"
#pragma warning(pop)

class DebugLinePass;
class Camera;

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

	RenderBoundingVolumeHierarchy(const CreateDesc& desc);

	~RenderBoundingVolumeHierarchy();

	void InsertNode(glm::vec3 origin, glm::vec3 extents);

	void BuildBottomUp(CLib::Vector<std::pair<glm::vec3, glm::vec3>>& nodes);

	void DebugDraw(DebugLinePass* lines, uint32_t depth, bool drawObjectBounds);

private:

	static constexpr const uint32_t ChildSoftLimit = 4;
	static_assert(ChildSoftLimit > 1);

	struct Bounds
	{
		glm::vec3 m_origin;
		glm::vec3 m_extents;

		inline float MaxX() const { return m_origin.x + m_extents.x; }
		inline float MaxY() const { return m_origin.y + m_extents.y; }
		inline float MaxZ() const { return m_origin.z + m_extents.z; }

		glm::vec3 Centre() const
		{
			return m_origin + (m_extents * 0.5f);
		}

		float Volume() const
		{
			return m_extents.x * m_extents.y * m_extents.z;
		}

		bool IntersectsWith(const Bounds& other) const
		{
			return	(m_origin.x <= other.MaxX() && MaxX() >= other.m_origin.x) &&
					(m_origin.y <= other.MaxY() && MaxY() >= other.m_origin.y) &&
					(m_origin.z <= other.MaxZ() && MaxZ() >= other.m_origin.z);
		}

		bool Encapsulates(const Bounds& other) const
		{
			return (m_origin.x <= other.m_origin.x && m_origin.y <= other.m_origin.y && m_origin.z <= other.m_origin.z)
				&& (MaxX() >= other.MaxX() && MaxY() >= other.MaxY() && MaxZ() >= other.MaxZ());
		}

		void Encapsulate(const Bounds& other)
		{
			m_extents.x = glm::max(MaxX(), other.MaxX());
			m_extents.y = glm::max(MaxY(), other.MaxY());
			m_extents.z = glm::max(MaxZ(), other.MaxZ());

			m_origin.x = glm::min(m_origin.x, other.m_origin.x);
			m_origin.y = glm::min(m_origin.y, other.m_origin.y);
			m_origin.z = glm::min(m_origin.z, other.m_origin.z);

			m_extents -= m_origin;
		}
	};

	struct Plane
	{
		glm::vec3 m_normal;
		float m_distance = 0.0f;
	};

	struct CameraFrustrum
	{
		Plane m_left;
		Plane m_right;
		Plane m_top;
		Plane m_bottom;
		Plane m_near;
		Plane m_far;

		glm::vec3 m_nearTopLeft;
		glm::vec3 m_nearTopRight;
		glm::vec3 m_nearBottomLeft;
		glm::vec3 m_nearBottomRight;

		glm::vec3 m_farTopLeft;
		glm::vec3 m_farTopRight;
		glm::vec3 m_farBottomLeft;
		glm::vec3 m_farBottomRight;
	};

	struct Node;

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

	struct Node
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
		void RemoveChild(Node* child);

		Bounds m_bounds{};
		CLib::Vector<Node*, ChildSoftLimit> m_children{};
		uint32_t m_depth = 0;
		bool m_isObject = false;

		glm::vec3 m_objectOrigin;
		glm::vec3 m_objectExtents;
	};

	using Cluster = CLib::Vector<Node*>;
	using NodeWave = std::pair<CLib::Vector<Node*>*, float>;

	static uint64_t GetScore(const Bounds& bounds);

	void MergeNodes(Node& dst, CLib::Vector<Node*, ChildSoftLimit>& mergeCandidates);

	float GetClusterScore(const Node* a, std::vector<Node*>& pool);

	/*
	Description: Insert a node into a destination node and manage children further down the tree.
				 It is assumed dst's bounding box already fully encapsulates src's bounding box.
	Param:
		Node& src: The node to insert into dst.
		Node& dst: The destination node src will be inserted into.
	*/
	void RecursiveInsertInternal(Node& src, Node& dst);

	void GetContacts(Node& node, CLib::Vector<Node*, ChildSoftLimit>& pool, CLib::Vector<Node*, ChildSoftLimit>& outContacts);

	void GenClusters(CLib::Vector<Node*>& nodes, CLib::Vector<Cluster*>& outClusters, EProjectedAxis axis);

	void AxisSplitClusters(CLib::Vector<Cluster*>& clusters, EProjectedAxis axis);

	void RecursiveBuildBottomUp(CLib::Vector<Node*>& nodes, CLib::Vector<NodeWave>& waves, uint32_t waveIdx, uint32_t passCount);

	void AssignDepth(Node* node, uint32_t depth);

	/*
	Description: Free provided node allocation and all of its children.
	Param:
		Node* node: Node to free (including its children).
	*/
	void RecursiveFreeNode(Node* node);

	void ConstructCameraFrustrum(const Camera* camera, CameraFrustrum& outFrustrum);

	inline bool IsInFrontOfPlane(const Plane& plane, const Bounds& bounds);

	inline bool FrustrumTest(const CameraFrustrum& frustrum, const Bounds& bounds);

	void RecursiveDebugDrawNode(DebugLinePass* lines, const CameraFrustrum& frustrum, Node* node, uint32_t depth, bool drawObjectBounds);

	void DebugDrawCube(DebugLinePass* lines, const glm::vec3& origin, const glm::vec3& extents, const glm::vec3& lineColor);

	CLib::Allocator* m_allocator = nullptr;
	Node* m_root = nullptr;
	uint32_t m_desiredMaxDepth = 0;
	float m_toleranceDistance[uint32_t(EProjectedAxis::Z) + 1];
	float m_toleranceStep[uint32_t(EProjectedAxis::Z) + 1];
	const Camera* m_camera = nullptr;
};