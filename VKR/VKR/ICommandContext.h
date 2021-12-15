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

	enum class EIndexType
	{
		PB_INDEX_TYPE_UINT16,
		PB_INDEX_TYPE_UINT32
	};

	class ICommandList
	{
	public:

		PARABLIT_API ICommandList() = default;

		PARABLIT_API ~ICommandList() = default;
	};

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

		PARABLIT_INTERFACE void CmdEndRenderPass() = 0;

		PARABLIT_INTERFACE void CmdClearColorTargets(ClearDesc* clearColors, u32 targetCount) = 0;

		PARABLIT_INTERFACE void CmdTransitionTexture(ITexture* texture, ETextureState oldState, ETextureState newState, const SubresourceRange& subResourceRange = {}) = 0;

		/*
		Description: Barrier which waits on all graphics work to complete.
		*/
		PARABLIT_INTERFACE void CmdGraphicsBarrier() = 0;

		/*
		Description: Barrier which waits on all compute work to complete.
		*/
		PARABLIT_INTERFACE void CmdComputeBarrier() = 0;

		PARABLIT_INTERFACE void CmdBindPipeline(Pipeline pipeline) = 0;

		PARABLIT_INTERFACE void SetViewport(PB::Rect viewRect, float minDepth, float maxDepth) = 0;

		PARABLIT_INTERFACE void SetScissor(PB::Rect scissorRect) = 0;

		PARABLIT_INTERFACE void CmdBindResources(const BindingLayout& layout) = 0;

		PARABLIT_INTERFACE void CmdBindVertexBuffer(const IBufferObject* vertexBuffer, const IBufferObject* indexBuffer, EIndexType indexType) = 0;

		PARABLIT_INTERFACE void CmdBindVertexBuffers(const IBufferObject** vertexBuffers, u32 vertexBufferCount, const IBufferObject* indexBuffer, EIndexType indexType) = 0;

		PARABLIT_INTERFACE void CmdDraw(u32 vertexCount, u32 instanceCount) = 0;

		PARABLIT_INTERFACE void CmdDrawIndexed(u32 indexCount, u32 instanceCount) = 0;

		PARABLIT_INTERFACE void CmdDrawIndexedIndirect(IBufferObject* paramsBuffer, u32 offset) = 0;

		PARABLIT_INTERFACE void CmdDispatch(u32 threadGroupX, u32 threadGroupY, u32 threadGroupZ) = 0;

		PARABLIT_INTERFACE void CmdCopyBufferToBuffer(IBufferObject* src, IBufferObject* dst, u32 srcOffset, u32 dstOffset, u32 size) = 0;

		PARABLIT_INTERFACE void CmdCopyTextureToTexture(ITexture* src, ITexture* dst) = 0;

		PARABLIT_INTERFACE void CmdExecuteList(const ICommandList* list) = 0;
	};

	ICommandContext* CreateCommandContext(IRenderer* renderer);

	void DestroyCommandContext(ICommandContext*& cmdContext);

	class SCommandContext
	{
	public:

		PARABLIT_API SCommandContext(IRenderer* renderer);

		PARABLIT_API ~SCommandContext();

		PARABLIT_API inline ICommandContext* operator -> ();

		inline ICommandContext* GetContext() { return m_ptr; }

	private:

		ICommandContext* m_ptr;
	};
}