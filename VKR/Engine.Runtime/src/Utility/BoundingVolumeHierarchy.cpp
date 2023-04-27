#include "BoundingVolumeHierarchy.h"
#include "RenderGraphPasses/DebugLinePass.h"

#include "glm/gtc/type_ptr.hpp"
#include <algorithm>

namespace Eng
{
	BoundingVolumeHierarchy::BoundingVolumeHierarchy(const CreateDesc& desc)
	{
		Init(desc);
	}

	void BoundingVolumeHierarchy::Init(const CreateDesc& desc)
	{
		if (m_nodeAllocator == nullptr)
			m_nodeAllocator = new CLib::FixedBlockAllocator(sizeof(BuildNode) + sizeof(NodeData));

		m_origToleranceDistance[0] = m_toleranceDistance[0] = desc.m_toleranceDistanceX;
		m_origToleranceDistance[1] = m_toleranceDistance[1] = desc.m_toleranceDistanceY;
		m_origToleranceDistance[2] = m_toleranceDistance[2] = desc.m_toleranceDistanceZ;

		m_toleranceStep[0] = desc.m_toleranceStepX;
		m_toleranceStep[1] = desc.m_toleranceStepY;
		m_toleranceStep[2] = desc.m_toleranceStepZ;
	}

	BoundingVolumeHierarchy::~BoundingVolumeHierarchy()
	{

	}

	void BoundingVolumeHierarchy::Build()
	{
		m_root = this->BuildBottomUp(m_input, 0);
		m_input.Clear();
	}

	void BoundingVolumeHierarchy::Destroy()
	{
		if (m_root != nullptr)
		{
			RecursiveFreeNode(m_root);
		}

		if (m_nodeAllocator != nullptr)
		{
			delete m_nodeAllocator;
		}
	}

	const BoundingVolumeHierarchy::ObjectData* BoundingVolumeHierarchy::RaycastGetObjectData(DebugLinePass* lines, const glm::vec3& rayOrigin, const glm::vec3& rayDirection)
	{
		BuildNode* result = RecursiveObjectRayIntersection(m_root, rayOrigin, rayDirection).first;
		if (result)
		{
			auto* data = result->m_objectData;
			DebugDrawCube(lines, data->m_bounds.m_origin, data->m_bounds.m_extents, glm::vec3(0.0f, 1.0f, 0.0f));
			return data;
		}
		return nullptr;
	}

	void BoundingVolumeHierarchy::AddObject(const ObjectData* objectData)
	{
		assert(m_objectNodeMap.find(objectData) == m_objectNodeMap.end());

		m_input.PushBack(objectData);
		m_objectNodeMap.insert({objectData, nullptr});
	}

	void BoundingVolumeHierarchy::UpdateObject(const ObjectData* objectData)
	{
		assert(m_objectNodeMap.find(objectData) != m_objectNodeMap.end() && m_objectNodeMap[objectData] != nullptr);
		m_input.PushBack(objectData);
	}

	void BoundingVolumeHierarchy::RemoveObject(const ObjectData* objectData)
	{
		auto it = m_objectNodeMap.find(objectData);
		if (it != m_objectNodeMap.end())
		{
			m_erasureObjects.PushBack(objectData);
		}
	}

	BoundingVolumeHierarchy::BuildNode* BoundingVolumeHierarchy::BuildBottomUp(CLib::Vector<const ObjectData*>& objects, uint32_t baseDepth)
	{
		CLib::Vector<BuildNode*> nodes;
		nodes.Reserve(objects.Count());
		m_totalNodeCount = objects.Count();

		for (const ObjectData*& obj : objects)
		{
			BuildNode* n = AllocateBuildNode();
			nodes.PushBack(n);
			BuildNode& node = *n;

			m_objectNodeMap[obj] = n;

			node.m_parent = nullptr;
			node.m_bounds = obj->m_bounds;
			node.m_objectData = obj;
			node.m_depth = 0;
			node.m_isObject = true;
		}

		BuildNode* result = BuildBottomUpInternal(nodes);
		AssignDepth(result, baseDepth);

		return result;
	}

	void BoundingVolumeHierarchy::RebuildSubTree(BuildNode* subtreeRoot)
	{
		CLib::Vector<const ObjectData*> objects;
		RecursiveGetObjectData(objects, subtreeRoot);

		printf_s("Rebuilding subtree at depth: %u\n", subtreeRoot->m_depth);
		BuildNode* newSubtree = this->BuildBottomUp(objects, subtreeRoot->m_depth);
		newSubtree->m_parent = subtreeRoot->m_parent;

		// Replace old subtree with new one in parent.
		BuildNode* parent = newSubtree->m_parent;
		if (parent != nullptr)
		{
			for (auto& child : parent->m_children)
			{
				if (child == subtreeRoot)
				{
					child = newSubtree;
					break;
				}
			}
		}
		else
		{
			// New subtree is not a subtree. The whole tree has been rebuilt and it is the new root.
			assert(newSubtree->m_depth == 0);
			m_root = newSubtree;
		}

		// Free old subtree.
		RecursiveFreeNode(subtreeRoot);
	}

	void BoundingVolumeHierarchy::UpdateTree()
	{
		if (m_input.Count() == 0 && m_erasureObjects.Count() == 0)
			return;

		CLib::Vector<BuildNode*> subtreesToRebuild;
		GetRebuildCandidates(subtreesToRebuild, m_input, m_erasureObjects);
		m_input.Clear();

		for (auto& erasureObj : m_erasureObjects)
		{
			auto it = m_objectNodeMap.find(erasureObj);
			if(it == m_objectNodeMap.end())
				continue;

			// No longer treating the node as an object node will prevent it from being included in any subtree rebuild.
			BuildNode* erasureNode = it->second;
			erasureNode->m_isObject = false;

			m_objectNodeMap.erase(it);
		}
		m_erasureObjects.Clear();

		for (auto& subtree : subtreesToRebuild)
		{
			RebuildSubTree(subtree);
		}
	}

	void BoundingVolumeHierarchy::DebugDraw(const Camera* camera, DebugLinePass* lines, uint32_t depth, bool drawObjectBounds) const
	{
		Camera::CameraFrustrum frustrum;
		if (camera != nullptr)
		{
			frustrum = camera->GetFrustrum();
			// Draw frustrum
			glm::vec3 frustrumColor(1.0f, 0.0f, 1.0f);
			Camera::DrawFrustrum(lines, frustrum, frustrumColor);
		}

		RecursiveDebugDrawNode(lines, frustrum, m_root, depth, drawObjectBounds);
	}

	float BoundingVolumeHierarchy::GetClusterScore(const BuildNode* a, std::vector<BuildNode*>& pool)
	{
		float total = 0.0f;
		for (const BuildNode* b : pool)
		{
			if (b != a)
			{
				float dist = glm::distance(a->m_bounds.Centre(), b->m_bounds.Centre());
				total -= dist;
			}
		}

		return total - GetScore(a->m_bounds);
	}

	void BoundingVolumeHierarchy::AxisSplitClusters(ClusterArray& clusters, EProjectedAxis axis)
	{
		const auto rangeCompare = [=](const BuildNode* a, const BuildNode* b) -> bool
		{
			return a->GetRange(axis).m_min < b->GetRange(axis).m_min;
		};

		// For each cluster, split any nodes within which are too far apart on the provided axis into new clusters.
		CLib::Vector<Cluster*, 32, 32> newClusters;
		for (Cluster* c : clusters)
		{
			Cluster& srcClusterToSplit = *c;
			std::sort(srcClusterToSplit.begin(), srcClusterToSplit.end(), rangeCompare);
			Cluster* currentCluster = c;

			uint32_t srcSplitCount = srcClusterToSplit.Count();
			ProjectedRange currentClusterRange = srcClusterToSplit[0]->GetRange(axis);
			for (uint32_t i = 1; i < srcClusterToSplit.Count(); ++i)
			{
				BuildNode* currentNode = srcClusterToSplit[i];
				ProjectedRange nodeRange = currentNode->GetRange(axis);
				assert(nodeRange.m_min >= currentClusterRange.m_min);

				float gap = nodeRange.m_min - currentClusterRange.m_max;
				bool acceptableGap = gap <= m_toleranceDistance[uint32_t(axis)] && currentCluster->Count() < ChildSoftLimit;
				bool overlap = nodeRange.m_min <= currentClusterRange.m_max && nodeRange.m_max >= currentClusterRange.m_min;

				if (overlap || acceptableGap)
				{
					// Do not split as the range is close enough to the cluster to remain within it.
					// If the current cluster is the source cluster. Leave it rather than adding the node twice.
					if(currentCluster != c)
						currentCluster->PushBack(currentNode);

					currentClusterRange.m_max = glm::max<float>(currentClusterRange.m_max, nodeRange.m_max);
				}
				else
				{
					// If the current cluster is the source cluster, we will set its count to contain only the nodes which were not split off.
					if (currentCluster == c)
					{
						srcSplitCount = i;
					}

					// Split current node into a new cluster.
					currentCluster = newClusters.PushBack() = m_nodeAllocator->Alloc<Cluster>();
					currentCluster->PushBack(currentNode);

					currentClusterRange = currentNode->GetRange(axis);
				}
			}

			srcClusterToSplit.SetCount(srcSplitCount);
		}

		clusters += newClusters;
	}

	BoundingVolumeHierarchy::BuildNode* BoundingVolumeHierarchy::BuildBottomUpInternal(CLib::Vector<BuildNode*>& nodes)
	{
		// The tree is built using "waves" which are vectors of nodes which are built in separate recursive passes.
		// The idea is to prevent nodes with large differences in scale from being clustered together.
		// Nodes from a wave which have been clustered will then be moved to the next cluster with nodes of a similar scale to be clustered with them.
		// Which wave a node belongs to is determined by its largest scale relative to other nodes.
		CLib::Vector<NodeWave> waves(nodes.Count());

		// Create waves...
		{
			// Sort by largest scale on any axis from smallest node to largest.
			const auto scaleCompare = [=](const BuildNode* a, const BuildNode* b)
			{
				return a->GetLargestScale() < b->GetLargestScale();
			};
			std::sort(nodes.begin(), nodes.end(), scaleCompare);

			float prevScale = 0.0f;
			Cluster* currentGroup = nullptr;
			for (auto& n : nodes)
			{
				float scale = n->GetLargestScale();

				if (scale - prevScale > (prevScale * WaveScalePercentageThreshold) || currentGroup == nullptr)
				{
					NodeWave& newWave = waves.PushBack();
					newWave.second = scale;

					currentGroup = newWave.first = m_nodeAllocator->Alloc<Cluster>(32);
					currentGroup->PushBack(n);
					n = nullptr;

					prevScale = scale;
				}
				else if (currentGroup != nullptr)
				{
					currentGroup->PushBack(n);
					n = nullptr;
				}
			}
		}

		// Reverse waves to order them from largest wave scale to smallest. Each pass in the recursive algorithm will pop a wave from the back of the vector to cluster.
		assert(waves.Count() > 0);
		std::reverse(waves.begin(), waves.end());

		BuildNode* root = nullptr;
		root = RecursiveBuildBottomUp(waves);

		m_toleranceDistance[0] = m_origToleranceDistance[0];
		m_toleranceDistance[1] = m_origToleranceDistance[1];
		m_toleranceDistance[2] = m_origToleranceDistance[2];

		for (NodeWave& wave : waves)
		{
			m_nodeAllocator->Free(wave.first);
		}

		return root;
	}

	BoundingVolumeHierarchy::BuildNode* BoundingVolumeHierarchy::RecursiveBuildBottomUp(CLib::Vector<NodeWave>& waves, uint32_t passCount)
	{
		NodeWave wave = waves.PopBack();
		Cluster& waveCluster = *wave.first;
		{
			ClusterArray clusters{};

			// Place all nodes in one cluster to begin with.
			Cluster* startCluster = clusters.PushBack() = m_nodeAllocator->Alloc<Cluster>();
			for (uint32_t i = 0; i < waveCluster.Count(); ++i)
			{
				startCluster->PushBack(waveCluster[i]);
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

			Cluster* clusterNodes = m_nodeAllocator->Alloc<Cluster>();
			clusterNodes->Reserve(clusters.Count());

			// Convert clusters to nodes...
			float largestClusterScale = 0.0f;
			for (Cluster*& cluster : clusters)
			{
				if (cluster == nullptr)
					continue;

				assert(cluster->Count() > 0);
				if (cluster->Count() == 1)
				{
					// Cluster only has one node. Continue with the node as-is.
					clusterNodes->PushBack(cluster->Front());
				}
				else
				{
					// Cluster has multiple nodes. Place them under a new parent node, and continue with the parent.
					BuildNode* newNode = clusterNodes->PushBack() = AllocateBuildNode();
					newNode->m_bounds = (*cluster)[0]->m_bounds;

					for (auto& child : *cluster)
					{
						child->m_parent = newNode;
						newNode->m_bounds.Encapsulate(child->m_bounds);
						newNode->m_children.PushBack(child);
					}
				}

				m_nodeAllocator->Free(cluster);
				cluster = nullptr;

				BuildNode& convertedCluster = *clusterNodes->Back();
				const float largestScale = convertedCluster.GetLargestScale();
				largestClusterScale = glm::max<float>(largestClusterScale, largestScale);
			}

			if (waves.Count() > 0)
			{
				// Insert nodes into an appropriate wave, based on scale.
				bool foundWave = false;
				for (NodeWave& wave : waves)
				{
					if (largestClusterScale - wave.second <= (wave.second * WaveScalePercentageThreshold))
					{
						*wave.first += *clusterNodes;
						foundWave = true;
						break;
					}
				}

				if (foundWave == false)
				{
					*waves.Back().first += *clusterNodes;
				}

				m_nodeAllocator->Free(clusterNodes);
			}
			else
			{
				// If there are no waves left, make one.
				waves.PushBack({ clusterNodes, largestClusterScale });
			}
		}
		m_nodeAllocator->Free(wave.first);

		BuildNode* root = nullptr;

		printf("Pass Count: %u\n", passCount);
		assert(waves.Count() > 0);

		// Finish condition is one wave remaining with one node (the root) inside.
		bool finished = waves.Count() == 1 && waves.Front().first->Count() == 1;
		if (finished == false && passCount < 300)
		{
			m_toleranceDistance[0] += m_toleranceStep[0];
			m_toleranceDistance[1] += m_toleranceStep[1];
			m_toleranceDistance[2] += m_toleranceStep[2];

			root = RecursiveBuildBottomUp(waves, passCount + 1);
		}
		else if (finished == false)
		{
			root = AllocateBuildNode();

			for (auto& wave : waves)
			{
				for (auto& waveNode : *wave.first)
				{
					waveNode->m_parent = root;
					root->m_bounds.Encapsulate(waveNode->m_bounds);
					root->m_children.PushBack(waveNode);
				}
			}
		}
		else
		{
			root = waves.Front().first->Front();
		}

		return root;
	}

	void BoundingVolumeHierarchy::AssignDepth(BuildNode* node, uint32_t depth)
	{
		node->m_depth = depth;

		for (auto& n : node->m_children)
			AssignDepth(n, depth + 1);
	}

	BoundingVolumeHierarchy::BuildNode* BoundingVolumeHierarchy::FindNewObjectParent(const ObjectData* object)
	{
		m_root->m_bounds.Encapsulate(object->m_bounds);
		BuildNode* encapsulatingParent = m_root;
		bool foundNewParent = true;
		while (foundNewParent)
		{
			foundNewParent = false;
			for (auto& child : encapsulatingParent->m_children)
			{
				if (child->m_bounds.Encapsulates(object->m_bounds))
				{
					encapsulatingParent = child;
					foundNewParent = true;
					break;
				}
			}
		}

		return encapsulatingParent;
	}

	std::pair<BoundingVolumeHierarchy::BuildNode*, BoundingVolumeHierarchy::BuildNode*> BoundingVolumeHierarchy::RebuildOneOrBoth(BuildNode* a, BuildNode* b)
	{
		if (a == b)
		{
			// Object has not changed parents. Only rebuild the one parent.
			return { a, nullptr };
		}
		else
		{
			if (a->m_depth == b->m_depth)
			{
				// Object changed parents but the depth is the same, meaning the object has changed branches in the tree.
				// Rebuild both parents.
				return { a, b };
			}
			else
			{
				// 1. Find which of the two parents is deeper in the tree and which is shallower.
				// 2. Traverse up from the deeper node until the depth of the shallower node is reached.
				// 3. If the node at the shallower node's depth is not the shallower node, rebuild both. Otherwise rebuild just the shallower node.

				BuildNode* shallowestNode = nullptr;
				BuildNode* deepestNode = nullptr;
				if (a->m_depth > b->m_depth)
				{
					deepestNode = a;
					shallowestNode = b;
				}
				else
				{
					deepestNode = b;
					shallowestNode = a;
				}

				BuildNode* currentNode = deepestNode;
				while (currentNode->m_depth > shallowestNode->m_depth)
				{
					currentNode = currentNode->m_parent;
				}

				if (currentNode == shallowestNode)
				{
					return { currentNode, nullptr };
				}
				else
				{
					return { shallowestNode, currentNode };
				}
			}
		}
	}

	void BoundingVolumeHierarchy::GetRebuildCandidates(CLib::Vector<BuildNode*>& outNodes, InputObjects& objects, InputObjects& erasureObjects)
	{
		CLib::Vector<BuildNode*, 16, 16> rebuildCandidates;
		for (auto& obj : objects)
		{
			auto it = m_objectNodeMap.find(obj);
			bool isNew = it == m_objectNodeMap.end();

			BuildNode* newParent = FindNewObjectParent(obj);
			if (isNew == false)
			{
				BuildNode* oldParent = it->second->m_parent;
				auto [a, b] = RebuildOneOrBoth(newParent, oldParent);
				rebuildCandidates.PushBack(a);
				if (b != nullptr)
				{
					rebuildCandidates.PushBack(b);
				}
			}
			else
			{
				rebuildCandidates.PushBack(newParent);
			}
		}

		for (auto& obj : erasureObjects)
		{
			rebuildCandidates.PushBack(m_objectNodeMap[obj]->m_parent);
		}

		// Eliminate redundant rebuild candidates.
		for (uint32_t i = 0; i < rebuildCandidates.Count();)
		{
			if (i >= rebuildCandidates.Count() - 1)
			{
				outNodes.PushBack(rebuildCandidates.Back());
				break;
			}

			BuildNode*& current = rebuildCandidates[i];
			BuildNode*& next = rebuildCandidates[i + 1];

			auto [a, b] = RebuildOneOrBoth(current, next);
			outNodes.PushBack(a);
			
			i += b != nullptr ? 1 : 2;
		}
		
		// All of the nodes in outNodes should be possible to rebuild individually (possibly in parallel) without conflict.
	}

	void BoundingVolumeHierarchy::RecursiveGetObjectData(InputObjects& outObjectData, BuildNode* node)
	{
		if (node->m_isObject)
		{
			outObjectData.PushBack(node->m_objectData);
		}

		for (auto& child : node->m_children)
			RecursiveGetObjectData(outObjectData, child);
	}

	void BoundingVolumeHierarchy::RecursiveFreeNode(BuildNode* node)
	{
		for (BuildNode* child : node->m_children)
		{
			RecursiveFreeNode(child);
		}

		FreeBuildNode(node);
	}

	bool BoundingVolumeHierarchy::IsInFrontOfPlane(const Camera::CameraFrustrum::Plane& plane, const Bounds& bounds) const
	{
		const glm::vec3 centre = bounds.Centre();
		const glm::vec3 halfExtents = bounds.m_extents * 0.5f;

		const float r = halfExtents.x * std::abs(plane.x) +
			halfExtents.y * std::abs(plane.y) + halfExtents.z * std::abs(plane.z);

		const float signedDist = glm::dot(glm::vec3(plane), centre) - plane.w;

		return -r <= signedDist;
	}

	bool BoundingVolumeHierarchy::FrustrumTest(const Camera::CameraFrustrum& frustrum, const Bounds& bounds) const
	{
		return IsInFrontOfPlane(frustrum.m_near, bounds)
			&& IsInFrontOfPlane(frustrum.m_left, bounds)
			&& IsInFrontOfPlane(frustrum.m_right, bounds)
			&& IsInFrontOfPlane(frustrum.m_top, bounds)
			&& IsInFrontOfPlane(frustrum.m_bottom, bounds)
			&& IsInFrontOfPlane(frustrum.m_far, bounds);
	}

	std::pair<BoundingVolumeHierarchy::BuildNode*, float> BoundingVolumeHierarchy::RecursiveObjectRayIntersection(BuildNode* node, const glm::vec3& rayOrigin, const glm::vec3& rayDirection)
	{
		using Hit = std::pair<BuildNode*, float>;
		static const Hit NoHit = { nullptr, INFINITY };

		assert(node->m_isObject == false);

		// Search node children for any ray hits.
		Hit closestHit = NoHit;						// Track closest hit to narrow search.
		CLib::Vector<Hit, 8> intersectingChildren;	// Track all child hits in case the closest one does not yield an object hit.
		{
			for (BuildNode*& child : node->m_children)
			{
				float dist;
				if (child->m_bounds.IsIntersectingWithRay(rayOrigin, rayDirection, dist))
				{
					if (dist < closestHit.second)
						closestHit = { child, dist };

					intersectingChildren.PushBack({ child, dist });
				}
			}
		}

		if (closestHit != NoHit)
		{
			// If the closest hit is an object. Return it.
			if (closestHit.first->m_isObject)
				return closestHit;

			// If the closest hit is a parent node, find the closest child object the ray hits (if any), and return it.
			Hit hitObject = RecursiveObjectRayIntersection(closestHit.first, rayOrigin, rayDirection);
			if (hitObject.first != nullptr)
			{
				assert(hitObject.first->m_isObject == true);
				return hitObject;
			}

			// If no object under the closest node was hit, search other hit children from closest to furthest.
			static constexpr auto distanceCompare = [](const Hit& a, const Hit& b)
			{
				return a.second < b.second;
			};
			std::sort(intersectingChildren.begin(), intersectingChildren.end(), distanceCompare);

			for (Hit& childHit : intersectingChildren)
			{
				if (childHit.first->m_isObject)
					hitObject = childHit;
				else
					hitObject = RecursiveObjectRayIntersection(childHit.first, rayOrigin, rayDirection);

				if (hitObject.first != nullptr)
				{
					assert(hitObject.first->m_isObject == true);
					return hitObject;
				}
			}
		}

		return NoHit;
	}

	void BoundingVolumeHierarchy::RecursiveDebugDrawNode(DebugLinePass* lines, const Camera::CameraFrustrum& frustrum, BuildNode* node, uint32_t depth, bool drawObjectBounds) const
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

		if (FrustrumTest(frustrum, node->m_bounds) == false)
		{
			return;
		}

		if (node->m_depth <= depth)
		{
			for (BuildNode* n : node->m_children)
				RecursiveDebugDrawNode(lines, frustrum, n, depth, drawObjectBounds);
		}

		if ((node->m_depth == depth && !node->IsLeaf()) || node->m_depth == (depth + 1) || depth == ~uint32_t(0))
		{
			DebugDrawCube(lines, origin, extents, lineColor);
		}
	}

	void BoundingVolumeHierarchy::DebugDrawCube(DebugLinePass* lines, const glm::vec3& origin, const glm::vec3& extents, const glm::vec3& lineColor) const
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

	BoundingVolumeHierarchy::BuildNode* BoundingVolumeHierarchy::AllocateBuildNode()
	{
		BuildNode* newNode = m_nodeAllocator->Alloc<BuildNode>();
		AllocateNodeData(newNode);
		return newNode;
	}

	void BoundingVolumeHierarchy::FreeBuildNode(BuildNode* node)
	{
		FreeNodeData(node);
		m_nodeAllocator->Free(node);
	}

	uint64_t BoundingVolumeHierarchy::GetScore(const Bounds& bounds)
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

	float BoundingVolumeHierarchy::BuildNode::GetLargestScale() const
	{
		const float scaleX = m_bounds.m_extents.x;
		const float scaleY = m_bounds.m_extents.y;
		const float scaleZ = m_bounds.m_extents.z;

		return glm::max<float>(glm::max<float>(scaleX, scaleY), scaleZ);
	}

	void BoundingVolumeHierarchy::BuildNode::RemoveChild(BuildNode* child)
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

};