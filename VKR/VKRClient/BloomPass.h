#pragma once
#include "RenderGraphNode.h"
#include "BlurHelper.h"

class RenderGraphBuilder;

class BloomPass : public RenderGraphBehaviour
{
public:

	BloomPass(PB::IRenderer* renderer, CLib::Allocator* allocator, bool halfRes = false);

	~BloomPass();

	void OnPrePass(const RenderGraphInfo& info) override;

	void OnPassBegin(const RenderGraphInfo& info) override;

	void OnPostPass(const RenderGraphInfo& info) override;

	void AddToRenderGraph(RenderGraphBuilder* builder);

	void SetOutputTexture(PB::ITexture* tex);

private:

	static constexpr const uint32_t BlurTargetMipCount = 5;

	struct BloomConstants
	{
		PB::Float3 m_rgbChannelWeights;
		float m_minBrightnessThreshold;
	};

	static constexpr const uint32_t GaussianKernelSize = 8;
	struct BlurConstants
	{
		float m_guassianNormPart;
		float m_pad0[3];
		PB::Float4 m_weights[GaussianKernelSize];
	};

	PBClient::BlurHelper m_blurHelper;
	PB::IBufferObject* m_bloomConstantsBuffer = nullptr;
	PB::IBufferObject* m_blurConstants = nullptr;
	PB::ResourceView m_colorSampler = 0;
	PB::ResourceView m_blurImageSampler = 0;
	PB::ResourceView m_mergeSampler = 0;

	PB::ITexture* m_outputTexture = nullptr;
	PB::ICommandList* m_reusableCmdListA = nullptr;
	PB::ICommandList* m_reusableCmdListB = nullptr;

	bool m_halfRes = false;
};