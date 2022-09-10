#pragma once
#include "RenderGraphNode.h"
#include "BlurHelper.h"

class RenderGraphBuilder;

class BloomBlurPass : public RenderGraphBehaviour
{
public:

	BloomBlurPass(PB::IRenderer* renderer, CLib::Allocator* allocator, bool halfRes = false);

	~BloomBlurPass();

	void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void AddToRenderGraph(RenderGraphBuilder* builder);

	void SetOutputTexture(PB::ITexture* tex);

private:

	static constexpr const uint32_t BlurTargetMipCount = 6;

	static constexpr const uint32_t GaussianKernelSize = 8;
	struct BlurConstants
	{
		float m_guassianNormPart;
		float m_pad0[3];
		PB::Float4 m_weights[GaussianKernelSize];
	};

	PBClient::BlurHelper m_blurHelper;
	PB::ResourceView m_blurImageSampler = 0;
	PB::ResourceView m_mergeSampler = 0;

	PB::ITexture* m_outputTexture = nullptr;
	PB::ICommandList* m_reusableCmdList = nullptr;

	bool m_halfRes = false;
};