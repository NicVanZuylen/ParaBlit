#include "RenderBoundingVolumeHierarchy.h"
#include "DebugLinePass.h"
#include "Camera.h"
#include "Mesh.h"
#include "Material.h"
#include "BatchDispatcher.h"

#include "glm/gtc/type_ptr.hpp"
#include <algorithm>
#include <iostream>
#include <map>

RenderBoundingVolumeHierarchy::RenderBoundingVolumeHierarchy(PB::IRenderer* renderer, CLib::Allocator* allocator, const CreateDesc& desc)
{
	m_renderer = renderer;
	m_allocator = allocator;

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

	for (auto& hierarchy : m_pipelineHierarchies)
	{
		for (auto& bakedNode : hierarchy.second.m_bakedHierarchy)
		{
			if (bakedNode.m_batch)
			{
				m_allocator->Free(bakedNode.m_batch);
			}
		}

		RecursiveFreeNode(hierarchy.second.m_root);
	}
}

void RenderBoundingVolumeHierarchy::BuildBottomUp(CLib::Vector<ObjectData>& objects)
{
	CLib::Vector<BuildNode*> nodes;
	nodes.Reserve(objects.Count());
	m_totalNodeCount = objects.Count();

	for (ObjectData& obj : objects)
	{
		BuildNode* n = m_allocator->Alloc<BuildNode>();
		nodes.PushBack(n);
		BuildNode& node = *n;

		node.m_bounds = obj.m_mesh->GetBounds();
		node.m_bounds.Transform(obj.m_transform);
		node.m_objectData = obj;
		node.m_depth = 0;
		node.m_isObject = true;
	}

	m_root = BuildBottomUpInternal(nodes);

	CLib::Vector<PB::Pipeline> excludedPipelines;
	BuildDrawBatches(m_root, excludedPipelines);
}

void RenderBoundingVolumeHierarchy::DebugDraw(DebugLinePass* lines, uint32_t depth, bool drawObjectBounds)
{
	Camera::CameraFrustrum frustrum;
	if (m_camera != nullptr)
	{
		frustrum = m_camera->GetFrustrum();

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

void RenderBoundingVolumeHierarchy::BakeHierarchies(PB::ICommandContext* commandContext)
{
	for (auto& pair : m_pipelineHierarchies)
	{
		CLib::Vector<BuildNode*> buildNodes(pair.second.m_batches.Count());
		for (DrawBatch* batch : pair.second.m_batches)
		{
			BuildNode* node = buildNodes.PushBack() = m_allocator->Alloc<BuildNode>();
			node->m_bounds = batch->GetBounds();
			node->m_drawBatch = batch;
			node->m_isObject = true;
		}

		pair.second.m_root = BuildBottomUpInternal(buildNodes);
		RecursiveBakeHierarchy(pair.second.m_bakedHierarchy, pair.second.m_root);

		if (pair.second.m_batchesInitialized == false)
		{
			for (DrawBatch*& batch : pair.second.m_batches)
			{
				batch->UpdateIndices(commandContext);
			}
			pair.second.m_batchesInitialized = true;
		}
	}
}

void RenderBoundingVolumeHierarchy::CullBatches(const Camera* camera, BatchDispatcher* dispatcher, const PB::BindingLayout& globalBindings) const
{
	const Camera::CameraFrustrum& cameraFrustrum = camera->GetFrustrum();
	for(const auto& pair : m_pipelineHierarchies)
	{
		const PipelineHierarchy& hierarchy = pair.second;
		RecursiveCullBatches(dispatcher, pair.first, globalBindings, cameraFrustrum, hierarchy.m_bakedHierarchy.Front());
	}
}

float RenderBoundingVolumeHierarchy::GetClusterScore(const BuildNode* a, std::vector<BuildNode*>& pool)
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

void RenderBoundingVolumeHierarchy::GenClusters(CLib::Vector<BuildNode*>& nodes, CLib::Vector<Cluster*>& outClusters, EProjectedAxis axis)
{
	auto rangeCompare = [=](const BuildNode* a, const BuildNode* b) -> bool
	{
		return a->GetRange(axis).m_min < b->GetRange(axis).m_min;
	};
	std::sort(nodes.begin(), nodes.end(), rangeCompare);
}

void RenderBoundingVolumeHierarchy::AxisSplitClusters(CLib::Vector<Cluster*>& clusters, EProjectedAxis axis)
{
	auto rangeCompare = [=](const BuildNode* a, const BuildNode* b) -> bool
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

RenderBoundingVolumeHierarchy::BuildNode* RenderBoundingVolumeHierarchy::BuildBottomUpInternal(CLib::Vector<BuildNode*>& nodes)
{
	auto compare = [=](const BuildNode* a, const BuildNode* b)
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

			currentGroup = newWave.first = m_allocator->Alloc<CLib::Vector<BuildNode*>>();
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
	BuildNode* root = RecursiveBuildBottomUp(nodes, deferredNodes, 1, 0);

	for (NodeWave& wave : deferredNodes)
	{
		m_allocator->Free(wave.first);
	}

	AssignDepth(root, 0);
	return root;
}

RenderBoundingVolumeHierarchy::BuildNode* RenderBoundingVolumeHierarchy::RecursiveBuildBottomUp(CLib::Vector<BuildNode*>& nodes, CLib::Vector<NodeWave>& waves, uint32_t waveIdx, uint32_t passCount)
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

	CLib::Vector<BuildNode*> clusterNodes;
	clusterNodes.Reserve(clusters.Count());

	auto volumeCompare = [=](const BuildNode* a, const BuildNode* b)
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
				BuildNode*& cur = c[i];
				BuildNode*& prev = c[prevIdx];
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
			BuildNode* last = clusterNodes.PushBack() = cluster->PopBack();

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
			BuildNode* newNode = clusterNodes.PushBack() = m_allocator->Alloc<BuildNode>();
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
		root = m_allocator->Alloc<BuildNode>();

		for (auto& child : clusterNodes)
		{
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

void RenderBoundingVolumeHierarchy::AssignDepth(BuildNode* node, uint32_t depth)
{
	node->m_depth = depth;

	for (auto& n : node->m_children)
		AssignDepth(n, depth + 1);
}

void RenderBoundingVolumeHierarchy::BuildDrawBatch(PB::Pipeline pipeline, BatchObjects& objects)
{
	DrawBatch::CreateDesc drawBatchDesc;
	drawBatchDesc.m_renderer = m_renderer;
	drawBatchDesc.m_allocator = m_allocator;
	DrawBatch* batch = m_allocator->Alloc<DrawBatch>(drawBatchDesc);

	for (auto& node : objects)
	{
		ObjectData& obj = node->m_objectData;
		PB::BindingLayout materialBindings = obj.m_material->GetBindings();

		batch->AddInstance(obj.m_mesh, glm::value_ptr(obj.m_transform), node->m_bounds, materialBindings.m_resourceViews, materialBindings.m_resourceCount, obj.m_material->GetSampler());
	}
	batch->UpdateCullParams();

	m_pipelineHierarchies.try_emplace(pipeline);
	auto it = m_pipelineHierarchies.find(pipeline);
	assert(it != m_pipelineHierarchies.end());

	it->second.m_batches.PushBack(batch);
}

void RenderBoundingVolumeHierarchy::BuildDrawBatches(BuildNode* root, CLib::Vector<PB::Pipeline>& excludedPipelines)
{
	assert(root->m_isObject == false);

	excludedPipelines.Clear();
	BatchMap batchMap;
	for (auto* child : root->m_children)
	{
		GetBatchCandidates(batchMap, child);
	}

	for (auto pair : batchMap)
	{
		if (pair.second.Count() <= DrawBatch::MaxObjects)
		{
			bool excluded = false;
			for (auto& pipeline : excludedPipelines)
			{
				if (pipeline == pair.first)
				{
					excluded = true;
					break;
				}
			}

			if (excluded == false)
			{
				excludedPipelines.PushBack(pair.first);
				BuildDrawBatch(pair.first, pair.second);
			}
		}
		else
		{
			for (auto* child : root->m_children)
			{
				BuildDrawBatches(child, excludedPipelines);
			}
		}
	}
}

void RenderBoundingVolumeHierarchy::GetBatchCandidates(BatchMap& batchMap, BuildNode* node)
{
	glm::vec3 centre = node->m_bounds.Centre();
	auto centreProximitySort = [&](const BuildNode* a, const BuildNode* b)
	{
		float distA = glm::distance(centre, a->m_bounds.Centre());
		float distB = glm::distance(centre, b->m_bounds.Centre());
		return distA < distB;
	};
	std::sort(node->m_children.begin(), node->m_children.end(), centreProximitySort);

	if (node->m_isObject)
	{
		PB::Pipeline pipeline = node->m_objectData.m_material->GetPipeline();
		batchMap.try_emplace(pipeline);
		auto it = batchMap.find(pipeline);
		assert(it != batchMap.end());

		it->second.PushBack(node);
	}

	for (auto* child : node->m_children)
	{		
		GetBatchCandidates(batchMap, child);
	}
}

void RenderBoundingVolumeHierarchy::RecursiveBakeHierarchy(CLib::Vector<BakedNode>& bakedNodes, BuildNode* node)
{
	BakedNode& baked = bakedNodes.PushBack();
	baked.m_bounds = node->m_bounds;
	baked.m_index = bakedNodes.Count() - 1;
	baked.m_nextIndex = bakedNodes.Count() + node->m_children.Count();
	baked.m_batch = node->m_drawBatch;

	for (BuildNode* n : node->m_children)
	{
		RecursiveBakeHierarchy(bakedNodes, n);
	}
}

void RenderBoundingVolumeHierarchy::RecursiveFreeNode(BuildNode* node)
{
	for (BuildNode* n : node->m_children)
		RecursiveFreeNode(n);

	m_allocator->Free(node);
}

bool RenderBoundingVolumeHierarchy::IsInFrontOfPlane(const Camera::CameraFrustrum::Plane& plane, const Bounds& bounds) const
{
	const glm::vec3 centre = bounds.Centre();
	const glm::vec3 halfExtents = bounds.m_extents * 0.5f;

	const float r = halfExtents.x * std::abs(plane.x) +
		halfExtents.y * std::abs(plane.y) + halfExtents.z * std::abs(plane.z);

	const float signedDist = glm::dot(glm::vec3(plane), centre) - plane.w;

	return -r <= signedDist;
}

bool RenderBoundingVolumeHierarchy::FrustrumTest(const Camera::CameraFrustrum& frustrum, const Bounds& bounds) const
{
	return IsInFrontOfPlane(frustrum.m_near, bounds)
		&& IsInFrontOfPlane(frustrum.m_left, bounds)
		&& IsInFrontOfPlane(frustrum.m_right, bounds)
		&& IsInFrontOfPlane(frustrum.m_top, bounds)
		&& IsInFrontOfPlane(frustrum.m_bottom, bounds);
}

void RenderBoundingVolumeHierarchy::RecursiveCullBatches(BatchDispatcher* dispatcher, PB::Pipeline pipeline, const PB::BindingLayout& globalBindings, const Camera::CameraFrustrum& frustrum, const BakedNode& node) const
{
	if (FrustrumTest(frustrum, node.m_bounds))
	{
		if (node.m_batch != nullptr)
			dispatcher->AddBatch(node.m_batch, pipeline, globalBindings);

		uint32_t childCount = node.ChildCount();
		for (uint32_t i = 0; i < childCount; ++i)
		{
			const BakedNode& child = node.GetChild(i);
			RecursiveCullBatches(dispatcher, pipeline, globalBindings, frustrum, child);
		}
	}
}

void RenderBoundingVolumeHierarchy::RecursiveDebugDrawNode(DebugLinePass* lines, const Camera::CameraFrustrum& frustrum, BuildNode* node, uint32_t depth, bool drawObjectBounds)
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
	}

	if (node->m_depth <= depth)
	{
		for (BuildNode* n : node->m_children)
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

void RenderBoundingVolumeHierarchy::BuildNode::RemoveChild(BuildNode* child)
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
