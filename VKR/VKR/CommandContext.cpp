#include "CommandContext.h"
#include "FramebufferCache.h"
#include "ParaBlitDebug.h"
#include "PBUtil.h"
#include "Renderer.h"
#include "CLib/Vector.h"
#include "Texture.h"
#include "BufferObject.h"

namespace PB 
{
	CommandContext::CommandContext()
	{
		m_isPriority = false;
		m_isInternal = false;
		m_activeRenderpass = false;
	}

	CommandContext::~CommandContext()
	{
		if (m_cmdBuffer != VK_NULL_HANDLE)
		{
			if (m_state == PB_COMMAND_CONTEXT_STATE_RECORDING)
			{
				End();
				Return();
			}
			else if (m_state == PB_COMMAND_CONTEXT_STATE_PENDING_SUBMISSION)
			{
				Return();
			}
			else
			{
				m_renderer->ReturnOpenBuffer(*this);
				m_state = PB_COMMAND_CONTEXT_STATE_OPEN;
			}

			m_cmdBuffer = VK_NULL_HANDLE;
		}
	}

	void CommandContext::Init(CommandContextDesc& desc)
	{
		PB_ASSERT(desc.m_renderer);
		PB_ASSERT(desc.m_usage <= PB_COMMAND_CONTEXT_USAGE_MAX);

		m_renderer = reinterpret_cast<Renderer*>(desc.m_renderer);
		m_device = m_renderer->GetDevice();
		m_usage = desc.m_usage;
		m_isPriority = (desc.m_flags & PB_COMMAND_CONTEXT_PRIORITY) > 0;
		m_uboBindingDirty = false;
	}

	void CommandContext::Begin()
	{
		// Obtain command buffer, a new one is allocated if there are no free command buffers.
		if (m_cmdBuffer == VK_NULL_HANDLE)
		{
			m_cmdBuffer = m_renderer->AllocateCommandBuffer();
			PB_COMMAND_CONTEXT_LOG("Obtained command buffer [%X] for command context [%X].", m_cmdBuffer, this);
		}

		VkCommandBufferInheritanceInfo inheritInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, nullptr };
		inheritInfo.framebuffer = VK_NULL_HANDLE;
		inheritInfo.occlusionQueryEnable = VK_FALSE;
		inheritInfo.renderPass = VK_NULL_HANDLE;
		inheritInfo.subpass = 0;

		VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr };
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		beginInfo.pInheritanceInfo = &inheritInfo;

		VkResult res = vkBeginCommandBuffer(m_cmdBuffer, &beginInfo);
		if (res != VK_SUCCESS)
		{
			PB_ASSERT_MSG(false, "Failed to begin command buffer recording.");
			m_state = PB_COMMAND_CONTEXT_STATE_OPEN;
		}

		m_state = PB_COMMAND_CONTEXT_STATE_RECORDING;
	}

	void CommandContext::End()
	{
		PB_ASSERT_MSG(m_state == PB_COMMAND_CONTEXT_STATE_RECORDING, "Cannot end recording of a command context that is not currently recording.");
		if (m_state != PB_COMMAND_CONTEXT_STATE_RECORDING)
			return;

		if (m_activeRenderpass)
		{
			m_activeRenderpass = false;
			vkCmdEndRenderPass(m_cmdBuffer);
		}
		PB_ERROR_CHECK(vkEndCommandBuffer(m_cmdBuffer));
		PB_BREAK_ON_ERROR;

		// Unmap current DRI buffer.
		if (m_currentDRIBuffer)
		{
			m_currentDRIBuffer->m_stagingBuffer.Unmap();
			m_currentDRIBuffer = nullptr;
			m_dynamicResourceIndices = nullptr;
		}

		m_currentUBOSet = VK_NULL_HANDLE;
		m_currentUBOIndex = 0;

		m_state = PB_COMMAND_CONTEXT_STATE_PENDING_SUBMISSION;
	}

	void CommandContext::Return()
	{
		PB_ASSERT_MSG(m_state == PB_COMMAND_CONTEXT_STATE_PENDING_SUBMISSION, "Cannot submit command context that is not yet recorded or currently recording.");

		PB_COMMAND_CONTEXT_LOG("Returned recorded command buffer [%X] from command context [%X].", m_cmdBuffer, this);
		m_renderer->ReturnCommandBuffer(*this); // Give the command buffer back to the renderer for submission at the end of the frame.
		m_state = PB_COMMAND_CONTEXT_STATE_OPEN;
	}

	void CommandContext::CmdBeginRenderPass(RenderPass renderpass, u32 width, u32 height, TextureView* attachmentViews, u32 viewCount, Float4* clearColors, u32 clearColorCount)
	{
		ValidateRecordingState();
		PB_ASSERT(renderpass);
		PB_ASSERT(attachmentViews);
		PB_ASSERT(viewCount > 0);
		if (m_activeRenderpass)
			vkCmdEndRenderPass(m_cmdBuffer);

		VkRenderPass pass = reinterpret_cast<VkRenderPass>(renderpass);
		PB_ASSERT(pass);

		FramebufferDesc fbDesc;
		fbDesc.m_attachmentCount = static_cast<u32>(viewCount);
		fbDesc.m_attachmentViews = attachmentViews;
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

		vkCmdBeginRenderPass(m_cmdBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
		m_activeRenderpass = true;
	}

	void CommandContext::CmdEndRenderPass()
	{
		if (m_activeRenderpass)
		{
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

	void CommandContext::CmdTransitionTexture(ITexture* texture, ETextureState newState, const SubresourceRange& subResourceRange)
	{
		ValidateRecordingState();
		PB_ASSERT(texture);
		PB_ASSERT(newState < PB_TEXTURE_STATE_MAX);

		Texture* internalTex = reinterpret_cast<Texture*>(texture);

		auto oldState = internalTex->GetState();

		if (oldState == newState)
			return;

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

		barrierSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		if (internalTex->HasDepthPlane())
			barrierSubresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		if (internalTex->HasStencilPlane())
			barrierSubresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

		barrierSubresourceRange.baseMipLevel = subResourceRange.m_baseMip;
		barrierSubresourceRange.levelCount = subResourceRange.m_mipCount;
		barrierSubresourceRange.baseArrayLayer = subResourceRange.m_firstArrayElement;
		barrierSubresourceRange.layerCount = subResourceRange.m_arrayCount;

		VkPipelineStageFlags srcStageMask = GetSrcStatePipelineFlags(oldState);
		VkPipelineStageFlags dstStageMask = GetDstStatePipelineFlags(newState);

		// TODO: Add functionality to batch transitions with the same stage masks. Batching will be optional is it may affect transition order.
		vkCmdPipelineBarrier(m_cmdBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
		internalTex->SetState(newState);
	}

	void CommandContext::CmdBindPipeline(Pipeline pipeline)
	{
		PipelineData* pipelineData = reinterpret_cast<PipelineData*>(pipeline);
		vkCmdBindPipeline(m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData->m_pipeline);
		m_curPipelineLayout = pipelineData->m_layout;

		// Reset these since any descriptor bindings are invalidated when a pipeline is bound.
		m_currentDRIBuffer = nullptr;
		m_dynamicResourceIndices = nullptr;

		// Bind master descriptor set to the new pipeline.
		VkDescriptorSet masterDescSet = m_renderer->GetMasterSet();
		vkCmdBindDescriptorSets(m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_curPipelineLayout, 1, 1, &masterDescSet, 0, nullptr);

		if(m_currentUBOSet)
			vkCmdBindDescriptorSets(m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_curPipelineLayout, 2, 1, &m_currentUBOSet, 0, nullptr);
	}

	void CommandContext::CmdBindVertexBuffer(const IBufferObject* vertexBuffer, const IBufferObject* indexBuffer, EIndexType indexType)
	{
		const BufferObject* internalVBuffer = reinterpret_cast<const BufferObject*>(vertexBuffer);

		PB_ASSERT(vertexBuffer);
		PB_ASSERT_MSG(internalVBuffer->GetUsage() & PB_BUFFER_USAGE_VERTEX, "Provided vertex buffer cannot be used as a vertex buffer. Add the usage flag PB_BUFFER_USAGE_VERTEX to use it as a vertex buffer.");

		VkBuffer vertexBufferHandle = internalVBuffer->GetHandle();
		VkDeviceSize vertexOffset = 0;
		vkCmdBindVertexBuffers(m_cmdBuffer, 0, 1, &vertexBufferHandle, &vertexOffset);
		
		if (indexBuffer != nullptr)
		{
			const BufferObject* internalIBuffer = reinterpret_cast<const BufferObject*>(indexBuffer);
			PB_ASSERT_MSG(internalIBuffer->GetUsage() & PB_BUFFER_USAGE_INDEX, "Provided index buffer cannot be used as a vertex buffer. Add the usage flag PB_BUFFER_USAGE_INDEX to use it as a index buffer.");

			VkBuffer indexBufferHandle = internalIBuffer->GetHandle();

			VkIndexType vkindexType;
			switch (indexType)
			{
			case PB_INDEX_TYPE_UINT16:
				vkindexType = VK_INDEX_TYPE_UINT16;
				break;
			case PB_INDEX_TYPE_UINT32:
				vkindexType = VK_INDEX_TYPE_UINT32;
				break;
			default:
				vkindexType = VK_INDEX_TYPE_UINT16;
				break;
			}

			vkCmdBindIndexBuffer(m_cmdBuffer, indexBufferHandle, 0, vkindexType);
		}
	}

	void CommandContext::PreDraw()
	{
		if (m_uboBindingDirty == true)
		{
			m_uboBindingDirty = false;
			vkCmdBindDescriptorSets(m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_curPipelineLayout, 2, 1, &m_currentUBOSet, 0, nullptr);
		}
	}

	void CommandContext::CmdDraw(u32 vertexCount, u32 instanceCount)
	{
		PreDraw();

		vkCmdDraw(m_cmdBuffer, vertexCount, instanceCount, 0, 0);
	}

	void CommandContext::CmdDrawIndexed(u32 indexCount, u32 instanceCount)
	{
		PreDraw();

		vkCmdDrawIndexed(m_cmdBuffer, indexCount, 1, 0, 0, 0);
	}

	void CommandContext::CmdCopyBufferToBuffer(IBufferObject* src, IBufferObject* dst, u32 srcOffset, u32 dstOffset, u32 size)
	{
		BufferObject* srcInternal = reinterpret_cast<BufferObject*>(src);
		BufferObject* dstInternal = reinterpret_cast<BufferObject*>(dst);
		
		VkBufferCopy copyRegion;
		copyRegion.size = size;
		copyRegion.srcOffset = static_cast<VkDeviceSize>(srcInternal->GetStart()) + srcOffset;
		copyRegion.dstOffset = static_cast<VkDeviceSize>(dstInternal->GetStart()) + dstOffset;
		vkCmdCopyBuffer(m_cmdBuffer, srcInternal->GetHandle(), dstInternal->GetHandle(), 1, &copyRegion);
	}

	void CommandContext::CmdBindResources(const BindingLayout& layout)
	{
		PB_ASSERT_MSG(m_curPipelineLayout != VK_NULL_HANDLE, "A pipeline must be bound before shader resources are bound.");
		PB_ASSERT_MSG(layout.m_bufferCount <= m_device->GetDescriptorIndexingProperties()->maxDescriptorSetUpdateAfterBindUniformBuffers - 1, "Attempting to bind too many buffers.");

		if (m_currentDRIBuffer == nullptr)
		{
			m_currentDRIBuffer = m_renderer->GetDRIBuffer();
			PB_ASSERT(m_currentDRIBuffer);
			m_dynamicResourceIndices = reinterpret_cast<int*>(m_currentDRIBuffer->m_stagingBuffer.Map(0, Renderer::DRIBufferSize));
		}

		if (layout.m_bindingLocation == PB_BINDING_LAYOUT_LOCATION_DEFAULT)
		{
			DRIBuffer* indexBuffer = m_currentDRIBuffer;

			if (indexBuffer->m_currentOffset + layout.m_bufferCount + layout.m_textureCount + layout.m_samplerCount > Renderer::DRIBufferSize)
			{
				// Out of space in the current DRI buffer, get a new one.
				m_currentDRIBuffer->m_stagingBuffer.Unmap();
				m_currentDRIBuffer->m_isFull = true;

				m_currentDRIBuffer = m_renderer->GetDRIBuffer();
				m_dynamicResourceIndices = reinterpret_cast<int*>(m_currentDRIBuffer->m_stagingBuffer.Map(0, Renderer::DRIBufferSize));
			}
			u32& offset = indexBuffer->m_currentOffset;

			// Bind the DRI buffer with the new offset.
			vkCmdBindDescriptorSets(m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_curPipelineLayout, 0, 1, &m_currentDRIBuffer->m_descSet, 1, &m_currentDRIBuffer->m_currentOffset);

			// TODO: Remove indirection of getting maxDescriptorSetUpdateAfterBindUniformBuffers.
			// Get a new UBO set if the current one doesn't have enough room for the table's UBOs.
			if (m_currentUBOIndex + layout.m_bufferCount > m_device->GetDescriptorIndexingProperties()->maxDescriptorSetUpdateAfterBindUniformBuffers - 2)
			{
				m_currentUBOSet = m_renderer->GetUBOSet();
				m_currentUBOIndex = 0;
			}

			CLib::Vector<VkDescriptorBufferInfo, 3> uboBufferInfos;
			for (u16 i = 0; i < layout.m_bufferCount; ++i, ++offset)
			{
				BufferViewData* viewData = reinterpret_cast<BufferViewData*>(layout.m_buffers[i]);

				VkDescriptorBufferInfo& uboInfo = uboBufferInfos.PushBack();
				uboInfo.buffer = viewData->m_buffer;
				uboInfo.offset = viewData->m_offset;
				uboInfo.range = viewData->m_size;

				m_dynamicResourceIndices[offset] = m_currentUBOIndex + i;
			}

			// TODO: Analyse if memcpy here is actually any faster than simply looping and setting the indices.
			// Texture and sampler views are actually just descriptor indices, so we can just copy these.
			memcpy(&m_dynamicResourceIndices[offset], layout.m_textures, sizeof(TextureView) * layout.m_textureCount);
			offset += layout.m_textureCount;
			memcpy(&m_dynamicResourceIndices[offset], layout.m_samplers, sizeof(Sampler) * layout.m_samplerCount);
			offset += layout.m_samplerCount;

			if (uboBufferInfos.Count() != 0)
			{
				if (m_currentUBOSet == VK_NULL_HANDLE)
				{
					m_currentUBOSet = m_renderer->GetUBOSet();
				}

				// Update UBO descriptors.
				VkWriteDescriptorSet uboWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr };
				uboWrite.descriptorCount = uboBufferInfos.Count();
				uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				uboWrite.dstArrayElement = m_currentUBOIndex;
				uboWrite.dstBinding = 0;
				uboWrite.dstSet = m_currentUBOSet;
				uboWrite.pBufferInfo = uboBufferInfos.Data();
				uboWrite.pImageInfo = nullptr;
				uboWrite.pTexelBufferView = nullptr;

				vkUpdateDescriptorSets(m_device->GetHandle(), 1, &uboWrite, 0, nullptr);
				m_uboBindingDirty = true;
				m_currentUBOIndex += uboBufferInfos.Count();
			}
		}
		else
		{
			PB_NOT_IMPLEMENTED;
		}
	}

	void CommandContext::ValidateRecordingState()
	{
		PB_ASSERT_MSG(m_state == PB_COMMAND_CONTEXT_STATE_RECORDING, "Command context must be recording in-order to issue commands.");
		PB_ASSERT(m_cmdBuffer != VK_NULL_HANDLE);
	}
}