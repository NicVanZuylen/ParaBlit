#include "TextRenderPass.h"

TextRenderPass::TextRenderPass(PB::IRenderer* renderer, CLib::Allocator* allocator) : RenderGraphBehaviour(renderer, allocator)
{
	m_renderer = renderer;
	m_allocator = allocator;
}

TextRenderPass::~TextRenderPass()
{
}

void TextRenderPass::OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
{

}

void TextRenderPass::OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
{

}

void TextRenderPass::OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
{

}

void TextRenderPass::AddToRenderGraph(RenderGraphBuilder* builder)
{
}

void TextRenderPass::SetOutputTexture(PB::ITexture* tex)
{
	m_outputTexture = tex;
}
