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
#define PB_COMMAND_CONTEXT_LOG(...)
#endif

namespace PB 
{
	class Renderer;
	class IBufferObject;
	
	struct PipelineData;

	class CommandList : public ICommandList
	{
	public:

		CommandList(Renderer* renderer, VkCommandBuffer cmdBuffer);

		~CommandList() = default;

		CLib::Vector<VkDescriptorSet, 2>& GetDescriptorSets(EDescriptorSetType type) { return m_descriptorSets[u32(type)]; };

		inline VkCommandBuffer GetCommandBuffer() const { return m_cmdBuffer; }

	private:

		Renderer* m_renderer = nullptr;
		VkCommandBuffer m_cmdBuffer = VK_NULL_HANDLE;
		CLib::Vector<VkDescriptorSet, 2> m_descriptorSets[u32(EDescriptorSetType::DESCRIPTOR_TYPE_COUNT)];
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
		void CmdTextureBarrier(ITexture* texture, EMemoryBarrierType barrierType, const SubresourceRange& subResourceRange) override;
		void CmdGraphicsToComputeBarrier() override;
		void CmdComputeBarrier(EMemoryBarrierType type) override;
		void CmdBufferBarrier(BufferMemoryBarrier* barriers, u32 barrierCount) override;
		void CmdDrawIndirectBarrier(const PB::IBufferObject** drawParamBuffers, u32 drawParamBufferCount) override;
		void CmdBuildAccelerationStructureToTraceRaysBarrier(const IAccelerationStructure** accStructures, u32 accStructureCount) override;
		PARABLIT_API void CmdBindPipeline(Pipeline pipeline) override;
		void CmdSetViewport(PB::Rect viewRect, float minDepth, float maxDepth) override;
		void CmdSetScissor(PB::Rect scissorRect) override;
		void CmdBindVertexBuffer(const IBufferObject* vertexBuffer, const IBufferObject* indexBuffer, EIndexType indexType) override;
		void CmdBindVertexBuffers(const IBufferObject** vertexBuffers, u32 vertexBufferCount, const IBufferObject* indexBuffer, EIndexType indexType) override;
		PARABLIT_API void CmdDraw(u32 vertexCount, u32 instanceCount) override;
		void CmdDrawIndexed(u32 indexCount, u32 instanceCount) override;
		void CmdDrawIndexedIndirect(IBufferObject* paramsBuffer, u32 offset) override;
		void CmdDrawIndexedIndirectCount(IBufferObject* paramsBuffer, u32 paramsOffset, IBufferObject* drawCountBuffer, u32 drawCountOffset, u32 maxDrawCount, u32 paramStride) override;
		void CmdDrawMeshTasks(u32 threadGroupX, u32 threadGroupY, u32 threadGroupZ) override;
		void CmdDrawMeshTasksIndirect(IBufferObject* paramsBuffer, u32 offset) override;
		void CmdDrawMeshTasksIndirectCount(IBufferObject* paramsBuffer, u32 paramsOffset, IBufferObject* drawCountBuffer, u32 drawCountOffset, u32 maxDrawCount, u32 paramStride) override;
		void CmdDispatch(u32 threadGroupX, u32 threadGroupY, u32 threadGroupZ) override;
		void CmdTraceRays(u32 threadGroupX, u32 threadGroupY, u32 threadGroupZ) override;
		void CmdCopyBufferToBuffer(const IBufferObject* src, IBufferObject* dst, u32 srcOffset, u32 dstOffset, u32 size) override;
		void CmdCopyBufferToBuffer(const IBufferObject* src, IBufferObject* dst, const CopyRegion* copyRegions, u32 regionCount) override;
		void CmdBindResources(const BindingLayout& layout) override;
		void CmdBindResources(const IBindingCache* layout) override;
		void CmdBindAccelerationStructure(const IAccelerationStructure* as) override;
		void CmdCopyTextureToTexture(ITexture* src, ITexture* dst) override;
		void CmdCopyTextureSubresource(ITexture* src, ITexture* dst, u16 srcMipLevel, u16 srcArrayLayer, u16 dstMipLevel, u16 dstArrayLayer) override;
		void CmdCopyTextureToBuffer(ITexture* src, PB::IBufferObject* dst, const PB::SubresourceRange& subresources, TextureDataDesc* outSubresourceData) override;
		void CmdBuildAccelerationStructure(IAccelerationStructure* accelerationStructure, u32* primitiveCounts) override;
		void CmdExecuteList(const ICommandList* list) override;

#ifdef PB_USE_DEBUG_UTILS
		void CmdBeginLabel(const char* labelText, PB::Float4 color) override;
		void CmdEndLastLabel() override;
#endif

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

		friend class ThreadCommandContext;

		static PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHRFunc;
		static PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHRFunc;
		static PFN_vkCmdDrawMeshTasksEXT vkCmdDrawMeshTasksEXTFunc;
		static PFN_vkCmdDrawMeshTasksIndirectEXT vkCmdDrawMeshTasksIndirectEXTFunc;
		static PFN_vkCmdDrawMeshTasksIndirectCountEXT vkCmdDrawMeshTasksIndirectCountEXTFunc;
		static PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXTFunc;
		static PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXTFunc;
		static PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHRFunc;
		static PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHRFunc;

		void Pause();
		void Resume();

		inline void ValidateRecordingState();

		inline void ValidatePipelineState(bool computeFunction, bool meshFunction, bool rtFunction);

		static constexpr const u32 MaxPushConstantBytes = 128; // Vulkan spec requirement, all Vulkan compatible hardware will support 128 bytes for push constants.

		struct BindingState
		{
			void Clear()
			{
				m_boundAccelerationStructure = VK_NULL_HANDLE;
				m_boundUBOs.Clear();
			}

			void ClearDescriptorSets()
			{
				for (u32 i = 0; i < u32(EDescriptorSetType::DESCRIPTOR_TYPE_COUNT); ++i)
				{
					m_descriptorSets[i].Clear();
				}
			}

			VkAccelerationStructureKHR m_boundAccelerationStructure = VK_NULL_HANDLE;
			CLib::Vector<VkDescriptorBufferInfo, 3> m_boundUBOs;
			CLib::Vector<VkDescriptorSet, 16> m_descriptorSets[u32(EDescriptorSetType::DESCRIPTOR_TYPE_COUNT)];
		};

		Renderer* m_renderer = nullptr;
		Device* m_device = nullptr;
		BindingState* m_bindingState = nullptr;
		VkCommandBuffer m_cmdBuffer = VK_NULL_HANDLE;
		PipelineData* m_boundPipeline = nullptr;
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
				bool m_activePipelineHasMeshShader : 1;
				bool m_activePipelineIsRaytracing : 1;
				bool m_reusable : 1;
				bool m_reusableForRenderPass : 1;
			};
			u16 m_flags = false;
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

		CommandContext* Get() { return m_ptr; };
		CommandContext* Get(Renderer* renderer);

		bool IsRecording()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_ptr->GetState() == PB::ECmdContextState::RECORDING;
		}

		void End()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			PB_ASSERT_MSG(m_ptr->GetState() != ECmdContextState::RECORDING, "Command context is still recording. Cannot submit it now.");

			if (m_ptr->GetState() != PB::ECmdContextState::RECORDING)
			{
				m_ptr->End();
				m_ptr->Return();
			}
		}

		void ExplicitDestroy()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_ptr != nullptr)
			{
				if (m_ptr->GetState() == PB::ECmdContextState::RECORDING || m_ptr->GetState() == PB::ECmdContextState::PAUSED)
				{
					m_ptr->End();
					m_ptr->Return();
				}

				DestroyCommandContext(reinterpret_cast<ICommandContext*&>(m_ptr));
				m_ptr = nullptr;
			}
		}

		void Pause()
		{
			m_ptr->Pause();
		}

	private:

		Renderer* m_renderer = nullptr;
		CommandContext* m_ptr = nullptr;
		std::mutex m_mutex;
	};
}

