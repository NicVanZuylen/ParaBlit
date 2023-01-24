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
	
	class CommandList : public ICommandList
	{
	public:

		CommandList(Renderer* renderer, VkCommandBuffer cmdBuffer);

		~CommandList() = default;

		CLib::Vector<VkDescriptorSet, 2>& GetUBOSets() { return m_uboSets; };

		inline VkCommandBuffer GetCommandBuffer() const { return m_cmdBuffer; }

	private:

		Renderer* m_renderer = nullptr;
		VkCommandBuffer m_cmdBuffer = VK_NULL_HANDLE;
		CLib::Vector<VkDescriptorSet, 2> m_uboSets;
	};

	class CommandContext : public ICommandContext
	{
	public:

		static void GetExtensionFunctions(VkInstance instance);

		PARABLIT_API CommandContext();

		PARABLIT_API ~CommandContext();

		// Interface functions. Descriptions can be found in ICommandContext.h

		PARABLIT_API void Init(CommandContextDesc& desc) override;
		PARABLIT_API void Begin(PB::RenderPass renderPass = nullptr, PB::Framebuffer frameBuffer = nullptr) override;
		PARABLIT_API void End() override;
		PARABLIT_API ICommandList* Return() override;
		void CmdBeginRenderPass(RenderPass renderPass, u32 width, u32 height, RenderTargetView* attachmentViews, u32 viewCount, Float4* clearColors, u32 clearColorCount, bool useCommandLists = false) override;
		void CmdBeginRenderPass(RenderPass renderPass, u32 width, u32 height, Framebuffer frameBuffer, Float4* clearColors, u32 clearColorCount, bool useCommandLists = false) override;
		void CmdBeginRenderPassDynamic(RenderPass renderPass, u32 width, u32 height, RenderTargetView* attachmentViews, Float4* clearColors, bool useCommandLists = false) override;
		PARABLIT_API void CmdEndRenderPass() override;
		PARABLIT_API void CmdClearColorTargets(ClearDesc* clearColors, u32 targetCount) override;
		PARABLIT_API void CmdTransitionTexture(ITexture* texture, ETextureState oldState, ETextureState newState, const SubresourceRange& subResourceRange) override;
		void CmdGraphicsBarrier() override;
		void CmdComputeBarrier() override;
		void CmdDrawIndirectBarrier(const PB::IBufferObject** drawParamBuffers, u32 drawParamBufferCount) override;
		PARABLIT_API void CmdBindPipeline(Pipeline pipeline) override;
		void CmdSetViewport(PB::Rect viewRect, float minDepth, float maxDepth) override;
		void CmdSetScissor(PB::Rect scissorRect) override;
		void CmdBindVertexBuffer(const IBufferObject* vertexBuffer, const IBufferObject* indexBuffer, EIndexType indexType) override;
		void CmdBindVertexBuffers(const IBufferObject** vertexBuffers, u32 vertexBufferCount, const IBufferObject* indexBuffer, EIndexType indexType) override;
		PARABLIT_API void CmdDraw(u32 vertexCount, u32 instanceCount) override;
		void CmdDrawIndexed(u32 indexCount, u32 instanceCount) override;
		void CmdDrawIndexedIndirect(IBufferObject* paramsBuffer, u32 offset) override;
		void CmdDrawIndexedIndirectCount(IBufferObject* paramsBuffer, u32 paramsOffset, IBufferObject* drawCountBuffer, u32 drawCountOffset, u32 maxDrawCount, u32 paramStride) override;
		void CmdDispatch(u32 threadGroupX, u32 threadGroupY, u32 threadGroupZ) override;
		void CmdCopyBufferToBuffer(IBufferObject* src, IBufferObject* dst, u32 srcOffset, u32 dstOffset, u32 size) override;
		void CmdCopyBufferToBuffer(IBufferObject* src, IBufferObject* dst, const CopyRegion* copyRegions, u32 regionCount) override;
		void CmdBindResources(const BindingLayout& layout) override;
		void CmdBindResources(const IBindingCache* layout) override;
		void CmdCopyTextureToTexture(ITexture* src, ITexture* dst) override;
		void CmdCopyTextureSubresource(ITexture* src, ITexture* dst, u16 srcMipLevel, u16 srcArrayLayer, u16 dstMipLevel, u16 dstArrayLayer) override;
		void CmdCopyTextureToBuffer(ITexture* src, PB::IBufferObject* dst, const PB::SubresourceRange& subresources, TextureDataDesc* outSubresourceData) override;
		void CmdExecuteList(const ICommandList* list) override;

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

		static PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHRFunc;
		static PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHRFunc;

		inline void ValidateRecordingState();

		inline void ValidatePipelineState(bool computeFunction);

		static constexpr const u32 MaxPushConstantBytes = 128; // Vulkan spec requirement, all Vulkan compatible hardware will support 128 bytes for push constants.

		struct BindingState
		{
			void Clear()
			{
				m_boundUBOs.Clear();
			}

			void ClearUBOSets()
			{
				m_uboSets.Clear();
			}

			CLib::Vector<VkDescriptorBufferInfo, 3> m_boundUBOs;
			CLib::Vector<VkDescriptorSet, 16> m_uboSets;
		};

		Renderer* m_renderer = nullptr;
		Device* m_device = nullptr;
		BindingState* m_bindingState = nullptr;
		VkCommandBuffer m_cmdBuffer = VK_NULL_HANDLE;
		VkPipelineLayout m_curPipelineLayout = VK_NULL_HANDLE;
		ECmdContextState m_state = ECmdContextState::OPEN;
		ECommandContextUsage m_usage = ECommandContextUsage::COPY; // Copy by default since any device queue is capable of copy operations.
		union
		{
			struct
			{
				bool m_isPriority : 1;
				bool m_isInternal : 1;
				bool m_activeRenderpass : 1;
				bool m_activeRenderPassIsDynamic : 1;
				bool m_activePipelineIsCompute : 1;
				bool m_reusable : 1;
				bool m_reusableForRenderPass : 1;
			};
			u8 m_flags = false;
		};
	};

	// Command context wrapper used for thread_local storage duration.
	class ThreadCommandContext
	{
	public:

		ThreadCommandContext() = default;
		~ThreadCommandContext()
		{
			ExplicitDestroy();
		}

		CommandContext* Get(Renderer* renderer)
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
			}

			return m_ptr;
		}

		void End()
		{
			if (m_ptr->GetState() == PB::ECmdContextState::RECORDING)
			{
				m_ptr->End();
				m_ptr->Return();
			}
		}

		void ExplicitDestroy()
		{
			if (m_ptr != nullptr)
			{
				if (m_ptr->GetState() == PB::ECmdContextState::RECORDING)
				{
					m_ptr->End();
					m_ptr->Return();
				}

				DestroyCommandContext(reinterpret_cast<ICommandContext*&>(m_ptr));
				m_ptr = nullptr;
			}
		}

	private:

		Renderer* m_renderer = nullptr;
		CommandContext* m_ptr;
	};
}

