#pragma once
#include "RenderGraphNode.h"
#include "BlurHelper.h"

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

	void OnPrePass(const RenderGraphInfo& info) override;

	void OnPassBegin(const RenderGraphInfo& info) override;

	void OnPostPass(const RenderGraphInfo& info) override;

	void AddToRenderGraph(RenderGraphBuilder* builder);

	void SetOutputTexture(PB::ITexture* tex);

private:

	static constexpr const uint32_t AOSampleKernelSize = 32;
	static constexpr const uint32_t RandomRotationTextureResolution = 32;

	static constexpr const uint32_t GaussianKernelSize = 8;
	struct BlurConstants
	{
		float m_guassianNormPart;
		float m_pad0[3];
		PB::Float4 m_weights[GaussianKernelSize];
	};

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

	PBClient::BlurHelper m_blurHelper;
	PB::IBufferObject* m_blurConstants = nullptr;
	PB::UniformBufferView m_mvpUBOView = nullptr;
	PB::ResourceView m_colorSampler = 0;
	PB::ResourceView m_blurImageSampler = 0;

	PB::IBufferObject* m_aoConstantsBuffer = nullptr;
	PB::UniformBufferView m_aoConstantsView = 0;

	// Texture containing random values for rotating the sample kernel.
	PB::ITexture* m_randomRotationTexture = nullptr;
	PB::ResourceView m_randomRotationTexView = 0;
	PB::ResourceView m_randomRotationSampler = 0;

	PB::ITexture* m_outputTexture = nullptr;
	PB::ICommandList* m_reusableCmdListA = nullptr;
	PB::ICommandList* m_reusableCmdListB = nullptr;

	bool m_halfRes = false;
};