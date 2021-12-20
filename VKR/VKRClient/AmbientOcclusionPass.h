#pragma once
#include "RenderGraphNode.h"

class RenderGraphBuilder;

class AmbientOcclusionPass : public RenderGraphBehaviour
{
public:

	struct CreateDesc
	{
		PB::UniformBufferView m_mvpUBOView = nullptr;
		bool m_halfRes = false;
	};

	AmbientOcclusionPass(PB::IRenderer* renderer, CLib::Allocator* allocator, const CreateDesc& desc);

	~AmbientOcclusionPass();

	void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void AddToRenderGraph(RenderGraphBuilder* builder);

	void SetOutputTexture(PB::ITexture* tex);

private:

	static constexpr const uint32_t AOSampleKernelSize = 32;
	static constexpr const uint32_t RandomRotationTextureResolution = 32;
	struct AOConstants
	{
		float m_sampleRadius;
		float m_depthBias;
		float m_depthSlopeBias;
		float m_depthSlopeThreshold;
		float m_intensity;
		PB::u32 m_renderWidth;
		PB::u32 m_renderHeight;
		float m_pad;
		PB::Float4 m_samples[AOSampleKernelSize];
	};

	void GenerateRandomSamples(void* pixelValues);

	void GenerateRandomRotationTexture(PB::Float2* pixelValues);

	PB::UniformBufferView m_mvpUBOView = nullptr;
	PB::ResourceView m_colorSampler = 0;

	PB::IBufferObject* m_aoConstantsBuffer = nullptr;
	PB::UniformBufferView m_aoConstantsView = 0;

	// Texture containing random values for rotating the sample kernel.
	PB::ITexture* m_randomRotationTexture = nullptr;
	PB::ResourceView m_randomRotationTexView = 0;
	PB::ResourceView m_randomRotationSampler = 0;

	PB::ITexture* m_outputTexture = nullptr;
	PB::ICommandList* m_reusableCmdList = nullptr;

	bool m_halfRes = false;
};