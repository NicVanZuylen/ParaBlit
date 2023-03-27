#include "RenderBoundingVolumeHierarchy.h"
#include "WorldRender/BatchDispatcher.h"
#include "RenderGraphPasses/DebugLinePass.h"
#include "Resource/Mesh.h"
#include "Resource/Material.h"

#include "glm/gtc/type_ptr.hpp"
#include <algorithm>

namespace Eng
{

	RenderBoundingVolumeHierarchy::RenderBoundingVolumeHierarchy(PB::IRenderer* renderer, CLib::Allocator* allocator, AssetStreamer* streamer, const CreateDesc& desc)
		: BoundingVolumeHierarchy(allocator, desc)
	{
		m_renderer = renderer;
		m_streamer = streamer;
	}

	RenderBoundingVolumeHierarchy::~RenderBoundingVolumeHierarchy()
	{

	}

	void RenderBoundingVolumeHierarchy::Init(PB::IRenderer* renderer, CLib::Allocator* allocator, AssetStreamer* streamer, const CreateDesc& desc)
	{
		BoundingVolumeHierarchy::Init(allocator, desc);

		m_renderer = renderer;
		m_streamer = streamer;
	}

	void RenderBoundingVolumeHierarchy::Destroy()
	{
		if (m_root != nullptr)
		{
			RecursiveFreeNode(m_root);
			m_root = nullptr;
		}
	}

	BoundingVolumeHierarchy::BuildNode* RenderBoundingVolumeHierarchy::BuildBottomUp(InputObjects& objects)
	{
		BuildNode* buildResult = BoundingVolumeHierarchy::BuildBottomUp(objects);

		BatchMap batchMap;
		std::set<PB::Pipeline> excludedPipelines;
		BuildDrawBatchesExclusive(buildResult, batchMap, excludedPipelines);

		BuildNode* pipelineSubtree = BuildBottomUpInternal(m_globalBatchHierarchy.m_buildNodes);
		m_globalBatchHierarchy.m_buildNodes.Clear();

		RecursiveBakeHierarchy(m_globalBatchHierarchy.m_bakedNodes, pipelineSubtree, ~size_t(0));
		RecursiveFreeNode(pipelineSubtree, true);

		m_globalBatchHierarchy.m_bakedNodeUpdateIndices.PushBack(0);
		return buildResult;
	}

	void RenderBoundingVolumeHierarchy::RebuildTest()
	{
		BuildNode* subtree = m_root->m_children[1];

		RebuildSubTree(subtree);
	}

	void RenderBoundingVolumeHierarchy::RebuildSubTree(BuildNode* subtreeRoot)
	{
		InputObjects objects;
		BoundingVolumeHierarchy::RecursiveGetObjectData(objects, subtreeRoot);

		BuildNode* newSubtree = BoundingVolumeHierarchy::BuildBottomUp(objects);
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

	void RenderBoundingVolumeHierarchy::BuildDrawBatch(PB::Pipeline pipeline, BuildNode* node, BatchObjects& objects)
	{
		NodeData* nodeData = reinterpret_cast<NodeData*>(node->m_data);

		PipelineDrawbatch& pipelineBatch = nodeData->m_drawBatches.PushBack();
		pipelineBatch.m_pipeline = pipeline;
		pipelineBatch.m_bakedNodeIndex = ~size_t(0);

		DrawBatch::CreateDesc drawBatchDesc;
		drawBatchDesc.m_renderer = m_renderer;
		drawBatchDesc.m_allocator = m_allocator;
		drawBatchDesc.m_streamer = m_streamer;
		pipelineBatch.m_batch = m_allocator->Alloc<DrawBatch>(drawBatchDesc);

		for (auto& node : objects)
		{
			const ObjectData* obj = reinterpret_cast<const ObjectData*>(node->m_objectData);

			pipelineBatch.m_batch->AddInstance(obj->m_meshID, glm::value_ptr(obj->m_transform), node->m_bounds, obj->m_material->GetTextureIDs(), obj->m_material->GetTextureCount(), obj->m_material->GetSampler());
		}
		pipelineBatch.m_batch->UpdateCullParams();
	}

	void RenderBoundingVolumeHierarchy::RecursiveSearchDownForDrawbatches(BuildNode* node, size_t& highestBakedNodeIndex)
	{
		NodeData* nodeData = reinterpret_cast<NodeData*>(node->m_data);
		for (auto& pipelineBatch : nodeData->m_drawBatches)
		{
			highestBakedNodeIndex = std::min<size_t>(highestBakedNodeIndex, pipelineBatch.m_bakedNodeIndex);

			// This batch will be replaced.
			m_allocator->Free(pipelineBatch.m_batch);

			BakedNode& baked = m_globalBatchHierarchy.m_bakedNodes[pipelineBatch.m_bakedNodeIndex];
			baked.m_batch = nullptr;
		}
		nodeData->m_drawBatches.Clear();

		for (auto& child : node->m_children)
		{
			RecursiveSearchDownForDrawbatches(child, highestBakedNodeIndex);
		}
	}

	void RenderBoundingVolumeHierarchy::RecursiveSearchUpForDrawbatches(BuildNode* node, BuildNode*& highestFound, std::set<PB::Pipeline>& pipelinesToFind, size_t& highestBakedNodeIndex)
	{
		NodeData* nodeData = reinterpret_cast<NodeData*>(node->m_data);
		if(nodeData->m_drawBatches.Count() > 0)
		{
			NodeDrawBatches newBatches{};
			for (auto& pipelineBatch : nodeData->m_drawBatches)
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
			nodeData->m_drawBatches = newBatches;
		}

		if (node->m_parent != nullptr && pipelinesToFind.empty() == false)
		{
			RecursiveSearchUpForDrawbatches(node->m_parent, highestFound, pipelinesToFind, highestBakedNodeIndex);
		}
	}

	void RenderBoundingVolumeHierarchy::RebuildDrawBatchesForSubtree(BuildNode* subtree, BuildNode* oldSubtree, InputObjects& objectData)
	{
		std::set<PB::Pipeline> pipelinesToFind;
		for (const BoundingVolumeHierarchy::ObjectData*& obj : objectData)
		{
			const ObjectData* data = reinterpret_cast<const ObjectData*>(obj);

			PB::Pipeline pipeline = data->m_material->GetPipeline();
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

				NodeData* rootData = reinterpret_cast<NodeData*>(root->m_data);
				PipelineDrawbatch& pipelineBatch = rootData->m_drawBatches.Back();

				BuildNode* newBuildNode = m_globalBatchHierarchy.m_buildNodes.PushBack(AllocateBuildNode());
				newBuildNode->m_bounds = pipelineBatch.m_batch->GetBounds();
				newBuildNode->m_objectData = nullptr;
				newBuildNode->m_isObject = true;

				NodeData* nodeData = reinterpret_cast<NodeData*>(newBuildNode->m_data);
				nodeData->m_sourceNode = root;
				nodeData->m_drawBatches.PushBack(pipelineBatch);
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

				NodeData* rootData = reinterpret_cast<NodeData*>(root->m_data);
				PipelineDrawbatch& pipelineBatch = rootData->m_drawBatches.Back();

				BuildNode* newBuildNode = m_globalBatchHierarchy.m_buildNodes.PushBack(AllocateBuildNode());
				newBuildNode->m_bounds = pipelineBatch.m_batch->GetBounds();
				newBuildNode->m_objectData = nullptr;
				newBuildNode->m_isObject = true;

				NodeData* nodeData = reinterpret_cast<NodeData*>(newBuildNode->m_data);
				nodeData->m_sourceNode = root;
				nodeData->m_drawBatches.PushBack(pipelineBatch);
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

			BuildNode* newBuildNode = m_globalBatchHierarchy.m_buildNodes.PushBack(AllocateBuildNode());
			newBuildNode->m_bounds = batch.m_batch->GetBounds();
			newBuildNode->m_objectData = nullptr;
			newBuildNode->m_isObject = true;

			NodeData* nodeData = reinterpret_cast<NodeData*>(newBuildNode->m_data);
			nodeData->m_sourceNode = nullptr;
			nodeData->m_drawBatches.PushBack(batch);
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
			const ObjectData* objectData = reinterpret_cast<const ObjectData*>(node->m_objectData);

			PB::Pipeline pipeline = objectData->m_material->GetPipeline();
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

	void RenderBoundingVolumeHierarchy::RecursiveFreeNode(BuildNode* node, bool keepBatches)
	{
		for (BuildNode* child : node->m_children)
		{
			RecursiveFreeNode(child, keepBatches);
		}

		if (keepBatches == false)
		{
			NodeData* nodeData = reinterpret_cast<NodeData*>(node->m_data);
			for (auto& pipelineBatch : nodeData->m_drawBatches)
			{
				m_allocator->Free(pipelineBatch.m_batch);
			}
		}

		FreeBuildNode(node);
	}

	void RenderBoundingVolumeHierarchy::BakeHierarchies(BuildNode* start)
	{
		CLib::Vector<BuildNode> batchNodes;
	}

	void RenderBoundingVolumeHierarchy::RecursiveBakeHierarchy(BakedNodes& bakedNodes, BuildNode* node, size_t parentIndex, size_t baseIndex)
	{
		NodeData* nodeData = reinterpret_cast<NodeData*>(node->m_data);
		assert(nodeData->m_drawBatches.Count() <= 1);

		size_t index = bakedNodes.size();

		{
			PipelineDrawbatch* nodeBatch = nodeData->m_drawBatches.Count() > 0 ? &nodeData->m_drawBatches.Front() : nullptr;

			BakedNode& baked = bakedNodes.emplace_back();
			baked.m_bounds = node->m_bounds;
			baked.m_childCount = node->m_children.Count();
			baked.m_parentIndex = parentIndex;
			baked.m_batch = nodeBatch ? nodeBatch->m_batch : nullptr;
			baked.m_batchPipeline = nodeBatch ? nodeBatch->m_pipeline : 0;

			if (nodeData->m_sourceNode != nullptr)
			{
				NodeData* sourceNodeData = reinterpret_cast<NodeData*>(nodeData->m_sourceNode->m_data);
				NodeDrawBatches& sourceBatches = sourceNodeData->m_drawBatches;
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

	BoundingVolumeHierarchy::NodeData* RenderBoundingVolumeHierarchy::AllocateNodeData()
	{
		return m_allocator->Alloc<NodeData>();
	}

	void RenderBoundingVolumeHierarchy::FreeNodeData(BoundingVolumeHierarchy::NodeData* data)
	{
		m_allocator->Free(reinterpret_cast<NodeData*>(data));
	}
};