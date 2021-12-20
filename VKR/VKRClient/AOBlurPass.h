#pragma once
#include "RenderGraphNode.h"
#include "BlurHelper.h"

class RenderGraphBuilder;

class AOBlurPass : public RenderGraphBehaviour
{
public:

	struct CreateDesc
	{
		bool m_halfRes = false;
	};

	AOBlurPass(PB::IRenderer* renderer, CLib::Allocator* allocator, const CreateDesc& desc);

	~AOBlurPass();

	void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void AddToRenderGraph(RenderGraphBuilder* builder);

private:

	static constexpr const uint32_t GaussianKernelSize = 4;
	struct BlurConstants
	{
		float m_guassianNormPart;
		float m_pad0[3];
		PB::Float4 m_weights[GaussianKernelSize];
	};

	PBClient::BlurHelper m_blurHelper;
	PB::ResourceView m_colorSampler = 0;
	PB::ResourceView m_blurImageSampler = 0;

	PB::ITexture* m_outputTexture = nullptr;
	PB::ICommandList* m_reusableCmdList = nullptr;

	bool m_halfRes = false;
};