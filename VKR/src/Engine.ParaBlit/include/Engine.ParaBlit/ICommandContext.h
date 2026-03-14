#pragma once
#include "ParaBlitApi.h"
#include "ParaBlitInterface.h"
#include "ParaBlitDefs.h"
#include "IRenderPassCache.h"

namespace PB
{
	class IRenderer;
	class ITexture;
	class IBufferObject;
	class IBindingCache;
	class IAccelerationStructure;

	class ICommandList
	{
	public:

		PARABLIT_API ICommandList() = default;

		PARABLIT_API ~ICommandList() = default;
	};

	struct ClearDesc
	{
		Float4 m_color;
		Rect m_region;
		u32 m_attachmentIndex;
	};

	struct CommandContextDesc
	{
		IRenderer* m_renderer = nullptr;
		ECommandContextUsage m_usage = ECommandContextUsage::GRAPHICS;
		CommandContextFlags m_flags = ECommandContextFlags::NONE;
	};

	struct CopyRegion
	{
		u64 m_srcOffset;
		u64 m_dstOffset;
		u64 m_size;
	};

	enum class EIndexType
	{
		PB_INDEX_TYPE_UINT16,
		PB_INDEX_TYPE_UINT32
	};

	struct BufferMemoryBarrier
	{
		PARABLIT_API BufferMemoryBarrier() = default;
		PARABLIT_API BufferMemoryBarrier(const IBufferObject* buffer, EMemoryBarrierType type, u32 offset = 0, u32 size = ~u32(0));

		const class IBufferObject* m_buffer;
		EMemoryBarrierType m_barrierType;
		u32 m_offset;
		u32 m_size;
	};

	struct TextureDataDesc;

	class ICommandContext
	{
	public:

		PARABLIT_API ICommandContext();

		PARABLIT_API ~ICommandContext();

		PARABLIT_INTERFACE void Init(CommandContextDesc& desc) = 0;

		/*
		Description: Begin recording of commands using this command context.
		Param:
			PB::RenderPass renderPass: If recording a reusable command context, a render pass should be provided here for optimization.
			PB::Framebuffer framebuffer: If recording a reusable command context, a framebuffer can be provided here for optimization.
		*/
		PARABLIT_INTERFACE void Begin(PB::RenderPass renderPass = nullptr, PB::Framebuffer frameBuffer = nullptr) = 0;

		PARABLIT_INTERFACE void End() = 0;

		PARABLIT_INTERFACE ICommandList* Return() = 0;

		/*
		Description: Begin a render pass using a render pass desc and provided attachments.
		Param:
			RenderPass renderPass: The render pass to use.
			u32 width: The width of the render pass framebuffer.
			u32 height: The height of the render pass framebuffer.
			const RenderTargetView* attachmentViews: Array of texture views for the render pass attachments.
			const u32 viewCount: The amount of attachments to use in the render pass.
		*/
		PARABLIT_INTERFACE void CmdBeginRenderPass(RenderPass renderPass, u32 width, u32 height, RenderTargetView* attachmentViews, u32 viewCount, Float4* clearColors, u32 clearColorCount, bool useCommandLists = false) = 0;

		PARABLIT_INTERFACE void CmdBeginRenderPass(RenderPass renderPass, u32 width, u32 height, Framebuffer frameBuffer, Float4* clearColors, u32 clearColorCount, bool useCommandLists = false) = 0;

		PARABLIT_INTERFACE void CmdBeginRenderPassDynamic(RenderPass renderPass, u32 width, u32 height, RenderTargetView* attachmentViews, Float4* clearColors, bool useCommandLists = false) = 0;

		PARABLIT_INTERFACE void CmdEndRenderPass() = 0;

		PARABLIT_INTERFACE void CmdClearColorTargets(ClearDesc* clearColors, u32 targetCount) = 0;

		PARABLIT_INTERFACE void CmdTransitionTexture(ITexture* texture, ETextureState oldState, ETextureState newState, const SubresourceRange& subResourceRange = SubresourceRange::All()) = 0;

		PARABLIT_INTERFACE void CmdTextureBarrier(ITexture* texture, EMemoryBarrierType barrierType, const SubresourceRange& subResourceRange = SubresourceRange::All()) = 0;

		/*
		Description: Wait for all graphics work to complete before issuing compute work.
		*/
		PARABLIT_INTERFACE void CmdGraphicsToComputeBarrier() = 0;

		/*
		Description: Issue a barrier between compute dispatches of a specified type.
		Param:
			EMemoryBarrierType type: The type/nature of the memory barrier to issue.
		*/
		PARABLIT_INTERFACE void CmdComputeBarrier(EMemoryBarrierType type) = 0;

		PARABLIT_INTERFACE void CmdBufferBarrier(BufferMemoryBarrier* barriers, u32 barrierCount) = 0;

		PARABLIT_INTERFACE void CmdDrawIndirectBarrier(const PB::IBufferObject** drawParamBuffers, u32 drawParamBufferCount) = 0;

		PARABLIT_INTERFACE void CmdBuildAccelerationStructureToTraceRaysBarrier(const IAccelerationStructure** accStructures, u32 accStructureCount) = 0;

		PARABLIT_INTERFACE void CmdBindPipeline(Pipeline pipeline) = 0;

		PARABLIT_INTERFACE void CmdSetViewport(PB::Rect viewRect, float minDepth, float maxDepth) = 0;

		PARABLIT_INTERFACE void CmdSetScissor(PB::Rect scissorRect) = 0;

		PARABLIT_INTERFACE void CmdBindResources(const BindingLayout& layout) = 0;

		PARABLIT_INTERFACE void CmdBindResources(const IBindingCache* layout) = 0;

		PARABLIT_INTERFACE void CmdBindAccelerationStructure(const IAccelerationStructure* as) = 0;

		PARABLIT_INTERFACE void CmdBindVertexBuffer(const IBufferObject* vertexBuffer, const IBufferObject* indexBuffer, EIndexType indexType) = 0;

		PARABLIT_INTERFACE void CmdBindVertexBuffers(const IBufferObject** vertexBuffers, u32 vertexBufferCount, const IBufferObject* indexBuffer, EIndexType indexType) = 0;

		PARABLIT_INTERFACE void CmdDraw(u32 vertexCount, u32 instanceCount) = 0;

		PARABLIT_INTERFACE void CmdDrawIndexed(u32 indexCount, u32 instanceCount) = 0;

		PARABLIT_INTERFACE void CmdDrawIndexedIndirect(IBufferObject* paramsBuffer, u32 offset) = 0;

		PARABLIT_INTERFACE void CmdDrawIndexedIndirectCount(IBufferObject* paramsBuffer, u32 paramsOffset, IBufferObject* drawCountBuffer, u32 drawCountOffset, u32 maxDrawCount, u32 paramStride) = 0;

		PARABLIT_INTERFACE void CmdDrawMeshTasks(u32 threadGroupX, u32 threadGroupY, u32 threadGroupZ) = 0;

		PARABLIT_INTERFACE void CmdDrawMeshTasksIndirect(IBufferObject* paramsBuffer, u32 offset) = 0;

		PARABLIT_INTERFACE void CmdDrawMeshTasksIndirectCount(IBufferObject* paramsBuffer, u32 paramsOffset, IBufferObject* drawCountBuffer, u32 drawCountOffset, u32 maxDrawCount, u32 paramStride) = 0;

		PARABLIT_INTERFACE void CmdDispatch(u32 threadGroupX, u32 threadGroupY, u32 threadGroupZ) = 0;

		PARABLIT_INTERFACE void CmdTraceRays(u32 threadGroupX, u32 threadGroupY, u32 threadGroupZ) = 0;

		PARABLIT_INTERFACE void CmdCopyBufferToBuffer(const IBufferObject* src, IBufferObject* dst, u32 srcOffset, u32 dstOffset, u32 size) = 0;

		PARABLIT_INTERFACE void CmdCopyBufferToBuffer(const IBufferObject* src, IBufferObject* dst, const CopyRegion* copyRegions, u32 regionCount) = 0;

		PARABLIT_INTERFACE void CmdCopyTextureToTexture(ITexture* src, ITexture* dst) = 0;

		PARABLIT_INTERFACE void CmdCopyTextureSubresource(ITexture* src, ITexture* dst, u16 srcMipLevel = 0, u16 srcArrayLayer = 0, u16 dstMipLevel = 0, u16 dstArrayLayer = 0) = 0;

		/*
		Description: Copy selected subresources from a texture into a buffer.
		Param:
			ITexture* src: Source texture.
			IBufferObject* dst: Destination buffer.
			const SubresourceRange& subresources: Defines subresources to copy into dst buffer.
			TextureDataDesc* outSubresourceData: Optional - If non-null, is expected to be the amount of subresources to copy in size. Returns subresource offset in data pointer.
		*/
		PARABLIT_INTERFACE void CmdCopyTextureToBuffer(ITexture* src, PB::IBufferObject* dst, const PB::SubresourceRange& subresources, TextureDataDesc* outSubresourceData) = 0;

		PARABLIT_INTERFACE void CmdBuildAccelerationStructure(IAccelerationStructure* accelerationStructure, u32* primitiveCounts) = 0;

		PARABLIT_INTERFACE void CmdExecuteList(const ICommandList* list) = 0;

#ifdef PB_USE_DEBUG_UTILS
		PARABLIT_INTERFACE void CmdBeginLabel(const char* labelText, PB::Float4 color) = 0;
		PARABLIT_INTERFACE void CmdEndLastLabel() = 0;
#else
		PARABLIT_INTERFACE inline void CmdBeginLabel(const char* labelText, PB::Float4 color) {};
		PARABLIT_INTERFACE inline void CmdEndLastLabel() {};
#endif

	};

	PARABLIT_API ICommandContext* CreateCommandContext(IRenderer* renderer);

	PARABLIT_API void DestroyCommandContext(ICommandContext*& cmdContext);

	class SCommandContext
	{
	public:

		PARABLIT_API SCommandContext(IRenderer* renderer);

		PARABLIT_API ~SCommandContext();

		PARABLIT_API ICommandContext* operator -> ();

		inline ICommandContext* GetContext() { return m_ptr; }

	private:

		ICommandContext* m_ptr;
	};
}