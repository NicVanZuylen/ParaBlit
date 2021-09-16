#pragma once
#include "RenderGraphNode.h"

#include "Shader.h"

class RenderGraphBuilder;

class TextRenderPass : public RenderGraphBehaviour
{
public:

	TextRenderPass(PB::IRenderer* renderer, CLib::Allocator* allocator);

	~TextRenderPass();

	void OnPrePass(const RenderGraphInfo& info) override;

	void OnPassBegin(const RenderGraphInfo& info) override;

	void OnPostPass(const RenderGraphInfo& info) override;

	void AddToRenderGraph(RenderGraphBuilder* builder);

	void SetOutputTexture(PB::ITexture* tex);

private:

	PB::ITexture* m_outputTexture = nullptr;
};