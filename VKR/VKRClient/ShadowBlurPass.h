#pragma once
#include "RenderGraphNode.h"
#include "Shader.h"

class RenderGraphBuilder;

class ShadowBlurPass : public RenderGraphBehaviour
{
public:

	ShadowBlurPass(PB::IRenderer* renderer, CLib::Allocator* allocator);

	~ShadowBlurPass();

	void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void AddToRenderGraph(RenderGraphBuilder* builder);

	void SetOutputTexture(PB::ITexture* tex);

private:

	static constexpr const uint32_t GaussianKernelSize = 16;
	struct BlurConstants
	{
		float m_depthDiscontinuityThreshold;
		float m_normalDiscontinuityThreshold;
		float m_depthScaleFactor;
		float m_guassianNormPart;
		PB::Float4 m_weights[GaussianKernelSize];
	};

	PB::ITexture* m_outputTexture = nullptr;
	PB::IBufferObject* m_blurConstants = nullptr;

	PB::ResourceView m_gBufferSampler = 0;
	PB::ResourceView m_blurImageSampler = 0;

	PB::ICommandList* m_reusableCmdList = nullptr;
};

