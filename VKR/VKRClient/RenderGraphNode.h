#pragma once
#include "IRenderer.h"
#include "ICommandContext.h"
#include "CLib/Vector.h"
#include "CLib/Allocator.h"

class CLib::Allocator;

struct RenderGraphInfo
{
	PB::IRenderer* m_renderer = nullptr;
	PB::ICommandContext* m_commandContext = nullptr;
	CLib::Allocator* m_allocator = nullptr;

	PB::RenderPass m_renderPass = 0;
	PB::ITexture** m_renderTargets = nullptr;
	PB::TextureView* m_renderTargetViews = nullptr;
	uint32_t m_renderTargetCount = 0;
};

class RenderGraphBehaviour
{
public:

	RenderGraphBehaviour(PB::IRenderer* renderer, CLib::Allocator* allocator);

	virtual void OnPreRenderPass(const RenderGraphInfo& info) = 0;

	virtual void OnPassBegin(const RenderGraphInfo& info) = 0;

	virtual void OnPostRenderPass(const RenderGraphInfo& info) = 0;

protected:

	PB::IRenderer* m_renderer = nullptr;
	CLib::Allocator* m_allocator = nullptr;
};