#include "CommandContext.h"
#include "FramebufferCache.h"
#include "ParaBlitDebug.h"
#include "PBUtil.h"
#include "Renderer.h"
#include "FixedArray.h"
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
		m_usage = desc.m_usage;
		m_isPriority = (desc.m_flags & PB_COMMAND_CONTEXT_PRIORITY) > 0;
	}

	void CommandContext::Begin()
	{
		auto device = m_renderer->GetDevice();

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
			PB_ASSERT(false, "Failed to begin command buffer recording.");
			m_state = PB_COMMAND_CONTEXT_STATE_OPEN;
		}

		m_state = PB_COMMAND_CONTEXT_STATE_RECORDING;
	}

	void CommandContext::End()
	{
		PB_ASSERT(m_state == PB_COMMAND_CONTEXT_STATE_RECORDING, "Cannot end recording of a command context that is not currently recording.");
		if (m_state != PB_COMMAND_CONTEXT_STATE_RECORDING)
			return;

		if (m_activeRenderpass)
		{
			m_activeRenderpass = false;
			vkCmdEndRenderPass(m_cmdBuffer);
		}
		PB_ERROR_CHECK(vkEndCommandBuffer(m_cmdBuffer), "Failed to end command context recording.");
		m_state = PB_COMMAND_CONTEXT_STATE_PENDING_SUBMISSION;
	}

	void CommandContext::Return()
	{
		PB_ASSERT(m_state == PB_COMMAND_CONTEXT_STATE_PENDING_SUBMISSION, "Cannot submit command context that is not yet recorded or currently recording.");

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
		FixedArray<VkClearAttachment, 16> clearAttachments;
		FixedArray<VkClearRect, 16> clearRects;

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
		VkPipeline vulkanPipeline = reinterpret_cast<VkPipeline>(pipeline);
		vkCmdBindPipeline(m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanPipeline);
	}

	void CommandContext::CmdDraw(u32 vertexCount)
	{
		vkCmdDraw(m_cmdBuffer, vertexCount, 1, 0, 0);
	}

	void CommandContext::CmdCopyBufferToBuffer(IBufferObject* src, IBufferObject* dst, u32 srcOffset, u32 dstOffset, u32 size)
	{
		BufferObject* srcInternal = reinterpret_cast<BufferObject*>(src);
		BufferObject* dstInternal = reinterpret_cast<BufferObject*>(dst);
		
		VkBufferCopy copyRegion;
		copyRegion.size = size;
		copyRegion.srcOffset = srcInternal->GetStart() + srcOffset;
		copyRegion.dstOffset = dstInternal->GetStart() + dstOffset;
		vkCmdCopyBuffer(m_cmdBuffer, srcInternal->GetHandle(), dstInternal->GetHandle(), 1, &copyRegion);
	}

	void CommandContext::ValidateRecordingState()
	{
		PB_ASSERT(m_state == PB_COMMAND_CONTEXT_STATE_RECORDING, "Command context must be recording in-order to issue commands.");
		PB_ASSERT(m_cmdBuffer != VK_NULL_HANDLE);
	}
}