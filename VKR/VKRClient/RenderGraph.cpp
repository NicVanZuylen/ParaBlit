#include "RenderGraph.h"
#include "RenderGraphNode.h"

#include <algorithm>
#include <cassert>

#define RG_ASSERT(expr) assert(expr)

RenderGraphBuilder::RenderGraphBuilder(PB::IRenderer* renderer, CLib::Allocator* allocator)
{
	m_renderer = renderer;
	m_allocator = allocator;
}

RenderGraphBuilder::~RenderGraphBuilder()
{

}

inline PB::TextureStateFlags TextureStateFromAttachmentUsage(PB::AttachmentUsageFlags usage)
{
	PB::TextureStateFlags stateFlags = 0;
	if (usage & PB::EAttachmentUsage::COLOR)
		stateFlags |= PB::ETextureState::COLORTARGET;
	if (usage & PB::EAttachmentUsage::DEPTHSTENCIL)
		stateFlags |= PB::ETextureState::DEPTHTARGET;
	if (usage & PB::EAttachmentUsage::READ)
		stateFlags |= PB::ETextureState::SAMPLED;
	if (usage & PB::EAttachmentUsage::STORAGE)
		stateFlags |= PB::ETextureState::STORAGE;
	return stateFlags;
}

void RenderGraphBuilder::AddNode(const NodeDesc& desc)
{
	auto& currentBuildNode = m_buildNodes.PushBack();
	currentBuildNode.m_attachmentCount = desc.m_attachmentCount;
	currentBuildNode.m_behaviour = desc.m_behaviour;
	currentBuildNode.m_renderWidth = desc.m_renderWidth;
	currentBuildNode.m_renderHeight = desc.m_renderHeight;
	currentBuildNode.m_useReusableCommandLists = desc.m_useReusableCommandLists;
	currentBuildNode.m_computeOnlyPass = desc.m_computeOnlyPass;

	RG_ASSERT(desc.m_renderWidth > 0);
	RG_ASSERT(desc.m_renderHeight > 0);
	RG_ASSERT(desc.m_attachmentCount > 0);
	RG_ASSERT(desc.m_behaviour != nullptr);

	for (uint32_t i = 0; i < desc.m_attachmentCount; ++i)
	{
		auto& meta = currentBuildNode.m_attachments[i];
		auto& attach = desc.m_attachments[i];
		const char* name = attach.m_name;

		if (name)
		{
			meta.m_name = name;
			meta.m_usage = TextureStateFromAttachmentUsage(attach.m_usage);
			UpdateNamedAttachment(attach, m_buildNodes.Count() - 1);
		}
		else
		{
			meta.m_texture = nullptr;
			meta.m_name = nullptr;
			meta.m_format = attach.m_format;
			meta.m_width = attach.m_width;
			meta.m_height = attach.m_height;
			meta.m_mipCount = attach.m_mipCount;
			meta.m_usage = TextureStateFromAttachmentUsage(attach.m_usage);
			meta.m_flags = attach.m_flags;
		}

		currentBuildNode.m_usages[i] = attach.m_usage;
		currentBuildNode.m_clearColors[i] = attach.m_clearColor;
	}
}

RenderGraph* RenderGraphBuilder::Build()
{
	RenderGraph* graph = m_allocator->Alloc<RenderGraph>();
	RenderGraphExecuteNode** currentNode = &graph->m_start;

	auto& passInfo = graph->m_passInfo;
	passInfo.m_allocator = m_allocator;
	passInfo.m_renderer = m_renderer;

	for (uint32_t timePoint = 0; timePoint < m_buildNodes.Count(); ++timePoint)
	{
		RenderGraphExecuteNode* execNode = m_allocator->Alloc<RenderGraphExecuteNode>();
		*currentNode = execNode;
		currentNode = &execNode->m_next;

		auto& buildNode = m_buildNodes[timePoint];
		execNode->m_attachmentCount = buildNode.m_attachmentCount;
		execNode->m_behaviour = buildNode.m_behaviour;
		execNode->m_renderWidth = buildNode.m_renderWidth;
		execNode->m_renderHeight = buildNode.m_renderHeight;
		execNode->m_useReusableCommandLists = buildNode.m_useReusableCommandLists;

		CLib::Vector<AttachmentMeta, _countof(buildNode.m_attachments)> deferredMetaFrees;
		for (uint32_t i = 0; i < buildNode.m_attachmentCount; ++i)
		{
			// Look up existing textures to use for attachments, build views and add them to the node.
			auto& attachMeta = buildNode.m_attachments[i];
			auto* name = attachMeta.m_name;
			if (name)
			{
				auto it = m_namedAttachments.find(attachMeta.m_name);
				RG_ASSERT(it != m_namedAttachments.end());

				auto currentUsage = attachMeta.m_usage;
				attachMeta = it->second.m_meta;
				attachMeta.m_name = name;

				auto& namedAttachMeta = it->second.m_meta;

				if (attachMeta.m_texture == nullptr)
				{
					attachMeta.m_texture = FindTexture(attachMeta, graph);
					namedAttachMeta.m_texture = attachMeta.m_texture;
				}

				attachMeta.m_usage = currentUsage;
				execNode->m_attachments[i] = attachMeta.m_texture;
				execNode->m_attachmentViews[i] = GetTextureView(attachMeta, static_cast<PB::ETextureState>(attachMeta.m_usage));
				execNode->m_attachmentPriorStates[i] = namedAttachMeta.m_lastUsage;
				execNode->m_attachmentSubresourceRanges[i].m_mipCount = namedAttachMeta.m_mipCount;

				// Track most recent usage for the next pass.
				namedAttachMeta.m_lastUsage = TextureStateFromAttachmentUsage(buildNode.m_usages[i]);

				// Track the prior state value of the first pass the texture is used in, to update later when we know the final state.
				if (namedAttachMeta.m_firstPriorState == nullptr)
					namedAttachMeta.m_firstPriorState = &execNode->m_attachmentPriorStates[i];
				else
					*namedAttachMeta.m_firstPriorState = namedAttachMeta.m_lastUsage;

				if (it->second.m_mostRecentTimepoint == timePoint)
				{
					namedAttachMeta.m_name = attachMeta.m_name;
					deferredMetaFrees.PushBack(namedAttachMeta);
				}
			}
			else
			{
				attachMeta.m_texture = FindTexture(attachMeta, graph);

				// Immediately add this texture to the free list since it's not named and therefore a temporary resource.
				deferredMetaFrees.PushBack(attachMeta);

				execNode->m_attachments[i] = attachMeta.m_texture;
				execNode->m_attachmentViews[i] = GetTextureView(attachMeta, static_cast<PB::ETextureState>(attachMeta.m_usage));
				execNode->m_attachmentSubresourceRanges[i].m_mipCount = attachMeta.m_mipCount;
			}
		}

		for (auto& meta : deferredMetaFrees)
		{
			std::pair<size_t, AttachmentMeta> newPair{ GetSizeFromAttachMeta(meta), meta };
			m_freeList.insert(newPair);
		}

		// Render pass
		PB::RenderPassDesc rpDesc{};

		PB::SubpassDesc& subpass = rpDesc.m_subpasses[0];
		PB::AttachmentDesc attachmentDescs[_countof(buildNode.m_attachments)]{};
		rpDesc.m_subpassCount = 1;
		//rpDesc.m_subpasses = &subpass;
		//rpDesc.m_attachments = attachmentDescs;

		// Separate writable attachments from readonly attachments, as readonly attachments are not part of the renderpass and framebuffer.
		CLib::Vector<AttachmentMeta, _countof(buildNode.m_attachments)> validAttachments;
		CLib::Vector<PB::Float4, _countof(buildNode.m_attachments)> validClearColors;
		CLib::Vector<PB::EAttachmentUsage, _countof(buildNode.m_attachments)> validUsages;
		CLib::Vector<PB::RenderTargetView, _countof(buildNode.m_attachments)> validViews;
		for (uint32_t i = 0; i < buildNode.m_attachmentCount; ++i)
		{
			if (buildNode.m_usages[i] == PB::EAttachmentUsage::COLOR 
				|| buildNode.m_usages[i] == PB::EAttachmentUsage::DEPTHSTENCIL)
			{
				validAttachments.PushBack() = buildNode.m_attachments[i];
				validClearColors.PushBack() = buildNode.m_clearColors[i];
				validUsages.PushBack() = buildNode.m_usages[i];
				validViews.PushBack() = execNode->m_attachmentViews[i];
			}

			execNode->m_attachmentStates[i] = buildNode.m_attachments[i].m_usage;
		}

		rpDesc.m_attachmentCount = validAttachments.Count();

		for (uint32_t i = 0; i < rpDesc.m_attachmentCount; ++i)
		{
			// Any named textures that are no longer used beyond this pass can be re-purposed to use in future passes.
			auto& attach = validAttachments[i];

			execNode->m_clearColors[i] = validClearColors[i];

			auto& attachmentDesc = attachmentDescs[i];
			attachmentDesc.m_expectedState = validAttachments[i].m_usage;
			attachmentDesc.m_finalState = attachmentDesc.m_expectedState;
			attachmentDesc.m_format = attach.m_format;

			if (attach.m_name)
			{
				auto namedIt = m_namedAttachments.find(attach.m_name);
				RG_ASSERT(namedIt != m_namedAttachments.end());

				// Don't need to keep contents if this is the last render pass using this texture.
				attachmentDesc.m_keepContents = namedIt->second.m_mostRecentTimepoint != timePoint; 
				// Don't need to load if this is the first pass using this texture.
				PB::EAttachmentAction nonClearAction = namedIt->second.m_earliestTimePoint == timePoint ? PB::EAttachmentAction::NONE : PB::EAttachmentAction::LOAD;

				attachmentDesc.m_loadAction = (attach.m_flags & EAttachmentFlags::CLEAR) ? PB::EAttachmentAction::CLEAR : nonClearAction;
			}
			else
			{
				// Don't bother keeping contents, as this attachment will not be used later in the frame.
				attachmentDesc.m_keepContents = false;
				// Non-named/single pass attachments will have no past contents to load.
				attachmentDesc.m_loadAction = (attach.m_flags & EAttachmentFlags::CLEAR) ? PB::EAttachmentAction::CLEAR : PB::EAttachmentAction::NONE;
			}

			// TODO: REMOVE THIS
			attachmentDesc.m_keepContents = true;

			auto& attachmentUsage = subpass.m_attachments[i];
			attachmentUsage.m_attachmentFormat = attach.m_format;
			attachmentUsage.m_attachmentIdx = i;
			attachmentUsage.m_usage = validUsages[i];
		}

		memcpy(rpDesc.m_attachments, attachmentDescs, sizeof(attachmentDescs));

		// Compute only passes don't need framebuffers or render pass objects.
		if (buildNode.m_computeOnlyPass == false)
		{
			execNode->m_renderPass = m_renderer->GetRenderPassCache()->GetRenderPass(rpDesc);
			execNode->m_behaviour->m_renderPass = execNode->m_renderPass;

			// Framebuffer
			PB::FramebufferDesc fbDesc{};
			memcpy(fbDesc.m_attachmentViews, validViews.Data(), sizeof(PB::RenderTargetView) * validViews.Count());
			fbDesc.m_width = buildNode.m_renderWidth;
			fbDesc.m_height = buildNode.m_renderHeight;
			fbDesc.m_renderPass = execNode->m_renderPass;

			execNode->m_framebuffer = m_renderer->GetFramebufferCache()->GetFramebuffer(fbDesc);
		}
	}

	// All graph resources need to be transitioned to the initial state expected by the graph when executed.
	RecordInitialTransitions();

	return graph;
}

void RenderGraphBuilder::TextureDescFromAttachmentMeta(const AttachmentMeta& meta, PB::TextureDesc& outDesc)
{
	outDesc.m_data.m_format = meta.m_format;
	outDesc.m_width = meta.m_width;
	outDesc.m_height = meta.m_height;
	outDesc.m_mipCount = meta.m_mipCount;
	outDesc.m_initOptions = PB::ETextureInitOptions::PB_TEXTURE_INIT_NONE;
	outDesc.m_initialState = PB::ETextureState::NONE;
	outDesc.m_usageStates = meta.m_usage;

	if (meta.m_flags & EAttachmentFlags::COPY_SRC)
		outDesc.m_usageStates |= PB::ETextureState::COPY_SRC;
	if (meta.m_flags & EAttachmentFlags::COPY_DST)
		outDesc.m_usageStates |= PB::ETextureState::COPY_DST;
	if (meta.m_flags & EAttachmentFlags::SECONDARY_SAMPLED)
		outDesc.m_usageStates |= PB::ETextureState::SAMPLED;
	if (meta.m_flags & EAttachmentFlags::SECONDARY_STORAGE)
		outDesc.m_usageStates |= PB::ETextureState::STORAGE;
}

void RenderGraphBuilder::UpdateNamedAttachment(const AttachmentDesc& desc, uint32_t timePoint)
{
	auto it = m_namedAttachments.find(desc.m_name);
	if (it != m_namedAttachments.end())
	{
		auto& attach = it->second;
		attach.m_mostRecentTimepoint = timePoint;

		if (desc.m_width > attach.m_meta.m_width)
			attach.m_meta.m_width = desc.m_width;
		if (desc.m_height > attach.m_meta.m_height)
			attach.m_meta.m_height = desc.m_height;
		if (desc.m_mipCount > attach.m_meta.m_mipCount)
			attach.m_meta.m_mipCount = desc.m_mipCount;

		attach.m_meta.m_usage |= TextureStateFromAttachmentUsage(desc.m_usage);
		attach.m_meta.m_flags |= desc.m_flags;
	}
	else
	{
		std::pair<const char*, NamedAttachmentMeta> newPair(desc.m_name, {});
		auto& newAttach = newPair.second;

		newAttach.m_name = desc.m_name;
		newAttach.m_earliestTimePoint = timePoint;
		newAttach.m_mostRecentTimepoint = timePoint;

		newAttach.m_meta.m_format = desc.m_format;
		newAttach.m_meta.m_width = desc.m_width;
		newAttach.m_meta.m_height = desc.m_height;
		newAttach.m_meta.m_mipCount = desc.m_mipCount;
		newAttach.m_meta.m_usage = TextureStateFromAttachmentUsage(desc.m_usage);
		newAttach.m_meta.m_firstPriorState = nullptr;
		newAttach.m_meta.m_flags = desc.m_flags;

		m_namedAttachments.insert(newPair);
	}
}

void RenderGraphBuilder::RecordInitialTransitions()
{
	PB::CommandContextDesc contextDesc{};
	contextDesc.m_renderer = m_renderer;
	contextDesc.m_usage = PB::ECommandContextUsage::GRAPHICS;

	PB::SCommandContext scopedCmdContext(m_renderer);
	PB::ICommandContext* cmdContext = scopedCmdContext.GetContext();
	cmdContext->Init(contextDesc);

	cmdContext->Begin();

	for (auto& namedAttachment : m_namedAttachments)
	{
		auto& meta = namedAttachment.second.m_meta;
		PB::SubresourceRange subresourceRange{};
		subresourceRange.m_mipCount = meta.m_mipCount;
		cmdContext->CmdTransitionTexture(meta.m_texture, PB::ETextureState::NONE, *meta.m_firstPriorState);
	}

	cmdContext->End();
	cmdContext->Return();
}

inline uint32_t GetFormatBytesPerPixel(PB::ETextureFormat format)
{
	switch (format)
	{
	case PB::ETextureFormat::UNKNOWN:
		return 0;
	case PB::ETextureFormat::R8_UNORM:
		return 1;
	case PB::ETextureFormat::R8G8_UNORM:
		return 2;
	case PB::ETextureFormat::R8G8B8_UNORM:
		return 4;
	case PB::ETextureFormat::R8G8B8A8_UNORM:
		return 4;
	case PB::ETextureFormat::B8G8R8A8_UNORM:
		return 4;
	case PB::ETextureFormat::R32_FLOAT:
		return 4;
	case PB::ETextureFormat::R32G32_FLOAT:
		return 8;
	case PB::ETextureFormat::R32G32B32_FLOAT:
		return 12;
	case PB::ETextureFormat::R32G32B32A32_FLOAT:
		return 16;
	case PB::ETextureFormat::D16_UNORM:
		return 2;
	case PB::ETextureFormat::D16_UNORM_S8_UINT:
		return 4;
	case PB::ETextureFormat::D24_UNORM_S8_UINT:
		return 4;
	case PB::ETextureFormat::D32_FLOAT:
		return 4;
	case PB::ETextureFormat::D32_FLOAT_S8_UINT:
		return 8;
	default:
		// Not implemented.
		RG_ASSERT(false && "Format not implemented.");
		return 0;
	}
	return 0;
}

size_t RenderGraphBuilder::GetSizeFromAttachMeta(const AttachmentMeta& meta)
{
	return meta.m_width * meta.m_height * GetFormatBytesPerPixel(meta.m_format);
}

PB::ITexture* RenderGraphBuilder::FindTexture(const AttachmentMeta& meta, RenderGraph* graph)
{
	size_t desiredSize = GetSizeFromAttachMeta(meta);

	// If compareData.anySuitable is true, we can just pop the back element off of the free list and use it, since it should be the most suitable texture.
	if (m_freeList.empty() == false)
	{
		PB::TextureDesc aliasDesc{};
		TextureDescFromAttachmentMeta(meta, aliasDesc);
		aliasDesc.m_data.m_aliasOther = true;
		PB::ITexture* newAlias = m_renderer->AllocateTexture(aliasDesc);

		// Find the smallest free texture large enough for the desired size.
		auto freeIt = m_freeList.lower_bound(desiredSize);
		if (freeIt != m_freeList.end() && newAlias->CanAlias(freeIt->second.m_texture))
		{
			printf("Alias texture [%s] created from memory of [%s].\n", meta.m_name, freeIt->second.m_name);

			newAlias->AliasTexture(freeIt->second.m_texture);
			graph->m_aliasTextures.PushBack(newAlias);

			// Remove free list.
			m_freeList.erase(freeIt);

			return newAlias;
		}

		m_renderer->FreeTexture(newAlias);
	}

	// No suitable textures available. Create a new one and return it's metadata. (which should be identical to that provided.)
	auto* newTex = CreateTexture(meta);
	graph->m_baseTextures.PushBack(newTex);
	return newTex;
}

PB::ITexture* RenderGraphBuilder::CreateTexture(const AttachmentMeta& meta)
{
	PB::TextureDesc desc{};
	TextureDescFromAttachmentMeta(meta, desc);
	return m_renderer->AllocateTexture(desc);
}

PB::RenderTargetView RenderGraphBuilder::GetTextureView(const AttachmentMeta& meta, PB::ETextureState expectedState)
{
	PB::TextureViewDesc viewDesc;
	viewDesc.m_format = meta.m_format;
	viewDesc.m_subresources = {};
	viewDesc.m_texture = meta.m_texture;
	viewDesc.m_expectedState = expectedState;
	
	if(expectedState != PB::ETextureState::SAMPLED && expectedState != PB::ETextureState::STORAGE)
		return meta.m_texture->GetRenderTargetView(viewDesc);
	else
		return meta.m_texture->GetView(viewDesc);
}

RenderGraph::~RenderGraph()
{
	for (auto& tex : m_aliasTextures)
		m_passInfo.m_renderer->FreeTexture(tex);
	for (auto& tex : m_baseTextures)
		m_passInfo.m_renderer->FreeTexture(tex);

	CLib::Vector<RenderGraphExecuteNode*, 8, 8> nodesToFree;
	RenderGraphExecuteNode* currentNode = m_start;
	while (currentNode != nullptr)
	{
		nodesToFree.PushBack(currentNode);
		currentNode = currentNode->m_next;
	}
	for (auto& node : nodesToFree)
		m_passInfo.m_allocator->Free(node);
}

void RenderGraph::Execute()
{
	PB::CommandContextDesc contextDesc{};
	contextDesc.m_renderer = m_passInfo.m_renderer;
	contextDesc.m_usage = PB::ECommandContextUsage::GRAPHICS;

	PB::SCommandContext cmdContext(m_passInfo.m_renderer);
	m_passInfo.m_commandContext = cmdContext.GetContext();
	cmdContext->Init(contextDesc);

	cmdContext->Begin();

	RenderGraphExecuteNode* currentNode = m_start;
	while (currentNode != nullptr)
	{
		m_passInfo.m_renderPass = currentNode->m_renderPass;
		m_passInfo.m_frameBuffer = currentNode->m_framebuffer;
		m_passInfo.m_renderTargets = currentNode->m_attachments;
		m_passInfo.m_renderTargetViews = currentNode->m_attachmentViews;
		m_passInfo.m_renderTargetCount = currentNode->m_attachmentCount;

		for (uint32_t i = 0; i < currentNode->m_attachmentCount; ++i)
		{
			if (currentNode->m_attachmentStates[i] != PB::ETextureState::NONE)
				cmdContext->CmdTransitionTexture(currentNode->m_attachments[i], currentNode->m_attachmentPriorStates[i], currentNode->m_attachmentStates[i], currentNode->m_attachmentSubresourceRanges[i]);
		}

		currentNode->m_behaviour->OnPrePass(m_passInfo);
		if(m_passInfo.m_renderPass)
			cmdContext->CmdBeginRenderPass(currentNode->m_renderPass, currentNode->m_renderWidth, currentNode->m_renderHeight, currentNode->m_framebuffer, currentNode->m_clearColors, currentNode->m_attachmentCount, currentNode->m_useReusableCommandLists);

		currentNode->m_behaviour->OnPassBegin(m_passInfo);
		if (m_passInfo.m_renderPass)
			cmdContext->CmdEndRenderPass();
		currentNode->m_behaviour->OnPostPass(m_passInfo);

		currentNode = currentNode->m_next;
	}

	cmdContext->End();
	cmdContext->Return();
}
