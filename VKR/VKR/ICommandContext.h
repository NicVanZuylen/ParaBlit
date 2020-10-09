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

	class ICommandContext
	{
	public:

		PARABLIT_API ICommandContext();

		PARABLIT_API ~ICommandContext();

		PARABLIT_INTERFACE void Init(CommandContextDesc& desc) = 0;

		PARABLIT_INTERFACE void Begin() = 0;

		PARABLIT_INTERFACE void End() = 0;

		PARABLIT_INTERFACE void Return() = 0;

		/*
		Description: Begin a render pass using a render pass desc and provided attachments.
		Param:
			RenderPass renderPass: The render pass to use.
			u32 width: The width of the render pass framebuffer.
			u32 height: The height of the render pass framebuffer.
			const TextureView* attachmentViews: Array of texture views for the render pass attachments.
			const u32 viewCount: The amount of attachments to use in the render pass.
		*/
		PARABLIT_INTERFACE void CmdBeginRenderPass(RenderPass renderPass, u32 width, u32 height, TextureView* attachmentViews, u32 viewCount, Float4* clearColors, u32 clearColorCount) = 0;

		PARABLIT_INTERFACE void CmdBeginRenderPass(RenderPass renderPass, u32 width, u32 height, Framebuffer frameBuffer, Float4* clearColors, u32 clearColorCount) = 0;

		PARABLIT_INTERFACE void CmdEndRenderPass() = 0;

		PARABLIT_INTERFACE void CmdClearColorTargets(ClearDesc* clearColors, u32 targetCount) = 0;

		PARABLIT_INTERFACE void CmdTransitionTexture(ITexture* texture, ETextureState newState, const SubresourceRange& subResourceRange = {}) = 0;

		PARABLIT_INTERFACE void CmdBindPipeline(Pipeline pipeline) = 0;

		PARABLIT_INTERFACE void CmdBindResources(const BindingLayout& layout) = 0;

		PARABLIT_INTERFACE void CmdBindVertexBuffer(const IBufferObject* vertexBuffer, const IBufferObject* indexBuffer, EIndexType indexType) = 0;

		PARABLIT_INTERFACE void CmdBindVertexBuffers(const IBufferObject** vertexBuffers, u32 vertexBufferCount, const IBufferObject* indexBuffer, EIndexType indexType) = 0;

		PARABLIT_INTERFACE void CmdDraw(u32 vertexCount, u32 instanceCount) = 0;

		PARABLIT_INTERFACE void CmdDrawIndexed(u32 indexCount, u32 instanceCount) = 0;

		PARABLIT_INTERFACE void CmdCopyTextureToTexture(PB::ITexture* src, PB::ITexture* dst) = 0;
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