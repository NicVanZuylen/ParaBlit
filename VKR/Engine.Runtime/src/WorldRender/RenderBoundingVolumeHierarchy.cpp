#include "BatchDispatcher.h"
#include "Camera.h"
#include "RenderBoundingVolumeHierarchy.h"
#include "RenderGraphPasses/DebugLinePass.h"
#include "Resource/Mesh.h"
#include "Resource/Material.h"

#include "glm/gtc/type_ptr.hpp"
#include <algorithm>
#include <iostream>
#include <map>

namespace Eng
{

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
	}

	RenderBoundingVolumeHierarchy::~RenderBoundingVolumeHierarchy()
	{
		RecursiveFreeNode(m_sourceRoot);
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

		m_sourceRoot = BuildBottomUpInternal(nodes);

		BatchMap batchMap;
		std::set<PB::Pipeline> excludedPipelines;
		BuildDrawBatchesExclusive(m_sourceRoot, batchMap, excludedPipelines);

		BuildNode* pipelineSubtree = BuildBottomUpInternal(m_globalBatchHierarchy.m_buildNodes);
		m_globalBatchHierarchy.m_buildNodes.Clear();

		RecursiveBakeHierarchy(m_globalBatchHierarchy.m_bakedNodes, pipelineSubtree, ~size_t(0));
		RecursiveFreeNode(pipelineSubtree, true);

		m_globalBatchHierarchy.m_bakedNodeUpdateIndices.PushBack(0);
	}

	RenderBoundingVolumeHierarchy::BuildNode* RenderBoundingVolumeHierarchy::BuildSubTree(CLib::Vector<ObjectData>& objects)
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
			node.m_parent = nullptr;
			node.m_depth = 0;
			node.m_isObject = true;
		}

		BuildNode* subTree = BuildBottomUpInternal(nodes);
		return subTree;
	}

	void RenderBoundingVolumeHierarchy::RebuildTest()
	{
		BuildNode* subtree = m_sourceRoot->m_children[1];

		RebuildSubTree(subtree);
	}

	void RenderBoundingVolumeHierarchy::RebuildSubTree(BuildNode* subtreeRoot)
	{
		CLib::Vector<ObjectData> objects;
		RecursiveGetObjectData(objects, subtreeRoot);

		BuildNode* newSubtree = BuildSubTree(objects);
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

		// With the new subtree linked, we can now build or rebuild any necessary draw batches.
		RebuildDrawBatchesForSubtree(newSubtree, subtreeRoot, objects);

		// Free old subtree.
		RecursiveFreeNode(subtreeRoot, true);
	}

	void RenderBoundingVolumeHierarchy::BakeBatches(PB::ICommandContext* commandContext)
	{
		for (const size_t& updateIndex : m_globalBatchHierarchy.m_bakedNodeUpdateIndices)
		{
			RecursiveBakeBatches(commandContext, m_globalBatchHierarchy.m_bakedNodes[updateIndex]);
		}
		m_globalBatchHierarchy.m_bakedNodeUpdateIndices.Clear();
	}

	void RenderBoundingVolumeHierarchy::DebugDraw(const Camera* camera, DebugLinePass* lines, uint32_t depth, bool drawObjectBounds)
	{
		Camera::CameraFrustrum frustrum;
		if (camera != nullptr)
		{
			frustrum = camera->GetFrustrum();
			// Draw frustrum
			glm::vec3 frustrumColor(1.0f, 0.0f, 1.0f);
			Camera::DrawFrustrum(lines, frustrum, frustrumColor);
		}

		RecursiveDebugDrawNode(lines, frustrum, m_sourceRoot, depth, drawObjectBounds);
	}

	void RenderBoundingVolumeHierarchy::DebugDrawBatchTree(const Camera* camera, DebugLinePass* lines)
	{
		Camera::CameraFrustrum frustrum;
		if (camera != nullptr)
		{
			frustrum = camera->GetFrustrum();
			glm::vec3 frustrumColor(1.0f, 0.0f, 1.0f);
			Camera::DrawFrustrum(lines, frustrum, frustrumColor);
		}

		RecursiveDebugDrawBakedTree(lines, frustrum, m_globalBatchHierarchy.m_bakedNodes.front());
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

	void RenderBoundingVolumeHierarchy::AxisSplitClusters(ClusterArray& clusters, EProjectedAxis axis)
	{
		auto rangeCompare = [=](const BuildNode* a, const BuildNode* b) -> bool
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

		BuildNode* root = RecursiveBuildBottomUp(*deferredNodes[0].first, deferredNodes, 1, 0);
		for (NodeWave& wave : deferredNodes)
		{
			m_allocator->Free(wave.first);
		}

		AssignDepth(root, 0);
		return root;
	}

	RenderBoundingVolumeHierarchy::BuildNode* RenderBoundingVolumeHierarchy::RecursiveBuildBottomUp(const CLib::Vector<BuildNode*>& nodes, CLib::Vector<NodeWave>& waves, uint32_t waveIdx, uint32_t passCount)
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

		CLib::Vector<BuildNode*> clusterNodes{};
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
			else if (cluster->Count() > 3 && passCount <= 1)
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
				BuildNode* newNode = clusterNodes.PushBack() = m_allocator->Alloc<BuildNode>();
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

	void RenderBoundingVolumeHierarchy::AssignDepth(BuildNode* node, uint32_t depth)
	{
		node->m_depth = depth;

		for (auto& n : node->m_children)
			AssignDepth(n, depth + 1);
	}

	void RenderBoundingVolumeHierarchy::BuildDrawBatch(PB::Pipeline pipeline, BuildNode* node, BatchObjects& objects)
	{
		PipelineDrawbatch& pipelineBatch = node->m_drawBatches.PushBack();
		pipelineBatch.m_pipeline = pipeline;
		pipelineBatch.m_bakedNodeIndex = ~size_t(0);

		DrawBatch::CreateDesc drawBatchDesc;
		drawBatchDesc.m_renderer = m_renderer;
		drawBatchDesc.m_allocator = m_allocator;
		pipelineBatch.m_batch = m_allocator->Alloc<DrawBatch>(drawBatchDesc);

		for (auto& node : objects)
		{
			ObjectData& obj = node->m_objectData;
			PB::BindingLayout materialBindings = obj.m_material->GetBindings();

			pipelineBatch.m_batch->AddInstance(obj.m_mesh, glm::value_ptr(obj.m_transform), node->m_bounds, materialBindings.m_resourceViews, materialBindings.m_resourceCount, obj.m_material->GetSampler());
		}
		pipelineBatch.m_batch->UpdateCullParams();
	}

	void RenderBoundingVolumeHierarchy::RecursiveSearchDownForDrawbatches(BuildNode* node, size_t& highestBakedNodeIndex)
	{
		for (auto& pipelineBatch : node->m_drawBatches)
		{
			highestBakedNodeIndex = std::min<size_t>(highestBakedNodeIndex, pipelineBatch.m_bakedNodeIndex);

			// This batch will be replaced.
			m_allocator->Free(pipelineBatch.m_batch);

			BakedNode& baked = m_globalBatchHierarchy.m_bakedNodes[pipelineBatch.m_bakedNodeIndex];
			baked.m_batch = nullptr;
		}
		node->m_drawBatches.Clear();

		for (auto& child : node->m_children)
		{
			RecursiveSearchDownForDrawbatches(child, highestBakedNodeIndex);
		}
	}

	void RenderBoundingVolumeHierarchy::RecursiveSearchUpForDrawbatches(BuildNode* node, BuildNode*& highestFound, std::set<PB::Pipeline>& pipelinesToFind, size_t& highestBakedNodeIndex)
	{
		if(node->m_drawBatches.Count() > 0)
		{
			NodeDrawBatches newBatches{};
			for (auto& pipelineBatch : node->m_drawBatches)
			{
				auto it = pipelinesToFind.find(pipelineBatch.m_pipeline);
				if(it != pipelinesToFind.end())
				{
					pipelinesToFind.erase(it);
					highestBakedNodeIndex = std::min<size_t>(highestBakedNodeIndex, pipelineBatch.m_bakedNodeIndex);
					highestFound = node;

					// This batch will be replaced.
					m_allocator->Free(pipelineBatch.m_batch);

					BakedNode& baked = m_globalBatchHierarchy.m_bakedNodes[pipelineBatch.m_bakedNodeIndex];
					baked.m_batch = nullptr;
				}
				else
				{
					newBatches.PushBack(pipelineBatch);
				}
			}
			node->m_drawBatches = newBatches;
		}

		if (node->m_parent != nullptr && pipelinesToFind.empty() == false)
		{
			RecursiveSearchUpForDrawbatches(node->m_parent, highestFound, pipelinesToFind, highestBakedNodeIndex);
		}
	}

	void RenderBoundingVolumeHierarchy::RebuildDrawBatchesForSubtree(BuildNode* subtree, BuildNode* oldSubtree, CLib::Vector<ObjectData>& objectData)
	{
		std::set<PB::Pipeline> pipelinesToFind;
		for (auto& obj : objectData)
		{
			PB::Pipeline pipeline = obj.m_material->GetPipeline();
			if (pipelinesToFind.contains(pipeline) == false)
				pipelinesToFind.insert(pipeline);
		}
		std::set<PB::Pipeline> pipelinesToRebuild = pipelinesToFind;

		size_t highestBakedNodeIndex = ~size_t(0);
		RecursiveSearchDownForDrawbatches(oldSubtree, highestBakedNodeIndex);

		BuildNode* highestBatchNode = subtree;
		RecursiveSearchUpForDrawbatches(subtree, highestBatchNode, pipelinesToFind, highestBakedNodeIndex);

		BakedNodes& finalBakedNodes = m_globalBatchHierarchy.m_bakedNodes;
		BakedNode updateRoot = finalBakedNodes[highestBakedNodeIndex];
		if (updateRoot.m_parentIndex != ~size_t(0))
		{
			highestBakedNodeIndex = updateRoot.m_parentIndex;
			updateRoot = finalBakedNodes[updateRoot.m_parentIndex];
		}

		// Now we've found the highest node covering all draw batches which need rebuilding. Build draw batches from here and create build nodes for them.
		BatchMap batchMap;
		RebuildDrawBatchesInclusive(highestBatchNode, batchMap, pipelinesToRebuild);

		// Create build nodes from existing baked node's drawbatches below highestBakedNodeIndex.
		GetBakedBatchBuildNodes(m_globalBatchHierarchy.m_bakedNodes[highestBakedNodeIndex]);

		BuildNode* batchSubtree = BuildBottomUpInternal(m_globalBatchHierarchy.m_buildNodes);
		BakedNodes newBakedNodes{};
		RecursiveBakeHierarchy(newBakedNodes, batchSubtree, ~size_t(0), highestBakedNodeIndex);
		m_globalBatchHierarchy.m_buildNodes.Clear();

		// Clear build resources.
		RecursiveFreeNode(batchSubtree, true);

		int64_t parentStrideDiff = int64_t(newBakedNodes.size()) - updateRoot.m_strideToNext;
		finalBakedNodes.erase
		(
			finalBakedNodes.begin() + highestBakedNodeIndex,
			finalBakedNodes.begin() + highestBakedNodeIndex + updateRoot.m_strideToNext
		);

		finalBakedNodes.insert(finalBakedNodes.begin() + highestBakedNodeIndex, newBakedNodes.begin(), newBakedNodes.end());

		// Update strides of all parent nodes up the tree.
		if (parentStrideDiff != 0)
		{
			size_t parentIndex = highestBakedNodeIndex;
			while (parentIndex != ~size_t(0))
			{
				BakedNode& parent = finalBakedNodes[parentIndex];
				parent.m_strideToNext += parentStrideDiff;

				parentIndex = parent.m_parentIndex;
			}
		}

		printf_s("\nBaked Node Count: %u\n", uint32_t(finalBakedNodes.size()));

		m_globalBatchHierarchy.m_bakedNodeUpdateIndices.PushBack(highestBakedNodeIndex);
	}

	void RenderBoundingVolumeHierarchy::BuildDrawBatchesExclusive(BuildNode* root, BatchMap& batchMap, std::set<PB::Pipeline>& excludedPipelines)
	{
		// Draw batches are built covering as many branches of the BVH as possible.
		// If a draw batch would contain too many objects it will be created further down the tree.

		batchMap.clear();
		GetBatchCandidates(batchMap, root);

		// excludedPipelines contains pipelines for which a draw batch has already been created covering these nodes.
		std::set<PB::Pipeline> newlyExcludedPipelines = excludedPipelines;
		bool allPipelinesDone = true;
		for (auto pair : batchMap)
		{
			if (excludedPipelines.find(pair.first) != excludedPipelines.end())
				continue;

			if (pair.second.Count() <= PipelineDrawbatch::REMOVE_MaxObjects)
			{
				BuildDrawBatch(pair.first, root, pair.second);
				newlyExcludedPipelines.insert(pair.first);

				PipelineDrawbatch& pipelineBatch = root->m_drawBatches.Back();
				BuildNode* newBuildNode = m_globalBatchHierarchy.m_buildNodes.PushBack(m_allocator->Alloc<BuildNode>());
				newBuildNode->m_sourceNode = root;
				newBuildNode->m_bounds = pipelineBatch.m_batch->GetBounds();
				newBuildNode->m_drawBatches.PushBack(pipelineBatch);
				newBuildNode->m_isObject = true;
			}
			else
			{
				allPipelinesDone = false;
			}
		}

		if (allPipelinesDone == false)
		{
			for (auto* child : root->m_children)
			{
				BuildDrawBatchesExclusive(child, batchMap, newlyExcludedPipelines);
			}
		}
	}

	void RenderBoundingVolumeHierarchy::RebuildDrawBatchesInclusive(BuildNode* root, BatchMap& batchMap, std::set<PB::Pipeline> includedPipelines)
	{
		batchMap.clear();
		GetBatchCandidates(batchMap, root);

		for (auto pair : batchMap)
		{
			auto pipelineIt = includedPipelines.find(pair.first);
			if (pipelineIt == includedPipelines.end())
				continue;

			if (pair.second.Count() <= PipelineDrawbatch::REMOVE_MaxObjects)
			{
				BuildDrawBatch(pair.first, root, pair.second);
				includedPipelines.erase(pipelineIt);

				auto& pipelineBatch = root->m_drawBatches.Back();

				// Create build node for the new batch.
				BuildNode* newBuildNode = m_globalBatchHierarchy.m_buildNodes.PushBack(m_allocator->Alloc<BuildNode>());
				newBuildNode->m_sourceNode = root;
				newBuildNode->m_bounds = pipelineBatch.m_batch->GetBounds();
				newBuildNode->m_drawBatches.PushBack(pipelineBatch);
				newBuildNode->m_isObject = true;
			}
		}

		if (includedPipelines.empty() == false)
		{
			for (auto* child : root->m_children)
			{
				RebuildDrawBatchesInclusive(child, batchMap, includedPipelines);
			}
		}
	}

	void RenderBoundingVolumeHierarchy::GetBakedBatchBuildNodes(const BakedNode& node)
	{
		if (node.m_batch != nullptr)
		{
			PipelineDrawbatch batch;
			batch.m_bakedNodeIndex = ~size_t(0);
			batch.m_batch = node.m_batch;
			batch.m_pipeline = 0;

			BuildNode* newBuildNode = m_globalBatchHierarchy.m_buildNodes.PushBack(m_allocator->Alloc<BuildNode>());
			newBuildNode->m_sourceNode = nullptr;
			newBuildNode->m_bounds = node.m_batch->GetBounds();
			newBuildNode->m_drawBatches.PushBack(batch);
			newBuildNode->m_isObject = true;
		}

		const uint32_t& childCount = node.m_childCount;
		const BakedNode* child = reinterpret_cast<const BakedNode*>(&node + 1);
		for (uint32_t i = 0; i < childCount; ++i)
		{
			GetBakedBatchBuildNodes(*child);
			child = child->NextSibling();
		}
	}

	void RenderBoundingVolumeHierarchy::GetBatchCandidates(BatchMap& batchMap, BuildNode* node)
	{
		// Nodes are sorted by proximity to the parent node's centre.
		// This is done in an attempt to place nodes which are close together in world space closer to each other in draw batch memory to avoid memory fragmentation that would introduce more draw calls.
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

	void RenderBoundingVolumeHierarchy::RecursiveGetObjectData(CLib::Vector<ObjectData>& outObjectData, BuildNode* node)
	{
		if (node->m_isObject)
		{
			outObjectData.PushBack(node->m_objectData);
		}

		for (auto& child : node->m_children)
			RecursiveGetObjectData(outObjectData, child);
	}

	void RenderBoundingVolumeHierarchy::RecursiveFreeNode(BuildNode* node, bool keepBatches)
	{
		for (BuildNode* child : node->m_children)
		{
			RecursiveFreeNode(child, keepBatches);
		}

		if (keepBatches == false)
		{
			for (auto& pipelineBatch : node->m_drawBatches)
			{
				m_allocator->Free(pipelineBatch.m_batch);
			}
		}

		m_allocator->Free(node);
	}

	void RenderBoundingVolumeHierarchy::BakeHierarchies(BuildNode* start)
	{
		CLib::Vector<BuildNode> batchNodes;
	}

	void RenderBoundingVolumeHierarchy::RecursiveBakeHierarchy(BakedNodes& bakedNodes, BuildNode* node, size_t parentIndex, size_t baseIndex)
	{
		assert(node->m_drawBatches.Count() <= 1);

		size_t index = bakedNodes.size();

		{
			PipelineDrawbatch* nodeBatch = node->m_drawBatches.Count() > 0 ? &node->m_drawBatches.Front() : nullptr;

			BakedNode& baked = bakedNodes.emplace_back();
			baked.m_bounds = node->m_bounds;
			baked.m_childCount = node->m_children.Count();
			baked.m_parentIndex = parentIndex;
			baked.m_batch = nodeBatch ? nodeBatch->m_batch : nullptr;
			baked.m_batchPipeline = nodeBatch ? nodeBatch->m_pipeline : 0;

			if (node->m_sourceNode != nullptr)
			{
				NodeDrawBatches& sourceBatches = node->m_sourceNode->m_drawBatches;
				for (auto& pipelineBatch : sourceBatches)
				{
					if (pipelineBatch.m_batch == baked.m_batch)
					{
						pipelineBatch.m_bakedNodeIndex = baseIndex + index;
						break;
					}
				}
			}
		}

		for (BuildNode* n : node->m_children)
		{
			RecursiveBakeHierarchy(bakedNodes, n, index, baseIndex);
		}
		bakedNodes[index].m_strideToNext = uint32_t(bakedNodes.size() - index);
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
			&& IsInFrontOfPlane(frustrum.m_bottom, bounds)
			&& IsInFrontOfPlane(frustrum.m_far, bounds);
	}

	void RenderBoundingVolumeHierarchy::RecursiveBakeBatches(PB::ICommandContext* cmdContext, BakedNode& node)
	{
		if (node.m_batch != nullptr)
		{
			node.m_batch->UpdateIndices(cmdContext);
		}

		const uint32_t& childCount = node.m_childCount;
		BakedNode* child = reinterpret_cast<BakedNode*>(&node + 1);
		for (uint32_t i = 0; i < childCount; ++i)
		{
			RecursiveBakeBatches(cmdContext, *child);
			child = child->NextSibling();
		}
	}

	void RenderBoundingVolumeHierarchy::RecursiveCullBatches(BatchDispatcher* dispatcher, const PB::BindingLayout& globalBindings, const Camera::CameraFrustrum& frustrum, const BakedNode& node) const
	{
		if (FrustrumTest(frustrum, node.m_bounds))
		{
			if (node.m_batch != nullptr)
				dispatcher->AddBatch(node.m_batch, node.m_batchPipeline, globalBindings);

			const uint32_t& childCount = node.m_childCount;
			const BakedNode* child = reinterpret_cast<const BakedNode*>(&node + 1);
			for (uint32_t i = 0; i < childCount; ++i)
			{
				RecursiveCullBatches(dispatcher, globalBindings, frustrum, *child);
				child = child->NextSibling();
			}
		}
	}

	void RenderBoundingVolumeHierarchy::RecursiveDebugDrawBakedTree(DebugLinePass* lines, const Camera::CameraFrustrum& frustrum, const BakedNode& node)
	{
		if (FrustrumTest(frustrum, node.m_bounds))
		{
			glm::vec3 color = node.m_batch != nullptr ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
			DebugDrawCube(lines, node.m_bounds.m_origin, node.m_bounds.m_extents, color);

			const uint32_t& childCount = node.m_childCount;
			const BakedNode* child = reinterpret_cast<const BakedNode*>(&node + 1);
			for (uint32_t i = 0; i < childCount; ++i)
			{
				RecursiveDebugDrawBakedTree(lines, frustrum, *child);
				child = child->NextSibling();
			}
		}
	}

	void RenderBoundingVolumeHierarchy::CullBatches(const Camera::CameraFrustrum& cameraFrustrum, BatchDispatcher* dispatcher, const PB::BindingLayout& globalBindings) const
	{
		RecursiveCullBatches(dispatcher, globalBindings, cameraFrustrum, m_globalBatchHierarchy.m_bakedNodes.front());
	}

	void RenderBoundingVolumeHierarchy::RecursiveDebugDrawNode(DebugLinePass* lines, const Camera::CameraFrustrum& frustrum, BuildNode* node, uint32_t depth, bool drawObjectBounds)
	{
		glm::vec3 lineColor = node->m_isObject ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
		lineColor = (depth != ~uint32_t(0) && node->m_depth == depth) ? glm::vec3(0.0f, 0.0f, 1.0f) : lineColor;

		const glm::vec3& origin = node->m_bounds.m_origin;
		const glm::vec3& extents = node->m_bounds.m_extents;

		// Origin to sky
		if (node == m_sourceRoot)
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

};