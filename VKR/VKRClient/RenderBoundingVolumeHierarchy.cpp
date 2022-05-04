#include "RenderBoundingVolumeHierarchy.h"
#include "DebugLinePass.h"
#include "Camera.h"

#include <algorithm>
#include <iostream>

RenderBoundingVolumeHierarchy::RenderBoundingVolumeHierarchy(const CreateDesc& desc)
{
	m_desiredMaxDepth = desc.m_desiredMaxDepth;
	
	m_toleranceDistance[0] = desc.m_toleranceDistanceX;
	m_toleranceDistance[1] = desc.m_toleranceDistanceY;
	m_toleranceDistance[2] = desc.m_toleranceDistanceZ;

	m_toleranceStep[0] = desc.m_toleranceStepX;
	m_toleranceStep[1] = desc.m_toleranceStepY;
	m_toleranceStep[2] = desc.m_toleranceStepZ;

	m_camera = desc.m_camera;
}

RenderBoundingVolumeHierarchy::~RenderBoundingVolumeHierarchy()
{
	RecursiveFreeNode(m_root);
}

void RenderBoundingVolumeHierarchy::InsertNode(glm::vec3 origin, glm::vec3 extents)
{
	Node* newNode = m_allocator->Alloc<Node>();
	newNode->m_bounds.m_origin = origin;
	newNode->m_bounds.m_extents = extents;

	newNode->m_objectOrigin = origin;
	newNode->m_objectExtents = extents;

	m_root->m_bounds.Encapsulate(newNode->m_bounds);
	RecursiveInsertInternal(*newNode, *m_root);
}

void RenderBoundingVolumeHierarchy::BuildBottomUp(CLib::Vector<std::pair<glm::vec3, glm::vec3>>& inBounds)
{
	CLib::Vector<Node*> nodes;
	nodes.Reserve(inBounds.Count());

	for (auto& [origin, extents] : inBounds)
	{
		Node* n = m_allocator->Alloc<Node>();
		nodes.PushBack(n);
		Node& node = *n;

		node.m_bounds.m_origin = origin;
		node.m_bounds.m_extents = extents;
		node.m_objectOrigin = origin;
		node.m_objectExtents = extents;
		node.m_depth = 0;
		node.m_isObject = true;
	}

	auto compare = [=](const Node* a, const Node* b)
	{
		return a->m_bounds.Volume() < b->m_bounds.Volume();
	};
	std::sort(nodes.begin(), nodes.end(), compare);

	// Represents nodes who's insertion/cluster detection has been deferred to higher up the tree
	// due to large size relative to previous nodes.
	//
	// deferredNodes is a 2D array of nodes, where each array contains nodes X% larger volume than the previous arrays's largest node.
	CLib::Vector<NodeWave> deferredNodes{};

	const float percentageThreshold = 20.0f / 100.0f;
	float prevVolume = 0.0f;
	Cluster* currentGroup = nullptr;
	for (auto& n : nodes)
	{
		float volume = n->m_bounds.Volume();

		if (volume - prevVolume > (prevVolume * percentageThreshold))
		{
			NodeWave& newWave = deferredNodes.PushBack();
			newWave.second = volume;

			currentGroup = newWave.first = m_allocator->Alloc<CLib::Vector<Node*>>();
			currentGroup->PushBack(n);
			n = nullptr;

			prevVolume = volume;
		}
		else if (currentGroup != nullptr)
		{
			currentGroup->PushBack(n);
			n = nullptr;
		}

	}

	nodes = *deferredNodes[0].first;
	RecursiveBuildBottomUp(nodes, deferredNodes, 1, 0);

	for (NodeWave& wave : deferredNodes)
	{
		m_allocator->Free(wave.first);
	}

	AssignDepth(m_root, 0);
}

void RenderBoundingVolumeHierarchy::DebugDraw(DebugLinePass* lines, uint32_t depth, bool drawObjectBounds)
{
	CameraFrustrum frustrum;
	if (m_camera != nullptr)
	{
		ConstructCameraFrustrum(m_camera, frustrum);

		// Draw frustrum
		glm::vec3 frustrumColor(1.0f, 0.0f, 1.0f);

		lines->DrawLine(frustrum.m_nearTopLeft, frustrum.m_nearTopRight, frustrumColor);
		lines->DrawLine(frustrum.m_nearTopLeft, frustrum.m_nearBottomLeft, frustrumColor);
		lines->DrawLine(frustrum.m_nearBottomLeft, frustrum.m_nearBottomRight, frustrumColor);
		lines->DrawLine(frustrum.m_nearBottomRight, frustrum.m_nearTopRight, frustrumColor);

		lines->DrawLine(frustrum.m_farTopLeft, frustrum.m_farTopRight, frustrumColor);
		lines->DrawLine(frustrum.m_farTopLeft, frustrum.m_farBottomLeft, frustrumColor);
		lines->DrawLine(frustrum.m_farBottomLeft, frustrum.m_farBottomRight, frustrumColor);
		lines->DrawLine(frustrum.m_farBottomRight, frustrum.m_farTopRight, frustrumColor);

		lines->DrawLine(frustrum.m_nearTopLeft, frustrum.m_farTopLeft, frustrumColor);
		lines->DrawLine(frustrum.m_nearBottomLeft, frustrum.m_farBottomLeft, frustrumColor);
		lines->DrawLine(frustrum.m_nearTopRight, frustrum.m_farTopRight, frustrumColor);
		lines->DrawLine(frustrum.m_nearBottomRight, frustrum.m_farBottomRight, frustrumColor);
	}

	RecursiveDebugDrawNode(lines, frustrum, m_root, depth, drawObjectBounds);
}

void RenderBoundingVolumeHierarchy::MergeNodes(Node& dst, CLib::Vector<Node*, ChildSoftLimit>& mergeCandidates)
{
	static auto compare = [](const Node* a, const Node* b) -> bool
	{
		auto scoreA = GetScore(a->m_bounds);
		auto scoreB = GetScore(b->m_bounds);

		return scoreA > scoreB;
	};

	// Tree structure will be more effective for frustrum culling if larger objects remain higher in the tree in larger nodes.
	// So sort and pick the first node to remain as a child of 'dst', while all others become a child of the first node.
	std::sort(mergeCandidates.begin(), mergeCandidates.end(), compare);

	Node& mergeDst = *mergeCandidates[0];
	dst.RemoveChild(&mergeDst);

	for (uint32_t i = 1; i < mergeCandidates.Count(); ++i)
	{
		auto& cand = mergeCandidates[i];

		mergeDst.m_bounds.Encapsulate(cand->m_bounds);
		dst.RemoveChild(cand);

		RecursiveInsertInternal(*cand, mergeDst);
	}

	// Resume insertion process.
	RecursiveInsertInternal(mergeDst, dst);
}

void RenderBoundingVolumeHierarchy::GetContacts(Node& node, CLib::Vector<Node*, ChildSoftLimit>& pool, CLib::Vector<Node*, ChildSoftLimit>& outContacts)
{
	for (Node* n : pool)
	{
		if (n != &node && n->m_bounds.IntersectsWith(node.m_bounds))
		{
			outContacts.PushBack(n);
			GetContacts(*n, pool, outContacts);
		}
	}
}

float RenderBoundingVolumeHierarchy::GetClusterScore(const Node* a, std::vector<Node*>& pool)
{
	float total = 0.0f;
	for (const Node* b : pool)
	{
		if (b != a)
		{
			float dist = glm::distance(a->m_bounds.Centre(), b->m_bounds.Centre());
			total -= dist;
		}
	}

	return total - GetScore(a->m_bounds);
}

void RenderBoundingVolumeHierarchy::RecursiveInsertInternal(Node& src, Node& dst)
{
	src.m_depth = dst.m_depth + 1;

	// It is assumed dst always encapsulates src when this function is called.

	bool allContactsAreLeaves = src.IsLeaf();
	CLib::Vector<Node*, ChildSoftLimit> contacts;
	for (auto& node : dst.m_children)
	{
		if (node->m_bounds.IntersectsWith(src.m_bounds))
		{
			contacts.PushBack(node);
			allContactsAreLeaves &= node->IsLeaf();
		}
	}

	bool allowedIntersectingPlacement = allContactsAreLeaves && dst.m_depth >= m_desiredMaxDepth;
	bool notTooCrowded = contacts.Count() == 0 && dst.m_children.Count() < ChildSoftLimit;
	if (allowedIntersectingPlacement || notTooCrowded)
	{
		// Even if src does intersect with one or more of dst's children, if they are all leaves not merging is acceptable.

		// ---------------------------------------------------------
		// Place path
		std::cout << "Place Path!\n";

		dst.m_children.PushBack(&src);
		// ---------------------------------------------------------
	}
	else if (contacts.Count() > 0)
	{
		// ---------------------------------------------------------
		// Merge path
		std::cout << "Merge Path!\n";

		contacts.PushBack(&src);
		MergeNodes(dst, contacts);

		// ---------------------------------------------------------
	}
	else
	{
		std::cout << "Force Merge Path!\n";

		// Merge with the closest child node of 'dst' and any other child nodes it intersects with.
		//static auto compare = [&](const Node* a, const Node* b) -> bool
		//{
		//	float distA = glm::distance(src.m_bounds.Centre(), a->m_bounds.Centre());
		//	float distB = glm::distance(src.m_bounds.Centre(), b->m_bounds.Centre());

		//	float scoreA = (1.0f / distA); // - float(GetScore(a->m_bounds));
		//	float scoreB = (1.0f / distB); // - float(GetScore(b->m_bounds));

		//	return scoreA > scoreB;
		//};
		//std::sort(dst.m_children.begin(), dst.m_children.end(), compare);

		//contacts.PushBack(&src);
		//contacts.PushBack(dst.m_children[0]);
		//GetContacts(*dst.m_children[0], dst.m_children, contacts);

		//MergeNodes(dst, contacts);

		dst.m_children.PushBack(&src);

		{
			std::vector<Node*> searchPool{};
			for (Node* n : dst.m_children)
				searchPool.push_back(n);

			static auto compareA = [&](const Node* a, const Node* b) -> bool
			{
				float scoreA = GetClusterScore(a, searchPool);
				float scoreB = GetClusterScore(b, searchPool);
				return scoreA > scoreB;
			};
			std::sort(dst.m_children.begin(), dst.m_children.end(), compareA);
		}

		Node* winner = dst.m_children[0];
		assert(winner);
		//dst.RemoveChild(winner);
		contacts.PushBack(winner);
		GetContacts(*winner, dst.m_children, contacts);

		static auto compareB = [=](const Node* a, const Node* b) -> bool
		{
			float distA = glm::distance(winner->m_bounds.Centre(), a->m_bounds.Centre());
			float distB = glm::distance(winner->m_bounds.Centre(), b->m_bounds.Centre());
			return distA < distB;
		};
		dst.RemoveChild(winner);
		std::sort(dst.m_children.begin(), dst.m_children.end(), compareB);
		dst.m_children.PushBack(winner);

		Node* closest = dst.m_children[0];
		assert(closest);
		contacts.PushBack(closest);
		GetContacts(*closest, dst.m_children, contacts);

		assert(winner != closest);
		MergeNodes(dst, contacts);
	}
}


void RenderBoundingVolumeHierarchy::GenClusters(CLib::Vector<Node*>& nodes, CLib::Vector<Cluster*>& outClusters, EProjectedAxis axis)
{
	auto rangeCompare = [=](const Node* a, const Node* b) -> bool
	{
		return a->GetRange(axis).m_min < b->GetRange(axis).m_min;
	};
	std::sort(nodes.begin(), nodes.end(), rangeCompare);

	
}

void RenderBoundingVolumeHierarchy::AxisSplitClusters(CLib::Vector<Cluster*>& clusters, EProjectedAxis axis)
{
	auto rangeCompare = [=](const Node* a, const Node* b) -> bool
	{
		return a->GetRange(axis).m_min < b->GetRange(axis).m_min;
	};

	CLib::Vector<Cluster*> newClusters;
	for (Cluster* c : clusters)
	{
		Cluster& cluster = *c;

		std::sort(cluster.begin(), cluster.end(), rangeCompare);

		Cluster* lastCluster = &cluster;
		ProjectedRange prevNodeRange = cluster[0]->GetRange(axis);
		for (uint32_t i = 1; i < cluster.Count(); ++i)
		{
			ProjectedRange nodeRange = cluster[i]->GetRange(axis);

			if (nodeRange.m_min <= prevNodeRange.m_max || (nodeRange.m_min - prevNodeRange.m_max) <= m_toleranceDistance[uint32_t(axis)])
			{
				if (lastCluster != &cluster)
				{
					lastCluster->PushBack(cluster[i]);
					cluster[i] = nullptr;
				}
			}
			else
			{
				lastCluster = newClusters.PushBack() = m_allocator->Alloc<Cluster>();

				lastCluster->PushBack(cluster[i]);
				cluster[i] = nullptr;
			}

			prevNodeRange = nodeRange;
		}

		auto clusterCpy = cluster;
		cluster.Clear();

		for (auto& node : clusterCpy)
		{
			if(node != nullptr)
				cluster.PushBack(node);
		}
	}

	clusters += newClusters;
}

void RenderBoundingVolumeHierarchy::RecursiveBuildBottomUp(CLib::Vector<Node*>& nodes, CLib::Vector<NodeWave>& waves, uint32_t waveIdx, uint32_t passCount)
{
	CLib::Vector<Cluster*> clusters;

	Cluster* startCluster = clusters.PushBack() = m_allocator->Alloc<Cluster>();
	for (auto& node : nodes)
	{
		startCluster->PushBack(node);
	}

	// Increasing SplitPassCount will increase the amount of clusters produced.
	// An odd split pass count will most likely result in clusters extending further on one axis, so an even number is recommended.
	static constexpr uint32_t SplitPassCount = 2;

	for (uint32_t i = 0; i < SplitPassCount; ++i)
	{
		AxisSplitClusters(clusters, EProjectedAxis::X);
		AxisSplitClusters(clusters, EProjectedAxis::Y);
		AxisSplitClusters(clusters, EProjectedAxis::Z);
	}

	CLib::Vector<Node*> clusterNodes;
	clusterNodes.Reserve(clusters.Count());

	auto volumeCompare = [=](const Node* a, const Node* b)
	{
		return a->m_bounds.Volume() > b->m_bounds.Volume();
	};

	float largestClusterVolume = 0.0f;
	for (Cluster*& cluster : clusters)
	{
		if (cluster == nullptr)
			continue;

		std::sort(cluster->begin(), cluster->end(), volumeCompare);
		if(cluster->Count() > 1)
		{
			Cluster& c = *cluster;

			uint32_t prevIdx = 0;
			for (uint32_t i = 1; i < c.Count(); ++i)
			{
				Node*& cur = c[i];
				Node*& prev = c[prevIdx];
				if (cur->m_bounds.Encapsulates(prev->m_bounds))
				{
					cur->m_children.PushBack(prev);
					prevIdx = i;
					prev = nullptr;
				}
				else if (prev->m_bounds.Encapsulates(cur->m_bounds))
				{
					prev->m_children.PushBack(cur);
					cur = nullptr;
				}
			}

			auto cpy = c;
			c.Clear();

			for (auto& n : cpy)
			{
				if (n != nullptr)
					c.PushBack(n);
			}
		}

		assert(cluster->Count() > 0);
		if (cluster->Count() == 1)
		{
			// Cluster only has one node. Continue with the node as-is.
			clusterNodes.PushBack(cluster->Front());
			largestClusterVolume = glm::max<float>(largestClusterVolume, cluster->Front()->m_bounds.Volume());
		}
		else if (cluster->Count() > 3 && passCount <= 1)
		{
			// Cluster has many nodes and we're deep in the tree. Add nodes as a child of the last node to reduce complexity.
			Node* last = clusterNodes.PushBack() = cluster->PopBack();

			for (auto& child : *cluster)
			{
				last->m_bounds.Encapsulate(child->m_bounds);
				last->m_children.PushBack(child);
			}

			largestClusterVolume = glm::max<float>(largestClusterVolume, last->m_bounds.Volume());
		}
		else
		{
			// Cluster has few nodes, or is otherwise higher up in the tree. Create a new node and add all cluster nodes as children.
			Node* newNode = clusterNodes.PushBack() = m_allocator->Alloc<Node>();
			newNode->m_bounds = cluster->Front()->m_bounds;

			for (auto& child : *cluster)
			{
				newNode->m_bounds.Encapsulate(child->m_bounds);
				newNode->m_children.PushBack(child);
			}

			largestClusterVolume = glm::max<float>(largestClusterVolume, newNode->m_bounds.Volume());
		}

		m_allocator->Free(cluster);
		cluster = nullptr;
	}

	if (waveIdx < waves.Count() && (clusterNodes.Count() == 1 || largestClusterVolume >= waves[waveIdx].second))
	{
		clusterNodes += *waves[waveIdx].first;
		++waveIdx;
	}

	if ((clusterNodes.Count() > 1 || passCount < waves.Count() - 1) && passCount < 500)
	{
		m_toleranceDistance[0] += m_toleranceStep[0];
		m_toleranceDistance[1] += m_toleranceStep[1];
		m_toleranceDistance[2] += m_toleranceStep[2];

		RecursiveBuildBottomUp(clusterNodes, waves, waveIdx, passCount + 1);
	}
	else if (clusterNodes.Count() > 1)
	{
		m_root = m_allocator->Alloc<Node>();

		for (auto& child : clusterNodes)
		{
			m_root->m_bounds.Encapsulate(child->m_bounds);
			m_root->m_children.PushBack(child);
		}
	}
	else
	{
		m_root = clusterNodes[0];
	}
}

void RenderBoundingVolumeHierarchy::AssignDepth(Node* node, uint32_t depth)
{
	node->m_depth = depth;

	for (auto& n : node->m_children)
		AssignDepth(n, depth + 1);
}

void RenderBoundingVolumeHierarchy::RecursiveFreeNode(Node* node)
{
	for (Node* n : node->m_children)
		RecursiveFreeNode(n);

	m_allocator->Free(node);
}

void RenderBoundingVolumeHierarchy::ConstructCameraFrustrum(const Camera* camera, CameraFrustrum& outFrustrum)
{
	const Camera& cam = *camera;

	const glm::vec3 nearCentre = cam.Position() + (cam.Forward() * cam.ZNear());
	const glm::vec3 farCentre = cam.Position() + (cam.Forward() * cam.ZFar());

	const float nearFarHeight = cam.ZNear() * glm::tan(cam.FovY() * 0.5f);
	const float nearFarWidth = nearFarHeight * cam.Aspect();

	const float halfFarHeight = cam.ZFar() * glm::tan(cam.FovY() * 0.5f);
	const float halfFarWidth = halfFarHeight * cam.Aspect();

	outFrustrum.m_nearTopLeft = nearCentre + (cam.Up() * nearFarHeight) - (cam.Right() * nearFarWidth);
	outFrustrum.m_nearTopRight = nearCentre + (cam.Up() * nearFarHeight) + (cam.Right() * nearFarWidth);
	outFrustrum.m_nearBottomLeft = nearCentre - (cam.Up() * nearFarHeight) - (cam.Right() * nearFarWidth);
	outFrustrum.m_nearBottomRight = nearCentre - (cam.Up() * nearFarHeight) + (cam.Right() * nearFarWidth);

	outFrustrum.m_farTopLeft = farCentre + (cam.Up() * halfFarHeight) - (cam.Right() * halfFarWidth);
	outFrustrum.m_farTopRight = farCentre + (cam.Up() * halfFarHeight) + (cam.Right() * halfFarWidth);
	outFrustrum.m_farBottomLeft = farCentre - (cam.Up() * halfFarHeight) - (cam.Right() * halfFarWidth);
	outFrustrum.m_farBottomRight = farCentre - (cam.Up() * halfFarHeight) + (cam.Right() * halfFarWidth);

	const glm::vec3 leftNormal = glm::normalize
	(
		glm::cross
		(
			outFrustrum.m_farBottomLeft - outFrustrum.m_nearBottomLeft,
			outFrustrum.m_farTopLeft - outFrustrum.m_farBottomLeft
		)
	);
	const glm::vec3 rightNormal = glm::normalize
	(
		glm::cross
		(
			outFrustrum.m_farTopRight - outFrustrum.m_nearTopRight,
			outFrustrum.m_farBottomRight - outFrustrum.m_farTopRight
		)
	);
	const glm::vec3 topNormal = glm::normalize
	(
		glm::cross
		(
			outFrustrum.m_farTopLeft - outFrustrum.m_nearTopLeft,
			outFrustrum.m_farTopRight - outFrustrum.m_farTopLeft
		)
	);
	const glm::vec3 bottomNormal = glm::normalize
	(
		glm::cross
		(
			outFrustrum.m_farBottomRight - outFrustrum.m_nearBottomRight,
			outFrustrum.m_farBottomLeft - outFrustrum.m_farBottomRight
		)
	);

	outFrustrum.m_left = { leftNormal, glm::dot(outFrustrum.m_farTopLeft, leftNormal) };
	outFrustrum.m_right = { rightNormal, glm::dot(outFrustrum.m_farBottomRight, rightNormal) };

	outFrustrum.m_top = { topNormal, glm::dot(outFrustrum.m_farTopLeft, topNormal)};
	outFrustrum.m_bottom = { bottomNormal, glm::dot(outFrustrum.m_farBottomLeft, bottomNormal) };

	const float projectedNear = glm::dot(cam.Position() + (cam.Forward() * cam.ZNear()), -cam.Forward());
	const float projectedFar = glm::dot(cam.Position() + (cam.Forward() * cam.ZFar()), cam.Forward());

	outFrustrum.m_near = { -cam.Forward(), projectedNear };
	outFrustrum.m_far = { cam.Forward(), projectedFar };

	glm::mat4 noTranslate = glm::translate(glm::mat4(), cam.Position() * glm::vec3(2.0f, 0.0f, 2.0f));
	glm::mat4 trans = glm::rotate(noTranslate, glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));

	outFrustrum.m_nearTopLeft = trans * glm::vec4(outFrustrum.m_nearTopLeft, 1.0f);
	outFrustrum.m_nearTopRight = trans * glm::vec4(outFrustrum.m_nearTopRight, 1.0f);
	outFrustrum.m_nearBottomLeft = trans * glm::vec4(outFrustrum.m_nearBottomLeft, 1.0f);
	outFrustrum.m_nearBottomRight = trans * glm::vec4(outFrustrum.m_nearBottomRight, 1.0f);

	outFrustrum.m_farTopLeft = trans * glm::vec4(outFrustrum.m_farTopLeft, 1.0f);
	outFrustrum.m_farTopRight = trans * glm::vec4(outFrustrum.m_farTopRight, 1.0f);
	outFrustrum.m_farBottomLeft = trans * glm::vec4(outFrustrum.m_farBottomLeft, 1.0f);
	outFrustrum.m_farBottomRight = trans * glm::vec4(outFrustrum.m_farBottomRight, 1.0f);
}

bool RenderBoundingVolumeHierarchy::IsInFrontOfPlane(const Plane& plane, const Bounds& bounds)
{
	const glm::vec3 centre = bounds.Centre();
	const glm::vec3 halfExtents = bounds.m_extents * 0.5f;

	const float r = halfExtents.x * std::abs(plane.m_normal.x) +
		halfExtents.y * std::abs(plane.m_normal.y) + halfExtents.z * std::abs(plane.m_normal.z);

	const float signedDist = glm::dot(plane.m_normal, centre) - plane.m_distance;

	return -r <= signedDist;
}

bool RenderBoundingVolumeHierarchy::FrustrumTest(const CameraFrustrum& frustrum, const Bounds& bounds)
{
	return IsInFrontOfPlane(frustrum.m_near, bounds)
		&& IsInFrontOfPlane(frustrum.m_left, bounds)
		&& IsInFrontOfPlane(frustrum.m_right, bounds)
		&& IsInFrontOfPlane(frustrum.m_top, bounds)
		&& IsInFrontOfPlane(frustrum.m_bottom, bounds);
}

void RenderBoundingVolumeHierarchy::RecursiveDebugDrawNode(DebugLinePass* lines, const CameraFrustrum& frustrum, Node* node, uint32_t depth, bool drawObjectBounds)
{
	glm::vec3 lineColor = node->m_isObject ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
	lineColor = (depth != ~uint32_t(0) && node->m_depth == depth) ? glm::vec3(0.0f, 0.0f, 1.0f) : lineColor;

	const glm::vec3& origin = node->m_bounds.m_origin;
	const glm::vec3& extents = node->m_bounds.m_extents;

	// Origin to sky
	if (node == m_root)
	{
		const Bounds& b = node->m_bounds;

		lines->DrawLine(PB::Float3(origin.x, origin.y - 1000.0f, origin.z), PB::Float3(origin.x, origin.y + 1000.0f, origin.z), PB::Float3(0.0f, 0.0f, 1.0f));
		lines->DrawLine(PB::Float3(origin.x + extents.x, origin.y + extents.y - 1000.0f, origin.z + extents.z), PB::Float3(origin.x + extents.x, origin.y + extents.y + 1000.0f, origin.z + extents.z), PB::Float3(0.0f, 0.0f, 1.0f));
	}

	if (m_camera != nullptr && FrustrumTest(frustrum, node->m_bounds) == false)
	{
		return;
	}

	if ((node->m_depth == depth && !node->IsLeaf()) || node->m_depth == (depth + 1) || depth == ~uint32_t(0))
	{
		DebugDrawCube(lines, origin, extents, lineColor);

		if (drawObjectBounds && (origin != node->m_objectOrigin || extents != node->m_objectExtents))
			DebugDrawCube(lines, node->m_objectOrigin, node->m_objectExtents, glm::vec3(1.0f, 0.0f, 1.0f));
	}

	if (node->m_depth <= depth)
	{
		for (Node* n : node->m_children)
			RecursiveDebugDrawNode(lines, frustrum, n, depth, drawObjectBounds);
	}
}

void RenderBoundingVolumeHierarchy::DebugDrawCube(DebugLinePass* lines, const glm::vec3& origin, const glm::vec3& extents, const glm::vec3& lineColor)
{
	PB::Float3 color3(lineColor.r, lineColor.g, lineColor.b);

	// Bottom square
	lines->DrawLine(PB::Float3(origin.x, origin.y, origin.z), PB::Float3(origin.x + extents.x, origin.y, origin.z), color3);
	lines->DrawLine(PB::Float3(origin.x, origin.y, origin.z), PB::Float3(origin.x, origin.y, origin.z + extents.z), color3);
	lines->DrawLine(PB::Float3(origin.x + extents.x, origin.y, origin.z), PB::Float3(origin.x + extents.x, origin.y, origin.z + extents.z), color3);
	lines->DrawLine(PB::Float3(origin.x + extents.x, origin.y, origin.z + extents.z), PB::Float3(origin.x, origin.y, origin.z + extents.z), color3);

	// Top square
	lines->DrawLine(PB::Float3(origin.x, origin.y + extents.y, origin.z), PB::Float3(origin.x + extents.x, origin.y + extents.y, origin.z), color3);
	lines->DrawLine(PB::Float3(origin.x, origin.y + extents.y, origin.z), PB::Float3(origin.x, origin.y + extents.y, origin.z + extents.z), color3);
	lines->DrawLine(PB::Float3(origin.x + extents.x, origin.y + extents.y, origin.z), PB::Float3(origin.x + extents.x, origin.y + extents.y, origin.z + extents.z), color3);
	lines->DrawLine(PB::Float3(origin.x + extents.x, origin.y + extents.y, origin.z + extents.z), PB::Float3(origin.x, origin.y + extents.y, origin.z + extents.z), color3);

	// Columns
	lines->DrawLine(PB::Float3(origin.x, origin.y, origin.z), PB::Float3(origin.x, origin.y + extents.y, origin.z), color3);
	lines->DrawLine(PB::Float3(origin.x + extents.x, origin.y, origin.z), PB::Float3(origin.x + extents.x, origin.y + extents.y, origin.z), color3);
	lines->DrawLine(PB::Float3(origin.x, origin.y, origin.z + extents.z), PB::Float3(origin.x, origin.y + extents.y, origin.z + extents.z), color3);
	lines->DrawLine(PB::Float3(origin.x + extents.x, origin.y, origin.z + extents.z), PB::Float3(origin.x + extents.x, origin.y + extents.y, origin.z + extents.z), color3);
}

uint64_t RenderBoundingVolumeHierarchy::GetScore(const Bounds& bounds)
{
	// TODO: To produce better results, it may be worth biasing axes e.g. valuing taller volumes less that wider and longer volumes for mostly flat levels.
	//		 In an engine-level implementation, axes could be biased based on which angles a player is most likely to be looking at any given time at the location of insertion e.g. looking up/down more often in taller locations.

	glm::vec3 dimensions = glm::vec3
	(
		glm::abs(bounds.m_origin.x - bounds.MaxX()),
		glm::abs(bounds.m_origin.y - bounds.MaxY()),
		glm::abs(bounds.m_origin.z - bounds.MaxZ())
	);
	return uint64_t(dimensions.x * dimensions.y * dimensions.z);
}

void RenderBoundingVolumeHierarchy::Node::RemoveChild(Node* child)
{
	for (auto& c : m_children)
	{
		if (c == child)
		{
			if (c == m_children.Back())
				m_children.PopBack();
			else // Preserving order does not matter. Just move the back element to overwrite 'c'.
				c = m_children.PopBack();

			return;
		}
	}
}
