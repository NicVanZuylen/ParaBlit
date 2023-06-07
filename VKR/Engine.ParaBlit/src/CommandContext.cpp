#include "CommandContext.h"
#include "FramebufferCache.h"
#include "ParaBlitDebug.h"
#include "PBUtil.h"
#include "Renderer.h"
#include "CLib/Vector.h"
#include "Texture.h"
#include "BufferObject.h"
#include "IBindingCache.h"

namespace PB 
{
	CommandList::CommandList(Renderer* renderer, VkCommandBuffer cmdBuffer)
	{
		m_renderer = renderer;
		m_cmdBuffer = cmdBuffer;
	}

	PFN_vkCmdBeginRenderingKHR CommandContext::vkCmdBeginRenderingKHRFunc;
	PFN_vkCmdEndRenderingKHR CommandContext::vkCmdEndRenderingKHRFunc;
	PFN_vkCmdDrawMeshTasksEXT CommandContext::vkCmdDrawMeshTasksEXTFunc;
	PFN_vkCmdDrawMeshTasksIndirectEXT CommandContext::vkCmdDrawMeshTasksIndirectEXTFunc;
	PFN_vkCmdDrawMeshTasksIndirectCountEXT CommandContext::vkCmdDrawMeshTasksIndirectCountEXTFunc;

	void CommandContext::GetExtensionFunctions(VkInstance instance)
	{
		// From VK_KHR_dynamic_rendering:
		vkCmdBeginRenderingKHRFunc = (PFN_vkCmdBeginRenderingKHR)vkGetInstanceProcAddr(instance, "vkCmdBeginRenderingKHR");
		vkCmdEndRenderingKHRFunc = (PFN_vkCmdEndRenderingKHR)vkGetInstanceProcAddr(instance, "vkCmdEndRenderingKHR");

		// From VK_EXT_mesh_shaders:
		vkCmdDrawMeshTasksEXTFunc = (PFN_vkCmdDrawMeshTasksEXT)vkGetInstanceProcAddr(instance, "vkCmdDrawMeshTasksEXT");
		vkCmdDrawMeshTasksIndirectEXTFunc = (PFN_vkCmdDrawMeshTasksIndirectEXT)vkGetInstanceProcAddr(instance, "vkCmdDrawMeshTasksIndirectEXT");
		vkCmdDrawMeshTasksIndirectCountEXTFunc = (PFN_vkCmdDrawMeshTasksIndirectCountEXT)vkGetInstanceProcAddr(instance, "vkCmdDrawMeshTasksIndirectCountEXT");
	}

	CommandContext::CommandContext()
	{
		m_isPriority = false;
		m_isInternal = false;
		m_activeRenderpass = false;
		m_activePipelineIsCompute = false;
		m_activePipelineHasMeshShader = false;
		m_reusable = false;
	}

	CommandContext::~CommandContext()
	{
		if (m_cmdBuffer != VK_NULL_HANDLE)
		{
			if (m_state == ECmdContextState::RECORDING)
			{
				End();
				Return();
			}
			else if (m_state == ECmdContextState::PENDING_SUBMISSION)
			{
				Return();
			}
			else
			{
				m_renderer->ReturnOpenBuffer(*this);
				m_state = ECmdContextState::OPEN;
			}

			m_cmdBuffer = VK_NULL_HANDLE;
		}

		if (m_bindingState)
		{
			m_renderer->GetAllocator().Free(m_bindingState);
			m_bindingState = nullptr;
		}
	}

	void CommandContext::Init(CommandContextDesc& desc)
	{
		PB_ASSERT(desc.m_renderer);
		PB_ASSERT(desc.m_usage != ECommandContextUsage::END_RANGE);
		PB_ASSERT_MSG(!(((desc.m_flags & ECommandContextFlags::REUSABLE) > 0) && ((desc.m_flags & ECommandContextFlags::PRIORITY) > 0)), "Reusable command contexts cannot be prioritized, as they are executed at the user's discretion.");

		m_renderer = reinterpret_cast<Renderer*>(desc.m_renderer);
		m_device = m_renderer->GetDevice();
		if (!m_bindingState)
			m_bindingState = m_renderer->GetAllocator().Alloc<BindingState>();
		else
		{
			m_bindingState->Clear();
			m_bindingState->ClearUBOSets();
		}

		m_cmdBuffer = VK_NULL_HANDLE;
		m_curPipelineLayout = VK_NULL_HANDLE;
		m_state = ECmdContextState::OPEN;
		m_usage = desc.m_usage;
		m_isPriority = (desc.m_flags & ECommandContextFlags::PRIORITY) > 0;
		m_isInternal = false;
		m_activeRenderpass = false;
		m_reusable = (desc.m_flags & ECommandContextFlags::REUSABLE) > 0;
		m_reusableForRenderPass = false;
	}

	void CommandContext::Begin(PB::RenderPass renderPass, PB::Framebuffer frameBuffer)
	{
		// Obtain command buffer, a new one is allocated if there are no free command buffers.
		if (m_cmdBuffer == VK_NULL_HANDLE)
		{
			m_cmdBuffer = m_renderer->AllocateCommandBuffer(m_reusable);
			PB_COMMAND_CONTEXT_LOG("Obtained command buffer [%X] for command context [%X].", m_cmdBuffer, this);
		}

		VkCommandBufferInheritanceInfo inheritInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, nullptr };
		inheritInfo.renderPass = reinterpret_cast<VkRenderPass>(renderPass);
		inheritInfo.subpass = 0;
		inheritInfo.framebuffer = reinterpret_cast<VkFramebuffer>(frameBuffer);
		inheritInfo.occlusionQueryEnable = VK_FALSE;

		if (renderPass != nullptr && frameBuffer == nullptr && m_device->GetDynamicRenderingFeatures()->dynamicRendering == VK_TRUE)
		{
			RenderPassCache::DynamicRenderPass* dynamicPass = reinterpret_cast<RenderPassCache::DynamicRenderPass*>(inheritInfo.renderPass);
			inheritInfo.renderPass = nullptr;
			inheritInfo.pNext = &dynamicPass->m_inheritanceInfo;
		}

		VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr };
		if (m_reusable)
		{
			m_reusableForRenderPass = renderPass != nullptr;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | (renderPass != nullptr ? VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT : 0);
		}
		else
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		beginInfo.pInheritanceInfo = &inheritInfo;

		VkResult res = vkBeginCommandBuffer(m_cmdBuffer, &beginInfo);
		if (res != VK_SUCCESS)
		{
			PB_ASSERT_MSG(false, "Failed to begin command buffer recording.");
			m_state = ECmdContextState::OPEN;
		}
		else
			m_state = ECmdContextState::RECORDING;
	}

	void CommandContext::End()
	{
		PB_ASSERT_MSG(m_state == ECmdContextState::RECORDING, "Cannot end recording of a command context that is not currently recording.");
		if (m_state != ECmdContextState::RECORDING)
			return;

		if (m_activeRenderpass)
		{
			m_activeRenderpass = false;
			vkCmdEndRenderPass(m_cmdBuffer);
		}
		PB_ERROR_CHECK(vkEndCommandBuffer(m_cmdBuffer));
		PB_BREAK_ON_ERROR;

		m_state = ECmdContextState::PENDING_SUBMISSION;
	}

	ICommandList* CommandContext::Return()
	{
		PB_ASSERT_MSG(m_state == ECmdContextState::PENDING_SUBMISSION, "Cannot submit command context that is not yet recorded or currently recording.");

		m_state = ECmdContextState::OPEN;
		if (!m_reusable)
		{
			PB_COMMAND_CONTEXT_LOG("Returned recorded command buffer [%X] from command context [%X] for submission.", m_cmdBuffer, this);
			m_renderer->ReturnCommandBuffer(*this); // Give the command buffer back to the renderer for submission at the end of the frame.
			return nullptr;
		}
		else
		{
			PB_COMMAND_CONTEXT_LOG("Returned recorded command list containing command buffer [%X] from command context [%X] for execution in another command context.", m_cmdBuffer, this);
			PB::CommandList* returnList = m_renderer->GetAllocator().Alloc<CommandList>(m_renderer, m_cmdBuffer);
			returnList->GetUBOSets() += m_bindingState->m_uboSets;
			return returnList;
		}
	}

	void CommandContext::CmdBeginRenderPass(RenderPass renderPass, u32 width, u32 height, RenderTargetView* attachmentViews, u32 viewCount, Float4* clearColors, u32 clearColorCount, bool useCommandLists)
	{
		ValidateRecordingState();
		PB_ASSERT(renderPass);
		PB_ASSERT(attachmentViews);
		PB_ASSERT(viewCount > 0);
		PB_ASSERT_MSG(!m_reusable, "Reusable command contexts cannot be used to begin render passes.");
		if (m_activeRenderpass)
			CmdEndRenderPass();

		VkRenderPass pass = reinterpret_cast<VkRenderPass>(renderPass);
		PB_ASSERT(pass);

		FramebufferDesc fbDesc;
		for(u32 i = 0; i < viewCount; ++i)
			fbDesc.m_attachmentViews[i] = attachmentViews[i];
		fbDesc.m_renderPass = pass;
		fbDesc.m_width = width;
		fbDesc.m_height = height;
		VkFramebuffer framebuffer = reinterpret_cast<VkFramebuffer>(m_renderer->GetFramebufferCache()->GetFramebuffer(fbDesc));
		PB_ASSERT(framebuffer);

		VkRenderPassBeginInfo beginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr };
		beginInfo.framebuffer = framebuffer;
		beginInfo.renderPass = pass;
		beginInfo.renderArea.offset = { 0, 0 };
		beginInfo.renderArea.extent = { width, height };
		beginInfo.clearValueCount = clearColorCount;
		beginInfo.pClearValues = reinterpret_cast<VkClearValue*>(clearColors);

		vkCmdBeginRenderPass(m_cmdBuffer, &beginInfo, !useCommandLists ? VK_SUBPASS_CONTENTS_INLINE : VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
		m_activeRenderpass = true;
		m_activeRenderPassIsDynamic = false;
	}

	void CommandContext::CmdBeginRenderPass(RenderPass renderPass, u32 width, u32 height, Framebuffer frameBuffer, Float4* clearColors, u32 clearColorCount, bool useCommandLists)
	{
		ValidateRecordingState();
		PB_ASSERT(renderPass);
		PB_ASSERT_MSG(!m_reusable, "Reusable command contexts cannot be used to begin render passes.");
		PB_ASSERT_MSG(!m_activeRenderpass, "Cannot start a new render pass while another is currently active.");

		VkRenderPass pass = reinterpret_cast<VkRenderPass>(renderPass);
		PB_ASSERT(pass);

		VkRenderPassBeginInfo beginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr };
		beginInfo.framebuffer = reinterpret_cast<VkFramebuffer>(frameBuffer);
		beginInfo.renderPass = pass;
		beginInfo.renderArea.offset = { 0, 0 };
		beginInfo.renderArea.extent = { width, height };
		beginInfo.clearValueCount = clearColorCount;
		beginInfo.pClearValues = reinterpret_cast<VkClearValue*>(clearColors);

		vkCmdBeginRenderPass(m_cmdBuffer, &beginInfo, !useCommandLists ? VK_SUBPASS_CONTENTS_INLINE : VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
		m_activeRenderpass = true;
		m_activeRenderPassIsDynamic = false;
	}

	void CommandContext::CmdBeginRenderPassDynamic(RenderPass renderPass, u32 width, u32 height, RenderTargetView* attachmentViews, Float4* clearColors, bool useCommandLists)
	{
		ValidateRecordingState();
		PB_ASSERT(renderPass);
		PB_ASSERT_MSG(!m_reusable, "Reusable command contexts cannot be used to begin render passes.");
		PB_ASSERT_MSG(!m_activeRenderpass, "Cannot start a new render pass while another is currently active.");

		RenderPassCache::DynamicRenderPass* pass = reinterpret_cast<RenderPassCache::DynamicRenderPass*>(renderPass);
		PB_ASSERT(pass);

		// While most information for the pass is already known, some information needs to be set when the pass begins.
		VkRenderingInfoKHR& info = pass->m_renderingInfo;
		info.flags = useCommandLists ? VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR : 0;
		info.renderArea.offset = { 0, 0 };
		info.renderArea.extent = { width, height };

		u32 attachIndex = 0;
		u32 clearColorIndex = 0;
		for (auto& attach : pass->m_colorAttachmentInfos)
		{
			TextureViewData* viewData = reinterpret_cast<TextureViewData*>(attachmentViews[attachIndex++]);
			if (viewData->m_expectedState == PB::ETextureState::DEPTHTARGET)
			{
				auto& depthAttach = pass->m_depthAttachmentInfo;
				depthAttach.imageView = viewData->m_view;
				if(depthAttach.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
					depthAttach.clearValue = *reinterpret_cast<VkClearValue*>(&clearColors[clearColorIndex++]);

				viewData = reinterpret_cast<TextureViewData*>(attachmentViews[attachIndex++]);
			}

			attach.imageView = viewData->m_view;
			if (attach.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
				attach.clearValue = *reinterpret_cast<VkClearValue*>(&clearColors[clearColorIndex++]);
		}
		
		vkCmdBeginRenderingKHRFunc(m_cmdBuffer, &info);
		m_activeRenderpass = true;
		m_activeRenderPassIsDynamic = true;
	}

	void CommandContext::CmdEndRenderPass()
	{
		PB_ASSERT_MSG(!m_reusable, "Reusable command contexts cannot be used to end render passes.");
		if (m_activeRenderpass)
		{
			if (m_activeRenderPassIsDynamic)
				vkCmdEndRenderingKHRFunc(m_cmdBuffer);
			else
				vkCmdEndRenderPass(m_cmdBuffer);

			m_activeRenderpass = false;
		}
	}

	bool CommandContext::GetIsPriority()
	{
		return m_isPriority;
	}

	void CommandContext::SetIsInternal()
	{
		m_isInternal = true;
	}

	bool CommandContext::GetIsInternal()
	{
		return m_isInternal;
	}

	ECmdContextState CommandContext::GetState()
	{
		return m_state;
	}

	VkCommandBuffer CommandContext::GetCmdBuffer()
	{
		return m_cmdBuffer;
	}

	void CommandContext::Invalidate()
	{
		m_cmdBuffer = VK_NULL_HANDLE;
	}

	Renderer* CommandContext::GetRenderer()
	{
		return m_renderer;
	}

	void CommandContext::CmdClearColorTargets(ClearDesc* clearColors, u32 targetCount)
	{
		ValidateRecordingState();
		PB_ASSERT(targetCount > 0);

		// Convert clear descs to their Vulkan equivalent and issue the command.
		CLib::Vector<VkClearAttachment, 16> clearAttachments;
		CLib::Vector<VkClearRect, 16> clearRects;

		for (u32 i = 0; i < targetCount; ++i)
		{
			PB_ASSERT(clearColors[i].m_region.w > 0 && clearColors[i].m_region.h > 0);

			clearAttachments[i].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			clearAttachments[i].clearValue.color = *reinterpret_cast<VkClearColorValue*>(&clearColors->m_color);
			clearAttachments[i].colorAttachment = clearColors[i].m_attachmentIndex;
			clearRects[i].baseArrayLayer = 0;
			clearRects[i].layerCount = 1;
			clearRects[i].rect = *reinterpret_cast<VkRect2D*>(&clearColors[i].m_region);
		}

		vkCmdClearAttachments(m_cmdBuffer, targetCount, clearAttachments.Data(), targetCount, clearRects.Data());
	}

	void CommandContext::CmdTransitionTexture(ITexture* texture, ETextureState oldState, ETextureState newState, const SubresourceRange& subResourceRange)
	{
		ValidateRecordingState();
		PB_ASSERT(texture);
		PB_ASSERT(newState < ETextureState::MAX);

		if (oldState == newState)
			return;

		Texture* internalTex = reinterpret_cast<Texture*>(texture);

		PB_ASSERT((internalTex->GetUsage() & newState) > 0);

		VkImageMemoryBarrier imageBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr };
		imageBarrier.image = internalTex->GetImage();
		imageBarrier.oldLayout = ConvertPBStateToImageLayout(oldState);
		imageBarrier.newLayout = ConvertPBStateToImageLayout(newState);
		imageBarrier.srcAccessMask = GetSrcAccessFlags(oldState);
		imageBarrier.dstAccessMask = GetDstAccessFlags(newState);
		imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		auto& barrierSubresourceRange = imageBarrier.subresourceRange;

		uint32_t mipCount = subResourceRange.AllMipLevels() ? internalTex->GetMipCount() : subResourceRange.m_mipCount;
		uint32_t arrayLayerCount = subResourceRange.AllArrayLayers() ? internalTex->GetArrayLayerCount() : subResourceRange.m_arrayCount;

		barrierSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		if (internalTex->HasDepthPlane())
		{
			PB_ASSERT(mipCount == 1);
			barrierSubresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		if (internalTex->HasStencilPlane())
		{
			PB_ASSERT(arrayLayerCount == 1);
			barrierSubresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}

		barrierSubresourceRange.baseMipLevel = subResourceRange.m_baseMip;
		barrierSubresourceRange.levelCount = mipCount;
		barrierSubresourceRange.baseArrayLayer = subResourceRange.m_firstArrayElement;
		barrierSubresourceRange.layerCount = arrayLayerCount;

		VkPipelineStageFlags srcStageMask = GetSrcStatePipelineFlags(oldState);
		VkPipelineStageFlags dstStageMask = GetDstStatePipelineFlags(newState);

		// TODO: Add functionality to batch transitions with the same stage masks. Batching will be optional is it may affect transition order.
		vkCmdPipelineBarrier(m_cmdBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
	}

	void CommandContext::CmdGraphicsBarrier()
	{
		PB_ASSERT(!m_activeRenderpass);
		vkCmdPipelineBarrier(m_cmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 0, nullptr);
	}

	void CommandContext::CmdComputeBarrier()
	{
		vkCmdPipelineBarrier(m_cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 0, nullptr);
	}

	void CommandContext::CmdDrawIndirectBarrier(const PB::IBufferObject** drawParamBuffers, u32 drawParamBufferCount)
	{
		CLib::Vector<VkBufferMemoryBarrier, 8> bufferMemBarriers(drawParamBufferCount);
		bufferMemBarriers.Reserve(drawParamBufferCount);
		for (u32 i = 0; i < drawParamBufferCount; ++i)
		{
			VkBufferMemoryBarrier& barrier = bufferMemBarriers.PushBack();
			const BufferObject* buf = reinterpret_cast<const BufferObject*>(drawParamBuffers[i]);

			barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			barrier.pNext = nullptr;
			barrier.buffer = buf->GetHandle();
			barrier.offset = 0;
			barrier.size = buf->GetSize();
			barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		}

		vkCmdPipelineBarrier(m_cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, nullptr, bufferMemBarriers.Count(), bufferMemBarriers.Data(), 0, nullptr);
	}

	void CommandContext::CmdBindPipeline(Pipeline pipeline)
	{
		ValidateRecordingState();

		PipelineData* pipelineData = reinterpret_cast<PipelineData*>(pipeline);
		m_activePipelineIsCompute = pipelineData->m_isCompute;
		m_activePipelineHasMeshShader = pipelineData->m_hasMeshShader;

		PB_ASSERT(m_reusable == false || (m_reusable == true && m_reusableForRenderPass == true) || (m_reusable == true && m_activePipelineIsCompute));
		VkPipelineBindPoint bindPoint = !m_activePipelineIsCompute ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_COMPUTE;

		vkCmdBindPipeline(m_cmdBuffer, bindPoint, pipelineData->m_pipeline);
		m_curPipelineLayout = pipelineData->m_layout;

		// Bind master descriptor set to the new pipeline.
		VkDescriptorSet masterDescSet = m_renderer->GetMasterSet();
		vkCmdBindDescriptorSets(m_cmdBuffer, bindPoint, m_curPipelineLayout, 0, 1, &masterDescSet, 0, nullptr);

		// Reset UBO binding state.
		m_bindingState->Clear();
	}

	void CommandContext::CmdSetViewport(PB::Rect viewRect, float minDepth, float maxDepth)
	{
		VkViewport viewport;
		viewport.x = static_cast<float>(viewRect.x);
		viewport.y = static_cast<float>(viewRect.y);
		viewport.width = static_cast<float>(viewRect.w);
		viewport.height = static_cast<float>(viewRect.h);
		viewport.minDepth = minDepth;
		viewport.maxDepth = maxDepth;

		vkCmdSetViewport(m_cmdBuffer, 0, 1, &viewport);
	}

	void CommandContext::CmdSetScissor(PB::Rect scissorRect)
	{
		vkCmdSetScissor(m_cmdBuffer, 0, 1, reinterpret_cast<VkRect2D*>(&scissorRect));
	}

	void CommandContext::CmdBindVertexBuffer(const IBufferObject* vertexBuffer, const IBufferObject* indexBuffer, EIndexType indexType)
	{
		PB_ASSERT(m_reusable == false || (m_reusable == true && m_reusableForRenderPass == true));
		ValidatePipelineState(false, false);
		CmdBindVertexBuffers(&vertexBuffer, 1, indexBuffer, indexType);
	}

	void CommandContext::CmdBindVertexBuffers(const IBufferObject** vertexBuffers, u32 vertexBufferCount, const IBufferObject* indexBuffer, EIndexType indexType)
	{
		PB_ASSERT(m_reusable == false || (m_reusable == true && m_reusableForRenderPass == true));
		ValidateRecordingState();
		ValidatePipelineState(false, false);

		const BufferObject** internalVBuffers = reinterpret_cast<const BufferObject**>(vertexBuffers);

		CLib::Vector<VkBuffer, 8> vertexBufferHandles(vertexBufferCount);
		CLib::Vector<VkDeviceSize, 8> vertexBufferOffsets(vertexBufferCount);
		for (u32 i = 0; i < vertexBufferCount; ++i)
		{
			if (internalVBuffers[i] == nullptr)
				continue;
			PB_ASSERT_MSG(internalVBuffers[i]->GetUsage() & EBufferUsage::VERTEX, "Provided vertex buffer cannot be used as a vertex buffer. Add the usage flag PB_BUFFER_USAGE_VERTEX to use it as a vertex buffer.");
			vertexBufferHandles.PushBack() = internalVBuffers[i]->GetHandle();
			vertexBufferOffsets.PushBack() = 0;
		}

		VkDeviceSize vertexOffset = 0;
		if(vertexBufferCount > 0)
			vkCmdBindVertexBuffers(m_cmdBuffer, 0, vertexBufferCount, vertexBufferHandles.Data(), vertexBufferOffsets.Data());

		if (indexBuffer != nullptr)
		{
			const BufferObject* internalIBuffer = reinterpret_cast<const BufferObject*>(indexBuffer);
			PB_ASSERT_MSG(internalIBuffer->GetUsage() & EBufferUsage::INDEX, "Provided index buffer cannot be used as a vertex buffer. Add the usage flag PB_BUFFER_USAGE_INDEX to use it as a index buffer.");

			VkBuffer indexBufferHandle = internalIBuffer->GetHandle();

			VkIndexType vkindexType;
			switch (indexType)
			{
			case EIndexType::PB_INDEX_TYPE_UINT16:
				vkindexType = VK_INDEX_TYPE_UINT16;
				break;
			case EIndexType::PB_INDEX_TYPE_UINT32:
				vkindexType = VK_INDEX_TYPE_UINT32;
				break;
			default:
				vkindexType = VK_INDEX_TYPE_UINT16;
				break;
			}

			vkCmdBindIndexBuffer(m_cmdBuffer, indexBufferHandle, 0, vkindexType);
		}
	}

	void CommandContext::CmdDraw(u32 vertexCount, u32 instanceCount)
	{
		PB_ASSERT(m_reusable == false || (m_reusable == true && m_reusableForRenderPass == true));
		ValidateRecordingState();
		ValidatePipelineState(false, false);
		vkCmdDraw(m_cmdBuffer, vertexCount, instanceCount, 0, 0);
	}

	void CommandContext::CmdDrawIndexed(u32 indexCount, u32 instanceCount)
	{
		PB_ASSERT(m_reusable == false || (m_reusable == true && m_reusableForRenderPass == true));
		ValidateRecordingState();
		ValidatePipelineState(false, false);
		vkCmdDrawIndexed(m_cmdBuffer, indexCount, 1, 0, 0, 0);
	}

	void CommandContext::CmdDrawIndexedIndirect(PB::IBufferObject* paramsBuffer, u32 offset)
	{
		PB_ASSERT(m_reusable == false || (m_reusable == true && m_reusableForRenderPass == true));
		ValidateRecordingState();
		ValidatePipelineState(false, false);
		vkCmdDrawIndexedIndirect(m_cmdBuffer, reinterpret_cast<BufferObject*>(paramsBuffer)->GetHandle(), offset, 1, sizeof(VkDrawIndexedIndirectCommand));
	}

	void CommandContext::CmdDrawIndexedIndirectCount(IBufferObject* paramsBuffer, u32 paramsOffset, IBufferObject* drawCountBuffer, u32 drawCountOffset, u32 maxDrawCount, u32 paramStride)
	{
		PB_ASSERT(m_reusable == false || (m_reusable == true && m_reusableForRenderPass == true));
		PB_ASSERT(m_device->GetVulkan12Features()->drawIndirectCount == VK_TRUE);
		ValidateRecordingState();
		ValidatePipelineState(false, false);
		vkCmdDrawIndexedIndirectCount(m_cmdBuffer, reinterpret_cast<BufferObject*>(paramsBuffer)->GetHandle(), paramsOffset, reinterpret_cast<BufferObject*>(drawCountBuffer)->GetHandle(), drawCountOffset, maxDrawCount, paramStride);
	}

	void CommandContext::CmdDrawMeshTasks(u32 threadGroupX, u32 threadGroupY, u32 threadGroupZ)
	{
		ValidateRecordingState();
		ValidatePipelineState(false, true);
		vkCmdDrawMeshTasksEXTFunc(m_cmdBuffer, threadGroupX, threadGroupY, threadGroupZ);
	}

	void CommandContext::CmdDrawMeshTasksIndirect(IBufferObject* paramsBuffer, u32 offset)
	{
		ValidateRecordingState();
		ValidatePipelineState(false, true);
		vkCmdDrawMeshTasksIndirectEXTFunc(m_cmdBuffer, reinterpret_cast<BufferObject*>(paramsBuffer)->GetHandle(), offset, 1, sizeof(VkDrawMeshTasksIndirectCommandEXT));
	}

	void CommandContext::CmdDrawMeshTasksIndirectCount(IBufferObject* paramsBuffer, u32 paramsOffset, IBufferObject* drawCountBuffer, u32 drawCountOffset, u32 maxDrawCount, u32 paramStride)
	{
		ValidateRecordingState();
		ValidatePipelineState(false, true);
		vkCmdDrawMeshTasksIndirectCountEXTFunc(m_cmdBuffer, reinterpret_cast<BufferObject*>(paramsBuffer)->GetHandle(), paramsOffset, reinterpret_cast<BufferObject*>(drawCountBuffer)->GetHandle(), drawCountOffset, maxDrawCount, paramStride);
	}

	void CommandContext::CmdDispatch(u32 threadGroupX, u32 threadGroupY, u32 threadGroupZ)
	{
		ValidateRecordingState();
		PB_ASSERT_MSG(m_activeRenderpass == false, "Dispatch commands cannot be issued during a render pass.");
		ValidatePipelineState(true, false);
		vkCmdDispatch(m_cmdBuffer, threadGroupX, threadGroupY, threadGroupZ);
	}

	void CommandContext::CmdCopyBufferToBuffer(IBufferObject* src, IBufferObject* dst, u32 srcOffset, u32 dstOffset, u32 size)
	{
		PB_ASSERT_MSG(m_activeRenderpass == false, "Copy commands cannot be issued during a render pass.");
		ValidateRecordingState();

		BufferObject* srcInternal = reinterpret_cast<BufferObject*>(src);
		BufferObject* dstInternal = reinterpret_cast<BufferObject*>(dst);
		
		VkBufferCopy copyRegion;
		copyRegion.size = size;
		copyRegion.srcOffset = srcOffset;
		copyRegion.dstOffset = dstOffset;
		vkCmdCopyBuffer(m_cmdBuffer, srcInternal->GetHandle(), dstInternal->GetHandle(), 1, &copyRegion);
	}

	void CommandContext::CmdCopyBufferToBuffer(IBufferObject* src, IBufferObject* dst, const CopyRegion* copyRegions, u32 regionCount)
	{
		PB_ASSERT_MSG(m_activeRenderpass == false, "Copy commands cannot be issued during a render pass.");
		ValidateRecordingState();

		BufferObject* srcInternal = reinterpret_cast<BufferObject*>(src);
		BufferObject* dstInternal = reinterpret_cast<BufferObject*>(dst);

		static_assert(sizeof(CopyRegion) == sizeof(VkBufferCopy));
		vkCmdCopyBuffer(m_cmdBuffer, srcInternal->GetHandle(), dstInternal->GetHandle(), regionCount, reinterpret_cast<const VkBufferCopy*>(copyRegions));
	}

	void CommandContext::CmdBindResources(const BindingLayout& layout)
	{
		ValidateRecordingState();
		ValidatePipelineState(m_activePipelineIsCompute, m_activePipelineHasMeshShader); // This command function supports both compute and graphics.

		static constexpr const u32 MaxBindings = MaxPushConstantBytes / sizeof(u32);

		PB_ASSERT_MSG(m_curPipelineLayout != VK_NULL_HANDLE, "A pipeline must be bound before shader resources are bound.");
		PB_ASSERT_MSG(layout.m_uniformBufferCount <= m_device->GetDescriptorIndexingProperties()->maxDescriptorSetUpdateAfterBindUniformBuffers - 1, "Attempting to bind too many buffers.");
		PB_ASSERT_MSG(!(layout.m_uniformBufferCount + layout.m_resourceCount > MaxBindings), "Maximum amount of bindings exceeded.");
		PB_ASSERT_MSG(layout.m_uniformBufferCount == 0 || layout.m_uniformBuffers != nullptr, "No uniform buffer data provided, yet there are more than zero uniform buffers to be bound.");
		PB_ASSERT_MSG(layout.m_resourceCount == 0 || layout.m_resourceViews != nullptr, "No resource view data provided, yet there are more than zero resource views to be bound.");

		// TODO: Currently, with the max push constant block size of 128 bytes, we can only bind 32 resources across all shader stages using 32-bit indices. Consider using 16-bit indices for a max of 64.
		// It is also possible to use 8-bit indices for uniform buffers, since their maximum array size is very small, but it would require a dynamic maximum binding count.
		u32 offset = 0;
		u32 dynamicIndices[MaxBindings];

		CLib::Vector<VkDescriptorBufferInfo, 3> uboBufferInfos;
		bool redundantUBOBinding = true;
		for (u16 i = 0; i < layout.m_uniformBufferCount; ++i, ++offset)
		{
			UBOViewData* viewData = reinterpret_cast<UBOViewData*>(layout.m_uniformBuffers[i]);

			VkDescriptorBufferInfo& uboInfo = uboBufferInfos.PushBack();
			uboInfo.buffer = viewData->m_buffer;
			uboInfo.offset = viewData->m_offset;
			uboInfo.range = viewData->m_size;

			if (i < m_bindingState->m_boundUBOs.Count())
			{
				VkDescriptorBufferInfo& boundBuffer = m_bindingState->m_boundUBOs[i];
				redundantUBOBinding &= uboInfo.buffer == boundBuffer.buffer;
				redundantUBOBinding &= uboInfo.offset == boundBuffer.offset;
				redundantUBOBinding &= uboInfo.range == boundBuffer.range;
			}
			else
				redundantUBOBinding = false;

			dynamicIndices[offset] = i;
		}

		if (!redundantUBOBinding)
		{
			VkDescriptorSet uboSet = m_renderer->GetUBOSet();

			// Update binding state.
			m_bindingState->m_boundUBOs = uboBufferInfos;

			// Update UBO descriptors.
			VkWriteDescriptorSet uboWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr };
			uboWrite.descriptorCount = uboBufferInfos.Count();
			uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			uboWrite.dstArrayElement = 0;
			uboWrite.dstBinding = 0;
			uboWrite.dstSet = uboSet;
			uboWrite.pBufferInfo = uboBufferInfos.Data();
			uboWrite.pImageInfo = nullptr;
			uboWrite.pTexelBufferView = nullptr;

			vkUpdateDescriptorSets(m_device->GetHandle(), 1, &uboWrite, 0, nullptr);
			vkCmdBindDescriptorSets(m_cmdBuffer, !m_activePipelineIsCompute ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_COMPUTE, m_curPipelineLayout, 1, 1, &uboSet, 0, nullptr);

			if (!m_reusable)
				m_renderer->ReturnUBOSet(uboSet);
			else
				m_bindingState->m_uboSets.PushBack(uboSet);
		}

		// ResourceViews are actually just descriptor indices, so we can just copy these.
		static_assert(sizeof(ResourceView) == sizeof(u32), "ResourceViews need to be the same type as the descriptor index type, as they should be identical in size and format for direct copy.");

		memcpy(&dynamicIndices[offset], layout.m_resourceViews, layout.m_resourceCount * sizeof(u32));
		offset += layout.m_resourceCount;

		VkShaderStageFlags stageFlags;
		if (m_activePipelineIsCompute == true)
			stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		else if (m_activePipelineHasMeshShader == true)
			stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT;
		else
			stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

		// Dynamic indices are assigned, so they can be pushed.
		vkCmdPushConstants(m_cmdBuffer, m_curPipelineLayout, stageFlags, 0, offset * sizeof(u32), dynamicIndices);
	}

	void CommandContext::CmdBindResources(const IBindingCache* layout)
	{
		CmdBindResources(layout->GetLayout());
	}

	void CommandContext::CmdCopyTextureToTexture(PB::ITexture* src, PB::ITexture* dst)
	{
		PB_ASSERT_MSG(!m_activeRenderpass, "Copy commands cannot be issued during a render pass.");
		ValidateRecordingState();

		PB::Texture* srcInternal = reinterpret_cast<PB::Texture*>(src);
		PB::Texture* dstInternal = reinterpret_cast<PB::Texture*>(dst);

		PB_ASSERT_MSG(srcInternal->GetUsage() & ETextureState::COPY_SRC && dstInternal->GetUsage() & ETextureState::COPY_DST, "Src and Dst image layouts are incorrect.");
		PB_ASSERT(dstInternal->GetExtent().width >= srcInternal->GetExtent().width && dstInternal->GetExtent().height >= srcInternal->GetExtent().height);
		PB_ASSERT((srcInternal->HasDepthPlane() && dstInternal->HasDepthPlane()) || (!srcInternal->HasDepthPlane() && !dstInternal->HasDepthPlane()));
		PB_ASSERT_MSG(srcInternal->GetMipCount() == dstInternal->GetMipCount(), "Src and Dst mip count must be equal.");
		PB_ASSERT_MSG(srcInternal->GetArrayLayerCount() == dstInternal->GetArrayLayerCount(), "Src and Dst array layer count must be equal.");

		VkImageCopy region;
		region.dstOffset = { 0, 0, 0 };
		region.srcOffset = { 0, 0, 0 };
		region.extent = srcInternal->GetExtent(); // TODO: Make sure dst's extent is large enough to accomodate src's extent. If not, it should use the min extent.
		
		auto& srcSubresource = region.srcSubresource;
		srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		if (srcInternal->HasDepthPlane())
		{
			PB_ASSERT(srcInternal->GetMipCount() == 1);
			srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		if (srcInternal->HasStencilPlane())
		{
			PB_ASSERT(srcInternal->GetMipCount() == 1);
			srcSubresource.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}

		srcSubresource.baseArrayLayer = 0;
		srcSubresource.layerCount = 1;
		srcSubresource.mipLevel = 0;
		
		region.dstSubresource = srcSubresource;
		vkCmdCopyImage(m_cmdBuffer, srcInternal->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstInternal->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	}

	void CommandContext::CmdCopyTextureSubresource(ITexture* src, ITexture* dst, u16 srcMipLevel, u16 srcArrayLayer, u16 dstMipLevel, u16 dstArrayLayer)
	{
		PB_ASSERT_MSG(!m_activeRenderpass, "Copy commands cannot be issued during a render pass.");
		ValidateRecordingState();

		PB::Texture* srcInternal = reinterpret_cast<PB::Texture*>(src);
		PB::Texture* dstInternal = reinterpret_cast<PB::Texture*>(dst);

		PB_ASSERT_MSG(srcInternal->GetUsage() & ETextureState::COPY_SRC && dstInternal->GetUsage() & ETextureState::COPY_DST, "Src and Dst image layouts are incorrect.");
		PB_ASSERT((srcInternal->HasDepthPlane() && dstInternal->HasDepthPlane()) || (!srcInternal->HasDepthPlane() && !dstInternal->HasDepthPlane()));

		VkExtent3D srcExtent = srcInternal->GetExtent();
		srcExtent.width >>= srcMipLevel;
		srcExtent.height >>= srcMipLevel;

		VkExtent3D dstExtent = dstInternal->GetExtent();
		dstExtent.width >>= dstMipLevel;
		dstExtent.height >>= dstMipLevel;

		PB_ASSERT_MSG(srcExtent.width == dstExtent.width && srcExtent.height == dstExtent.height, "Width and height of Src and Dst mip level are not the same.");

		VkImageCopy region;
		region.dstOffset = { 0, 0, 0 };
		region.srcOffset = { 0, 0, 0 };
		region.extent = dstExtent;

		auto& srcSubresource = region.srcSubresource;
		srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		if (srcInternal->HasDepthPlane())
		{
			PB_ASSERT(srcInternal->GetMipCount() == 1);
			srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		if (srcInternal->HasStencilPlane())
		{
			PB_ASSERT(srcInternal->GetMipCount() == 1);
			srcSubresource.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}

		srcSubresource.baseArrayLayer = srcArrayLayer;
		srcSubresource.layerCount = 1;
		srcSubresource.mipLevel = srcMipLevel;

		auto& dstSubresource = region.dstSubresource;
		dstSubresource.aspectMask = srcSubresource.aspectMask;
		dstSubresource.baseArrayLayer = dstArrayLayer;
		dstSubresource.layerCount = 1;
		dstSubresource.mipLevel = dstMipLevel;

		vkCmdCopyImage(m_cmdBuffer, srcInternal->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstInternal->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	}

	void CommandContext::CmdCopyTextureToBuffer(ITexture* src, PB::IBufferObject* dst, const PB::SubresourceRange& subresources, TextureDataDesc* outSubresourceData)
	{
		PB_ASSERT(src && dst);
		PB_ASSERT(subresources.m_mipCount > 0 && subresources.m_baseMip < subresources.m_mipCount);
		PB_ASSERT(subresources.m_arrayCount > 0 && subresources.m_firstArrayElement < subresources.m_arrayCount);
		ValidateRecordingState();

		Texture* srcInternal = reinterpret_cast<Texture*>(src);
		BufferObject* dstInternal = reinterpret_cast<BufferObject*>(dst);

		PB_ASSERT(subresources.m_mipCount <= srcInternal->GetMipCount());
		PB_ASSERT(subresources.m_arrayCount <= srcInternal->GetArrayLayerCount());

		CLib::Vector<VkBufferImageCopy, 16> copyRegions{};

		VkDevice device = m_device->GetHandle();
		VkImage image = srcInternal->GetImage();
		VkImageSubresource subresource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
		VkSubresourceLayout subresourceLayout;

		for (uint32_t mip = subresources.m_baseMip; mip < subresources.m_mipCount; ++mip)
		{
			VkExtent3D extent = srcInternal->GetExtent();
			extent.width <<= mip;
			extent.height <<= mip;

			for (uint32_t layer = subresources.m_firstArrayElement; layer < subresources.m_arrayCount; ++layer)
			{
				subresource.mipLevel = mip;
				subresource.arrayLayer = layer;
				vkGetImageSubresourceLayout(device, image, &subresource, &subresourceLayout);

				VkBufferImageCopy& region = copyRegions.PushBack();
				region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.imageSubresource.baseArrayLayer = subresources.m_firstArrayElement;
				region.imageSubresource.layerCount = 1;
				region.imageSubresource.mipLevel = mip;
				region.imageOffset = { 0, 0, 0 };
				region.imageExtent = srcInternal->GetExtent();

				region.bufferOffset = subresourceLayout.offset;
				region.bufferRowLength = extent.width;
				region.bufferImageHeight = extent.height;

				if (outSubresourceData != nullptr)
				{
					TextureDataDesc& outDataDesc = outSubresourceData[(layer * subresources.m_mipCount) + mip];
					outDataDesc.m_data = (u8*)subresourceLayout.offset;
					outDataDesc.m_size = subresourceLayout.size;
					outDataDesc.m_mipLevel = mip;
					outDataDesc.m_arrayLayer = layer;
					outDataDesc.m_next = nullptr;
				}
			}
		}

		vkCmdCopyImageToBuffer(m_cmdBuffer, srcInternal->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstInternal->GetHandle(), copyRegions.Count(), copyRegions.Data());
	}

	void CommandContext::CmdExecuteList(const PB::ICommandList* list)
	{
		ValidateRecordingState();

		VkCommandBuffer cmdBuf = reinterpret_cast<const CommandList*>(list)->GetCommandBuffer();
		vkCmdExecuteCommands(m_cmdBuffer, 1, &cmdBuf);
	}

	void CommandContext::ValidateRecordingState()
	{
		PB_ASSERT_MSG(m_state == ECmdContextState::RECORDING, "Command context must be recording in-order to issue commands.");
		PB_ASSERT(m_cmdBuffer != VK_NULL_HANDLE);
	}

	inline void CommandContext::ValidatePipelineState(bool computeFunction, bool meshFunction)
	{
		PB_ASSERT_MSG(m_curPipelineLayout != VK_NULL_HANDLE, "No pipeline is bound where one is expected.");
		PB_ASSERT(m_activePipelineIsCompute == computeFunction);
		PB_ASSERT_MSG(m_activePipelineHasMeshShader == meshFunction,
			meshFunction ? "Bound pipeline is expecting a Mesh Shader stage but does not contain one." : "Bound pipeline is not expecting a Mesh Shader stage but is provided with one.");
	}

	CommandContext* ThreadCommandContext::Get(Renderer* renderer)
	{
		if (m_ptr == nullptr)
		{
			m_renderer = renderer;
			m_ptr = reinterpret_cast<CommandContext*>(CreateCommandContext(reinterpret_cast<IRenderer*>(m_renderer)));

			CommandContextDesc desc;
			desc.m_flags = ECommandContextFlags::PRIORITY;
			desc.m_usage = ECommandContextUsage::GRAPHICS;
			desc.m_renderer = reinterpret_cast<IRenderer*>(renderer);

			// Initialize and flag as internal.
			m_ptr->Init(desc);
			m_ptr->SetIsInternal();
		}

		if (m_ptr->GetState() != PB::ECmdContextState::RECORDING)
		{
			m_ptr->Begin();
			m_renderer->AddThreadCommandContext(this);
		}

		return m_ptr;
	}
}