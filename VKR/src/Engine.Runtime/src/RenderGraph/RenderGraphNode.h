#pragma once
#include "Engine.ParaBlit/IRenderer.h"
#include "Engine.ParaBlit/ICommandContext.h"
#include "CLib/Vector.h"
#include "CLib/Allocator.h"

namespace CLib
{
	class Allocator;
};

namespace Eng
{
	struct RenderGraphInfo
	{
		PB::IRenderer* m_renderer = nullptr;
		PB::ICommandContext* m_commandContext = nullptr;
		CLib::Allocator* m_allocator = nullptr;

		PB::RenderPass m_renderPass = nullptr;
		PB::Framebuffer m_frameBuffer = nullptr;
		uint32_t m_renderTargetCount = 0;
	};

	class RenderGraphBehaviour
	{
	public:

		RenderGraphBehaviour(PB::IRenderer* renderer, CLib::Allocator* allocator);

		virtual void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) = 0;

		virtual void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) = 0;

		virtual void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) = 0;

		PB::RenderPass GetRenderPass() { return m_renderPass; };

	protected:

		friend class RenderGraphBuilder;

		PB::IRenderer* m_renderer = nullptr;
		PB::RenderPass m_renderPass = nullptr;
		CLib::Allocator* m_allocator = nullptr;
	};

};