#include "CommandContext.h"
#include "ParaBlitDebug.h"
#include "Renderer.h"
#include "FixedArray.h"

namespace PB 
{
	CommandContext::CommandContext()
	{

	}

	CommandContext::~CommandContext()
	{

	}

	void CommandContext::Init(CommandContextDesc& desc)
	{
		PB_ASSERT(desc.m_renderer);
		PB_ASSERT(desc.m_usage <= PB_COMMAND_CONTEXT_USAGE_MAX);

		m_renderer = reinterpret_cast<Renderer*>(desc.m_renderer);
		m_usage = desc.m_usage;
		m_priority = desc.m_flags & PB_COMMAND_CONTEXT_PRIORITY > 0;
	}

	void CommandContext::Begin()
	{
		auto device = m_renderer->GetDevice();

		// Obtain command buffer, a new one is allocated if there are no free command buffers.
		if (m_cmdBuffer == VK_NULL_HANDLE)
		{
			m_cmdBuffer = m_renderer->AllocateCommandBuffer();
			PB_COMMAND_CONTEXT_LOG("Allocated command buffer [%X] for command context [%X].", m_cmdBuffer, this);
		}

		// TODO: Create render pass creation and retreival API in another class. And create commands to begin and end a render pass, and advance the subpass.
		// TODO: Provide framebuffer and subpass index for the renderpass this context is used for (if applicable).
		VkCommandBufferInheritanceInfo inheritInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, nullptr };
		inheritInfo.framebuffer = VK_NULL_HANDLE;
		inheritInfo.subpass = VK_NULL_HANDLE;
		inheritInfo.occlusionQueryEnable = VK_FALSE;
		inheritInfo.queryFlags = 0;

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

		PB_ERROR_CHECK(vkEndCommandBuffer(m_cmdBuffer), "Failed to end command context recording.");

		PB_COMMAND_CONTEXT_LOG("Returned recorded command buffer [%X] from command context [%X].", m_cmdBuffer, this);
		m_renderer->ReturnCommandBuffer(m_cmdBuffer); // Give the command buffer back to the renderer for submission at the end of the frame.
		m_state = PB_COMMAND_CONTEXT_STATE_OPEN;
	}

	ECmdContextState CommandContext::GetState()
	{
		return m_state;
	}

	VkCommandBuffer CommandContext::GetCmdBuffer()
	{
		return m_cmdBuffer;
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

	void CommandContext::ValidateRecordingState()
	{
		PB_ASSERT(m_state == PB_COMMAND_CONTEXT_STATE_RECORDING, "Command context must be recording in-order to issue commands.");
		PB_ASSERT(m_cmdBuffer != VK_NULL_HANDLE);
	}
}