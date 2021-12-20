#pragma once
#include "RenderGraphNode.h"
#include "Shader.h"

class RenderGraphBuilder;

class ShadowAccumPass : public RenderGraphBehaviour
{
public:

	ShadowAccumPass(PB::IRenderer* renderer, CLib::Allocator* allocator);

	~ShadowAccumPass();

	void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void AddToRenderGraph(RenderGraphBuilder* builder, uint32_t shadowmapResolution);

	void SetOutputTexture(PB::ITexture* tex);

	void SetMVPBuffer(PB::IBufferObject* buf);

	void SetSVBBuffer(PB::UniformBufferView view);

private:

	//static constexpr const uint32_t PCFDiskSampleCount = 16;
	static constexpr const uint32_t PCFRandomRotationTextureSize = 64;

	static constexpr const uint32_t GaussianKernelSize = 16;
	struct BlurConstants
	{
		float m_depthDiscontinuityThreshold;
		float m_normalDiscontinuityThreshold;
		float m_depthScaleFactor;
		float m_guassianNormPart;
		PB::Float4 m_weights[GaussianKernelSize];
	};

	void GenerateRandomRotationTexture(PB::Float2* pixelValues);

	PB::ITexture* m_outputTexture = nullptr;
	PB::IBufferObject* m_mvpBuffer = nullptr;
	PB::UniformBufferView m_svbView = nullptr;

	PB::ITexture* m_randomRotationTexture = nullptr;
	PB::ResourceView m_gBufferSampler = 0;
	PB::ResourceView m_shadowSampler = 0;
	PB::ResourceView m_randomRotationSampler = 0;

	PB::ICommandList* m_reusableCmdList = nullptr;
};

