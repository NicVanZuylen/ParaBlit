#pragma once
#include "ICommandContext.h"
#include "ParaBlitApi.h"
#include "ParaBlitDefs.h"
#include "StagingBufferAllocator.h"

#include <unordered_map>

#define PB_LOG_COMMAND_CONTEXT_STATE 0

#if PB_LOG_COMMAND_CONTEXT_STATE
#define PB_COMMAND_CONTEXT_LOG PB_LOG_FORMAT
#else
#define PB_COMMAND_CONTEXT_LOG
#endif

namespace PB 
{
	class Renderer;
	class IBufferObject;
	
	struct DRIBuffer;

	class CommandContext : public ICommandContext
	{
	public:
		PARABLIT_API CommandContext();

		PARABLIT_API ~CommandContext();

		// Interface functions. Descriptions can be found in ICommandContext.h

		PARABLIT_API void Init(CommandContextDesc& desc) override;
		PARABLIT_API void Begin() override;
		PARABLIT_API void End() override;
		PARABLIT_API void Return() override;
		void CmdBeginRenderPass(RenderPass renderPass, u32 width, u32 height, TextureView* attachmentViews, u32 viewCount, Float4* clearColors, u32 clearColorCount) override;
		void CmdBeginRenderPass(RenderPass renderPass, u32 width, u32 height, Framebuffer frameBuffer, Float4* clearColors, u32 clearColorCount) override;
		PARABLIT_API void CmdEndRenderPass() override;
		PARABLIT_API void CmdClearColorTargets(ClearDesc* clearColors, u32 targetCount) override;
		PARABLIT_API void CmdTransitionTexture(ITexture* texture, ETextureState newState, const SubresourceRange& subResourceRange) override;
		PARABLIT_API void CmdBindPipeline(Pipeline pipeline) override;
		void CmdBindVertexBuffer(const IBufferObject* vertexBuffer, const IBufferObject* indexBuffer, EIndexType indexType) override;
		void CmdBindVertexBuffers(const IBufferObject** vertexBuffers, u32 vertexBufferCount, const IBufferObject* indexBuffer, EIndexType indexType) override;
		PARABLIT_API void CmdDraw(u32 vertexCount, u32 instanceCount) override;
		void CmdDrawIndexed(u32 indexCount, u32 instanceCount) override;
		PARABLIT_API void CmdCopyBufferToBuffer(IBufferObject* src, IBufferObject* dst, u32 srcOffset, u32 dstOffset, u32 size);
		void CmdBindResources(const BindingLayout& layout) override;
		void CmdCopyTextureToTexture(PB::ITexture* src, PB::ITexture* dst) override;

		PARABLIT_API bool GetIsPriority();
		
		/*
		Description: Flag this command context as internal.
		*/
		PARABLIT_API void SetIsInternal();

		PARABLIT_API bool GetIsInternal();

		PARABLIT_API ECmdContextState GetState();

		PARABLIT_API VkCommandBuffer GetCmdBuffer();

		PARABLIT_API void Invalidate();

		PARABLIT_API Renderer* GetRenderer();

	private:

		PARABLIT_API inline void ValidateRecordingState();

		static constexpr const u32 MaxPushConstantBytes = 128; // Vulkan spec requirement, all Vulkan compatible hardware will support 128 bytes for push constants.

		struct BindingState
		{
			CLib::Vector<VkDescriptorBufferInfo, 3> m_boundUBOs;
		};

		Renderer* m_renderer = nullptr;
		Device* m_device = nullptr;
		BindingState* m_bindingState = nullptr;
		VkCommandBuffer m_cmdBuffer = VK_NULL_HANDLE;
		VkPipelineLayout m_curPipelineLayout = VK_NULL_HANDLE;
		ECmdContextState m_state = ECmdContextState::OPEN;
		ECommandContextUsage m_usage = ECommandContextUsage::COPY; // Copy by default since any device queue is capable of copy operations.
		bool m_isPriority : 1;
		bool m_isInternal : 1;
		bool m_activeRenderpass : 1;
	};
}

