#include "RenderBoundingVolumeHierarchy.h"
#include "WorldRender/BatchDispatcher.h"
#include "RenderGraphPasses/DebugLinePass.h"
#include "Resource/Mesh.h"
#include "Resource/Material.h"

#include "glm/gtc/type_ptr.hpp"
#include <algorithm>

namespace Eng
{

	RenderBoundingVolumeHierarchy::RenderBoundingVolumeHierarchy(PB::IRenderer* renderer, CLib::Allocator* extAllocator, AssetStreamer* streamer, const CreateDesc& desc)
		: BoundingVolumeHierarchy(desc)
	{
		m_renderer = renderer;
		m_extAllocator = extAllocator;
		m_streamer = streamer;
	}

	RenderBoundingVolumeHierarchy::~RenderBoundingVolumeHierarchy()
	{

	}

	void RenderBoundingVolumeHierarchy::Init(PB::IRenderer* renderer, CLib::Allocator* extAllocator, AssetStreamer* streamer, const CreateDesc& desc)
	{
		m_nodeAllocator = new CLib::FixedBlockAllocator(sizeof(BuildNode) + sizeof(RenderBoundingVolumeHierarchy::NodeData));

		BoundingVolumeHierarchy::Init(desc);

		m_renderer = renderer;
		m_extAllocator = extAllocator;
		m_streamer = streamer;
	}

	void RenderBoundingVolumeHierarchy::Destroy()
	{
		if (m_root != nullptr)
		{
			RecursiveFreeNode(m_root);
			m_root = nullptr;
		}

		if (m_nodeAllocator != nullptr)
		{
			delete m_nodeAllocator;
		}
	}

	BoundingVolumeHierarchy::BuildNode* RenderBoundingVolumeHierarchy::BuildBottomUp(InputObjects& objects, uint32_t baseDepth)
	{
		BuildNode* buildResult = BoundingVolumeHierarchy::BuildBottomUp(objects, baseDepth);

		BatchMap batchMap;
		std::set<PB::Pipeline> excludedPipelines;
		BuildDrawBatchesExclusive(buildResult, batchMap, excludedPipelines);

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

		BuildNode* newSubtree = BoundingVolumeHierarchy::BuildBottomUp(objects, subtreeRoot->m_depth);
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
			assert(newSubtree->m_depth == 0);
			m_root = newSubtree;
		}

		// With the new subtree linked, we can now build or rebuild any necessary draw batches.
		RebuildDrawBatchesForSubtree(newSubtree, subtreeRoot, objects);

		// Free old subtree including its batches.
		RecursiveFreeNode(subtreeRoot, false);
	}

	void RenderBoundingVolumeHierarchy::BakeBatches(PB::ICommandContext* commandContext)
	{
		for (auto& batch : m_drawBatchesToUpdate)
		{
			batch->UpdateIndices(commandContext);
		}
		m_drawBatchesToUpdate.Clear();
	}

	void RenderBoundingVolumeHierarchy::DebugDrawBatchTree(const Camera* camera, DebugLinePass* lines, uint32_t desiredDepth)
	{
		Camera::CameraFrustrum frustrum;
		if (camera != nullptr)
		{
			frustrum = camera->GetFrustrum();
			glm::vec3 frustrumColor(1.0f, 0.0f, 1.0f);
			Camera::DrawFrustrum(lines, frustrum, frustrumColor);
		}

		RecursiveDebugDrawBatches(lines, frustrum, m_root, desiredDepth);
	}

	void RenderBoundingVolumeHierarchy::BuildDrawBatch(PB::Pipeline pipeline, BuildNode* node, BatchObjects& objects)
	{
		NodeData* nodeData = node->GetData<NodeData>();

		PipelineDrawbatch& pipelineBatch = nodeData->m_drawBatches.PushBack();
		pipelineBatch.m_pipeline = pipeline;

		DrawBatch::CreateDesc drawBatchDesc;
		drawBatchDesc.m_renderer = m_renderer;
		drawBatchDesc.m_allocator = m_extAllocator;
		drawBatchDesc.m_streamer = m_streamer;
		pipelineBatch.m_batch = m_extAllocator->Alloc<DrawBatch>(drawBatchDesc);

		for (auto& node : objects)
		{
			const ObjectData* obj = reinterpret_cast<const ObjectData*>(node->m_objectData);

			pipelineBatch.m_batch->AddInstance(obj->m_meshID, glm::value_ptr(obj->m_transform), node->m_bounds, obj->m_material->GetTextureIDs(), obj->m_material->GetTextureCount(), obj->m_material->GetSampler());
		}
		pipelineBatch.m_batch->UpdateCullParams();
	}

	void RenderBoundingVolumeHierarchy::RecursiveSearchDownForDrawbatches(BuildNode* node, std::set<PB::Pipeline>& pipelinesToFind)
	{
		NodeData* nodeData = node->GetData<NodeData>();
		if (nodeData->m_drawBatches.Count() > 0)
		{
			for (auto& pipelineBatch : nodeData->m_drawBatches)
			{
				auto it = pipelinesToFind.find(pipelineBatch.m_pipeline);
				if (it != pipelinesToFind.end())
				{
					pipelinesToFind.erase(it);
				}
			}
		}		

		for(auto& child : node->m_children)
			RecursiveSearchDownForDrawbatches(child, pipelinesToFind);
	}

	void RenderBoundingVolumeHierarchy::RecursiveSearchUpForDrawbatches(BuildNode* node, BuildNode*& highestBatchNode, std::set<PB::Pipeline>& pipelinesToFind)
	{
		NodeData* nodeData = node->GetData<NodeData>();
		if(nodeData->m_drawBatches.Count() > 0)
		{
			NodeDrawBatches newBatches{};
			for (auto& pipelineBatch : nodeData->m_drawBatches)
			{
				auto it = pipelinesToFind.find(pipelineBatch.m_pipeline);
				if(it != pipelinesToFind.end())
				{
					pipelinesToFind.erase(it);

					// Batch will be replaced.
					m_extAllocator->Free(pipelineBatch.m_batch);

					highestBatchNode = node;
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
			RecursiveSearchUpForDrawbatches(node->m_parent, highestBatchNode, pipelinesToFind);
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

		// Eliminate pipelines for searching up by searching down first.
		RecursiveSearchDownForDrawbatches(oldSubtree, pipelinesToFind);

		// Search above the rebuilt subtree for any batch nodes which need to be updated. Rebuild batches from there.
		BuildNode* highestBatchNode = subtree;
		RecursiveSearchUpForDrawbatches(subtree, highestBatchNode, pipelinesToFind);
		assert(highestBatchNode != nullptr);
		assert(pipelinesToFind.empty());

		// Rebuild all drawbatches under the highest batch node we're updating from.
		BatchMap batchMap;
		std::set<PB::Pipeline> excludedPipelines;
		RebuildDrawBatchesExclusive(highestBatchNode, batchMap, excludedPipelines);

		//// Build a BVH of the drawbatch nodes to later bake into the new set of baked nodes.
		//BuildNode* batchSubtree = BuildBottomUpInternal(m_globalBatchHierarchy.m_buildNodes);
		//m_globalBatchHierarchy.m_buildNodes.Clear();

		//// Find the corresponding range of baked nodes (parent and children) to the old drawbatches which need to be removed.
		//// This range will be replaced with the baked nodes of the new batches.
		//CLib::Vector<BakedNode*> correspondingBakedNodes;
		//FindCorrespondingBakedNodes(correspondingBakedNodes, m_globalBatchHierarchy.m_bakedNodes.front(), *batchSubtree);
		//BakedNode* updateRoot = correspondingBakedNodes[0];
		//size_t updateRootIndex = updateRoot - m_globalBatchHierarchy.m_bakedNodes.data();
		//size_t updateRootParentIndex = m_globalBatchHierarchy.m_bakedNodes[updateRootIndex].m_parentIndex;
		//assert(correspondingBakedNodes.Count() == 1);

		//BakedNodes newBakedNodes;
		//RecursiveBakeHierarchy(newBakedNodes, batchSubtree, updateRootParentIndex, updateRootIndex);
		//RecursiveFreeNode(batchSubtree, true);

		//int64_t parentStrideDiff = int64_t(newBakedNodes.size()) - updateRoot->m_strideToNext;
		//BakedNodes& finalBakedNodes = m_globalBatchHierarchy.m_bakedNodes;
		//finalBakedNodes.erase
		//(
		//	finalBakedNodes.begin() + updateRootIndex,
		//	finalBakedNodes.begin() + updateRootIndex + updateRoot->m_strideToNext
		//);

		//finalBakedNodes.insert(finalBakedNodes.begin() + updateRootIndex, newBakedNodes.begin(), newBakedNodes.end());

		//if (parentStrideDiff != 0)
		//{
		//	size_t parentIndex = updateRootParentIndex;
		//	while (parentIndex != ~size_t(0))
		//	{
		//		BakedNode& parent = finalBakedNodes[parentIndex];
		//		parent.m_strideToNext += parentStrideDiff;

		//		parentIndex = parent.m_parentIndex;
		//	}
		//}

		//m_globalBatchHierarchy.m_bakedNodeUpdateIndices.PushBack(updateRootIndex);
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

			if (pair.second.Count() <= DrawBatch::MaxObjects)
			{
				BuildDrawBatch(pair.first, root, pair.second);
				newlyExcludedPipelines.insert(pair.first);

				NodeData* rootData = root->GetData<NodeData>();
				PipelineDrawbatch& pipelineBatch = rootData->m_drawBatches.Back();

				m_drawBatchesToUpdate.PushBack(pipelineBatch.m_batch);
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

	void RenderBoundingVolumeHierarchy::RebuildDrawBatchesExclusive(BuildNode* root, BatchMap& batchMap, std::set<PB::Pipeline> excludedPipelines)
	{
		batchMap.clear();
		GetBatchCandidates(batchMap, root);

		// Clear old drawbatches
		NodeData* nodeData = root->GetData<NodeData>();
		for (auto& pipelineBatch : nodeData->m_drawBatches)
		{
			m_extAllocator->Free(pipelineBatch.m_batch);
		}
		nodeData->m_drawBatches.Clear();

		for (auto pair : batchMap)
		{
			if (pair.second.Count() <= DrawBatch::MaxObjects && excludedPipelines.contains(pair.first) == false)
			{
				BuildDrawBatch(pair.first, root, pair.second);
				excludedPipelines.insert(pair.first);

				NodeData* rootData = root->GetData<NodeData>();
				PipelineDrawbatch& pipelineBatch = rootData->m_drawBatches.Back();

				m_drawBatchesToUpdate.PushBack(pipelineBatch.m_batch);
			}
		}

		for (auto* child : root->m_children)
		{
			RebuildDrawBatchesExclusive(child, batchMap, excludedPipelines);
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
			NodeData* nodeData = node->GetData<NodeData>();
			for (auto& pipelineBatch : nodeData->m_drawBatches)
			{
				m_extAllocator->Free(pipelineBatch.m_batch);
			}
		}

		FreeBuildNode(node);
	}

	void RenderBoundingVolumeHierarchy::RecursiveCullBatchesByNode(BatchDispatcher* dispatcher, const PB::BindingLayout& globalBindings, const Camera::CameraFrustrum& frustrum, const BuildNode* node) const
	{
		if (FrustrumTest(frustrum, node->m_bounds))
		{
			const NodeData* data = node->GetData<NodeData>();
			for (auto& batch : data->m_drawBatches)
			{
				dispatcher->AddBatch(batch.m_batch, batch.m_pipeline, globalBindings);
			}

			for (const BuildNode* child : node->m_children)
			{
				RecursiveCullBatchesByNode(dispatcher, globalBindings, frustrum, child);
			}
		}
	}

	void RenderBoundingVolumeHierarchy::RecursiveDebugDrawBatches(DebugLinePass* lines, const Camera::CameraFrustrum& frustrum, const BuildNode* node, uint32_t desiredDepth, uint32_t depth)
	{
		if (FrustrumTest(frustrum, node->m_bounds))
		{
			for (const BuildNode* child : node->m_children)
			{
				RecursiveDebugDrawBatches(lines, frustrum, child, desiredDepth, depth + 1);
			}

			if (depth == desiredDepth || desiredDepth == ~uint32_t(0))
			{
				const NodeData* data = node->GetData<NodeData>();
				for (auto& pipelineBatch : data->m_drawBatches)
				{
					const Bounds& batchBounds = pipelineBatch.m_batch->GetBounds();
					DebugDrawCube(lines, batchBounds.m_origin, batchBounds.m_extents, glm::vec3(1.0f, 0.0f, 0.0f));
				}

				if (data->m_drawBatches.Count() == 0)
					DebugDrawCube(lines, node->m_bounds.m_origin, node->m_bounds.m_extents, glm::vec3(0.0f, 1.0f, 0.0f));
			}
		}
	}

	void RenderBoundingVolumeHierarchy::CullBatches(const Camera::CameraFrustrum& cameraFrustrum, BatchDispatcher* dispatcher, const PB::BindingLayout& globalBindings) const
	{
		RecursiveCullBatchesByNode(dispatcher, globalBindings, cameraFrustrum, m_root);
	}

	void RenderBoundingVolumeHierarchy::AllocateNodeData(BuildNode* node)
	{
		new (node + 1) NodeData;
	}

	void RenderBoundingVolumeHierarchy::FreeNodeData(BuildNode* node)
	{
		reinterpret_cast<NodeData*>(node + 1)->~NodeData();
	}
};