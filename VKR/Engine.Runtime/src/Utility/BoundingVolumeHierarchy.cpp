#include "BoundingVolumeHierarchy.h"
#include "RenderGraphPasses/DebugLinePass.h"

#include "glm/gtc/type_ptr.hpp"
#include <algorithm>

namespace Eng
{
	BoundingVolumeHierarchy::BoundingVolumeHierarchy(CLib::Allocator* allocator, const CreateDesc& desc)
	{
		Init(allocator, desc);
	}

	void BoundingVolumeHierarchy::Init(CLib::Allocator* allocator, const CreateDesc& desc)
	{
		m_allocator = allocator;

		m_desiredMaxDepth = desc.m_desiredMaxDepth;

		m_toleranceDistance[0] = desc.m_toleranceDistanceX;
		m_toleranceDistance[1] = desc.m_toleranceDistanceY;
		m_toleranceDistance[2] = desc.m_toleranceDistanceZ;

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
			assert(it != m_objectNodeMap.end());

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

	void BoundingVolumeHierarchy::GenClusters(CLib::Vector<BuildNode*>& nodes, CLib::Vector<Cluster*>& outClusters, EProjectedAxis axis)
	{
		const auto rangeCompare = [=](const BuildNode* a, const BuildNode* b) -> bool
		{
			return a->GetRange(axis).m_min < b->GetRange(axis).m_min;
		};
		std::sort(nodes.begin(), nodes.end(), rangeCompare);
	}

	void BoundingVolumeHierarchy::AxisSplitClusters(ClusterArray& clusters, EProjectedAxis axis)
	{
		const auto rangeCompare = [=](const BuildNode* a, const BuildNode* b) -> bool
		{
			return a->GetRange(axis).m_min < b->GetRange(axis).m_min;
		};

		CLib::Vector<Cluster*, 32, 32> newClusters;
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
				if (node != nullptr)
					cluster.PushBack(node);
			}
		}

		clusters += newClusters;
	}

	BoundingVolumeHierarchy::BuildNode* BoundingVolumeHierarchy::BuildBottomUpInternal(CLib::Vector<BuildNode*>& nodes)
	{
		const auto compare = [=](const BuildNode* a, const BuildNode* b)
		{
			return a->m_bounds.Volume() < b->m_bounds.Volume();
		};
		std::sort(nodes.begin(), nodes.end(), compare);

		// Represents nodes who's insertion/cluster detection has been deferred to higher up the tree
		// due to large size relative to previous nodes.
		//
		// deferredNodes is a 2D array of nodes, where each array contains nodes X% larger volume than the previous arrays's largest node.
		CLib::Vector<NodeWave> deferredNodes(nodes.Count());

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

				currentGroup = newWave.first = m_allocator->Alloc<Cluster>(32);
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

		BuildNode* root = nullptr;
		if(deferredNodes.Count() > 0)
			root = RecursiveBuildBottomUp(*deferredNodes[0].first, deferredNodes, 1, 0);

		for (NodeWave& wave : deferredNodes)
		{
			m_allocator->Free(wave.first);
		}

		return root;
	}

	BoundingVolumeHierarchy::BuildNode* BoundingVolumeHierarchy::RecursiveBuildBottomUp(const CLib::Vector<BuildNode*>& nodes, CLib::Vector<NodeWave>& waves, uint32_t waveIdx, uint32_t passCount)
	{
		CLib::Vector<BuildNode*> clusterNodes{};
		{
			ClusterArray clusters{};

			Cluster* startCluster = clusters.PushBack() = m_allocator->Alloc<Cluster>();
			for (uint32_t i = 0; i < nodes.Count(); ++i)
			{
				startCluster->PushBack(nodes[i]);
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

			clusterNodes.Reserve(clusters.Count());

			const auto volumeCompare = [=](const BuildNode* a, const BuildNode* b)
			{
				return a->m_bounds.Volume() > b->m_bounds.Volume();
			};

			float largestClusterVolume = 0.0f;
			for (Cluster*& cluster : clusters)
			{
				if (cluster == nullptr)
					continue;

				std::sort(cluster->begin(), cluster->end(), volumeCompare);
				if (cluster->Count() > 1)
				{
					Cluster& c = *cluster;

					uint32_t prevIdx = 0;
					for (uint32_t i = 1; i < c.Count(); ++i)
					{
						BuildNode*& cur = c[i];
						BuildNode*& prev = c[prevIdx];
						if (cur->m_bounds.Encapsulates(prev->m_bounds))
						{
							prev->m_parent = cur;
							cur->m_children.PushBack(prev);
							prevIdx = i;
							prev = nullptr;
						}
						else if (prev->m_bounds.Encapsulates(cur->m_bounds))
						{
							cur->m_parent = prev;
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
				else if (cluster->Count() >= ChildSoftLimit && passCount <= 1)
				{
					// Cluster has many nodes and we're deep in the tree. Add nodes as a child of the last node to reduce complexity.
					BuildNode* last = clusterNodes.PushBack() = cluster->PopBack();

					for (auto& child : *cluster)
					{
						child->m_parent = last;
						last->m_bounds.Encapsulate(child->m_bounds);
						last->m_children.PushBack(child);
					}

					largestClusterVolume = glm::max<float>(largestClusterVolume, last->m_bounds.Volume());
				}
				else
				{
					// Cluster has few nodes, or is otherwise higher up in the tree. Create a new node and add all cluster nodes as children.
					BuildNode* newNode = clusterNodes.PushBack() = AllocateBuildNode();
					newNode->m_bounds = cluster->Front()->m_bounds;

					for (auto& child : *cluster)
					{
						child->m_parent = newNode;
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
		}

		BuildNode* root = nullptr;

		if ((clusterNodes.Count() > 1 || passCount < waves.Count() - 1) && passCount < 500)
		{
			m_toleranceDistance[0] += m_toleranceStep[0];
			m_toleranceDistance[1] += m_toleranceStep[1];
			m_toleranceDistance[2] += m_toleranceStep[2];

			root = RecursiveBuildBottomUp(clusterNodes, waves, waveIdx, passCount + 1);
		}
		else if (clusterNodes.Count() > 1)
		{
			root = AllocateBuildNode();

			for (auto& child : clusterNodes)
			{
				child->m_parent = root;
				root->m_bounds.Encapsulate(child->m_bounds);
				root->m_children.PushBack(child);
			}
		}
		else
		{
			root = clusterNodes[0];
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

		if ((node->m_depth == depth && !node->IsLeaf()) || node->m_depth == (depth + 1) || depth == ~uint32_t(0))
		{
			DebugDrawCube(lines, origin, extents, lineColor);
		}

		if (node->m_depth <= depth)
		{
			for (BuildNode* n : node->m_children)
				RecursiveDebugDrawNode(lines, frustrum, n, depth, drawObjectBounds);
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
		BuildNode* newNode = m_allocator->Alloc<BuildNode>();
		newNode->m_data = AllocateNodeData();
		return newNode;
	}

	void BoundingVolumeHierarchy::FreeBuildNode(BuildNode* node)
	{
		if (node->m_data)
		{
			FreeNodeData(node->m_data);
		}
		m_allocator->Free(node);
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